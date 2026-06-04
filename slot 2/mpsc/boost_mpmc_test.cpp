#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <pthread.h>
#include <hdr/hdr_histogram.h>
#include <boost/lockfree/queue.hpp>

using namespace std;

struct UDT
{
    long long id;
    long long sequence;
};

// Global / Shared configuration
std::atomic<bool> go{false};
double tsc_freq_hz;

static constexpr int NUM_PRODUCERS = 20;
static constexpr int64_t ITERS_PER_PROD = 1000000;
static constexpr int64_t TOTAL_EXPECTED_ITEMS = NUM_PRODUCERS * ITERS_PER_PROD;

using BoostMPSCQueue = boost::lockfree::queue<UDT>;

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

    // Sleep ~100ms — long enough for accuracy, short enough for startup
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

// --- Pure Worker Functions ---
void producer_worker(long long id, BoostMPSCQueue &q, hdr_histogram *hist, int core_id)
{
    pin_thread_to_cpu_core(id % 4); // Pin all producers to core 0 (or map to i)

    while (!go.load(std::memory_order_relaxed))
    {
        std::this_thread::yield();
    }

    for (long long i = 0; i < ITERS_PER_PROD; i++)
    {
        UDT val{id, i};

        asm volatile("lfence" ::: "memory");
        uint64_t start = rdtsc();
        asm volatile("" ::: "memory");

        while (!q.push(val))
        {
            std::this_thread::yield();
        }

        asm volatile("lfence" ::: "memory");
        uint64_t end = rdtsc();

        double elapsed_ns = double(end - start) / tsc_freq_hz;
        hdr_record_value(hist, static_cast<int64_t>(elapsed_ns));
    }
}

void consumer_worker(BoostMPSCQueue &q, hdr_histogram *hist)
{
    pin_thread_to_cpu_core(5);

    while (!go.load(std::memory_order_relaxed))
    {
        std::this_thread::yield();
    }

    long long count = 0;
    while (count < TOTAL_EXPECTED_ITEMS)
    {
        UDT val;

        asm volatile("lfence" ::: "memory");
        uint64_t start = rdtsc();
        asm volatile("" ::: "memory");

        if (q.pop(val))
        {
            asm volatile("lfence" ::: "memory");
            uint64_t end = rdtsc();

            count++;
            double elapsed_ns = double(end - start) / tsc_freq_hz;
            hdr_record_value(hist, static_cast<int64_t>(elapsed_ns));
        }
        else
        {
            std::this_thread::yield();
        }
    }
    cout << "Total Consumer Received Count = " << count << "\n";
}

// --- Clean Execution Entrypoint ---
int main()
{
    // Setup timing (Assuming external calibration function is present as before)
    // pin_thread_to_cpu_core(5);
    pin_thread_to_cpu_core(4);
    tsc_freq_hz = calibrate_tsc_freq_ghz();

    BoostMPSCQueue q(1024);

    // 1. Allocate all Histograms safely on the main stack
    std::vector<hdr_histogram *> prod_hists(NUM_PRODUCERS, nullptr);
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        hdr_init(1, 1000000000, 3, &prod_hists[i]);
    }

    hdr_histogram *cons_hist = nullptr;
    hdr_init(1, 1000000000, 3, &cons_hist);
    uint64_t start;
    // 2. Launch threads directly using std::jthread (RAII auto-joining)
    {
        std::vector<std::jthread> producer_threads;
        producer_threads.reserve(NUM_PRODUCERS);

        // Spawn Consumer
        std::jthread consumer_thread(consumer_worker, std::ref(q), cons_hist);

        // Spawn Producers
        for (int i = 0; i < NUM_PRODUCERS; i++)
        {
            producer_threads.emplace_back(producer_worker, i * 1000000LL, std::ref(q), prod_hists[i], i);
        }

        // Signal everyone to start
        go.store(true, std::memory_order_relaxed);
        asm volatile("lfence" ::: "memory");
        start = rdtsc();
        asm volatile("" ::: "memory");
        // When this block closes, ALL jthreads implicitly block and .join()
        // until the work is 100% completed.
    }
    asm volatile("lfence" ::: "memory");
    uint64_t end = rdtsc();
    uint64_t cycle = (end - start);
    double time = double(cycle) / tsc_freq_hz;
    // 3. Threads are completely dead here. Safe to print and cleanup.
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        string label = "Producer ID " + to_string(i);
        print_histogram_stats(label.c_str(), prod_hists[i]);
        hdr_close(prod_hists[i]);
    }

    print_histogram_stats("Final Consumer", cons_hist);
    hdr_close(cons_hist);
    cout << "total time taken = " << time << " ns \n";
    return 0;
}