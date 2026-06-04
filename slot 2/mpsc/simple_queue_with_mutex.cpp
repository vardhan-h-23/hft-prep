#include <hdr/hdr_histogram.h>

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <mutex>
#include <pthread.h>
#include <queue>
#include <sched.h>
#include <string>
#include <thread>
#include <vector>

using namespace std;

static constexpr int NUM_PRODUCERS = 20;
static constexpr int64_t ITERS_PER_PROD = 1'000'000;
static constexpr int64_t TOTAL_EXPECTED_ITEMS = NUM_PRODUCERS * ITERS_PER_PROD;

// Global / Shared configuration
std::atomic<bool> go{false};
double tsc_freq_hz;

struct UDT
{
    long long id;
    long long sequence;
};

std::queue<UDT> q;
std::mutex q_mutex;

// --- Hardware Timing Utilities ---
inline int64_t clock_ns()
{
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    return static_cast<int64_t>(t0.tv_sec) * 1000000000LL + t0.tv_nsec;
}

inline uint64_t rdtsc()
{
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

inline uint64_t rdtscp()
{
    uint32_t lo, hi, aux;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux)::"memory");
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

void pin_thread_to_cpu_core(int core_id)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

double calibrate_tsc_freq_ghz()
{
    // Warm up
    rdtscp();
    clock_ns();

    int64_t ns_start = clock_ns();
    uint64_t tsc_start = rdtscp();

    struct timespec req = {0, 100'000'000}; // 100ms
    nanosleep(&req, nullptr);

    uint64_t tsc_end = rdtscp();
    int64_t ns_end = clock_ns();

    double tsc_delta = static_cast<double>(tsc_end - tsc_start);
    double ns_delta = static_cast<double>(ns_end - ns_start);

    // ticks per nanosecond = GHz
    return tsc_delta / ns_delta;
}

// --- Print Helper ---
void print_histogram_stats(const char *label, hdr_histogram *hist)
{
    printf("─────────────────────────────────\n");
    printf("  Stats for %s\n", label);
    printf("─────────────────────────────────\n");
    if (!hist)
        return;
    cout << "p50 latency   = " << hdr_value_at_percentile(hist, 50.0) << " ns\n";
    cout << "p99 latency   = " << hdr_value_at_percentile(hist, 99.0) << " ns\n";
    cout << "p99.9 latency = " << hdr_value_at_percentile(hist, 99.9) << " ns\n";
    cout << "Max latency   = " << hdr_max(hist) << " ns\n";
}

// --- Worker Functions ---
void producer_worker(int id, hdr_histogram *hist)
{
    pin_thread_to_cpu_core(id % 4);

    while (!go.load(std::memory_order_relaxed))
    {
        std::this_thread::yield();
    }

    for (int64_t i = 0; i < ITERS_PER_PROD; i++)
    {
        UDT val{static_cast<long long>(id), i};

        asm volatile("lfence" ::: "memory");
        uint64_t start = rdtsc();
        asm volatile("" ::: "memory");

        {
            std::lock_guard<std::mutex> lock(q_mutex);
            q.push(val);
        }

        asm volatile("lfence" ::: "memory");
        uint64_t end = rdtsc();

        double elapsed_ns = double(end - start) / tsc_freq_hz;
        hdr_record_value(hist, static_cast<int64_t>(elapsed_ns));
    }
}

void consumer_worker(hdr_histogram *hist)
{
    pin_thread_to_cpu_core(5);
    while (!go.load(std::memory_order_relaxed))
    {
        std::this_thread::yield();
    }
    int64_t count = 0;
    while (count < TOTAL_EXPECTED_ITEMS)
    {
        UDT val;
        bool success = false;

        asm volatile("lfence" ::: "memory");
        uint64_t start = rdtsc();
        asm volatile("" ::: "memory");

        {
            std::lock_guard<std::mutex> lock(q_mutex);
            if (!q.empty())
            {
                val = q.front();
                q.pop();
                success = true;
            }
        }

        asm volatile("lfence" ::: "memory");
        uint64_t end = rdtsc();

        if (success)
        {
            double elapsed_ns = double(end - start) / tsc_freq_hz;
            hdr_record_value(hist, static_cast<int64_t>(elapsed_ns));
            count++;
        }
        else
        {
            std::this_thread::yield();
        }
    }
    cout << "Total Consumer Received Count = " << count << "\n";
}

int main()
{
    pin_thread_to_cpu_core(4);
    // Calibrate TSC
    tsc_freq_hz = calibrate_tsc_freq_ghz();

    // 1. Allocate per-producer histograms + one consumer histogram
    std::vector<hdr_histogram *> prod_hists(NUM_PRODUCERS, nullptr);
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        hdr_init(1, 1000000000, 3, &prod_hists[i]);
    }

    hdr_histogram *cons_hist = nullptr;
    hdr_init(1, 1000000000, 3, &cons_hist);

    // 2. Launch consumer + producers
    std::thread consumer_thread(consumer_worker, cons_hist);

    std::vector<std::thread> producer_threads;
    producer_threads.reserve(NUM_PRODUCERS);
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        producer_threads.emplace_back(producer_worker, i, prod_hists[i]);
    }
    go.store(true, std::memory_order_relaxed);
    asm volatile("lfence" ::: "memory");
    uint64_t start = rdtsc();
    asm volatile("" ::: "memory");

    for (auto &t : producer_threads)
        t.join();
    consumer_thread.join();

    asm volatile("lfence" ::: "memory");
    uint64_t end = rdtsc();
    uint64_t cycle = (end - start);
    double time = double(cycle) / tsc_freq_hz;

    // 3. Per-producer enqueue stats
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        string label = "Producer ID " + to_string(i);
        print_histogram_stats(label.c_str(), prod_hists[i]);
        hdr_close(prod_hists[i]);
    }

    // 4. Consumer dequeue stats
    print_histogram_stats("Final Consumer", cons_hist);
    hdr_close(cons_hist);
    cout << "total time taken = " << time << " ns \n";
    return 0;
}