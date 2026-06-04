#include "mpsc_queue.hpp"
#include <thread>
#include <vector>
#include <pthread.h>
#include <hdr/hdr_histogram.h>
//[] long long ll;
std::atomic<bool> go{false};

double tsc_freq_hz;

using namespace std;

static constexpr int64_t WARMUP_ITERS = 10000;
static constexpr int64_t MEASURE_ITERS = 1000000;

static constexpr int64_t HDR_MIN_NS = 1;
static constexpr int64_t HDR_MAX_NS = 1000000000;
static constexpr int HDR_SIG_FIGS = 3;

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
    pthread_setaffinity_np(pthread_self(),
                           sizeof(cpu_set_t),
                           &cpuset);
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

struct producer
{
    std::thread t1;
    ll id;
    hdr_histogram *hist1 = nullptr;
    producer(ll id_, MPSCQueue<UDT> &q) : id(id_)
    {
        int rc1 = hdr_init(HDR_MIN_NS, HDR_MAX_NS, HDR_SIG_FIGS, &hist1);
        if (rc1 != 0 || hist1 == nullptr)
        {
            cerr << "Failed to initiate the hdr\n";
        }
        t1 = std::thread(&producer::run, this, std::ref(q));
    }
    ~producer()
    {
        t1.join();
        print_stats();
        // hdr_close(hist1);
    }
    void run(MPSCQueue<UDT> &q)
    {
        pin_thread_to_cpu_core(id % 4);
        while (!go.load(std::memory_order_relaxed))
        {
            std::this_thread::yield();
        }
        for (ll i = 0; i < 1000000; i++)
        {
            UDT a{id, i};
            asm volatile("lfence" ::: "memory");
            uint64_t start = rdtsc();
            asm volatile("" ::: "memory");
            q.enqueue(a);
            asm volatile("lfence" ::: "memory");
            uint64_t end = rdtsc();
            // uint64_t end2 = clock_ns();
            uint64_t cycle = (end - start);
            double time = double(cycle) / tsc_freq_hz;
            hdr_record_value(hist1, time);
        }
    }
    void print_stats()
    {
        printf("─────────────────────────────────\n");
        printf("  Iterations : for producer\n");
        printf("─────────────────────────────────\n");
        cout << "p99.9 latency = " << hdr_value_at_percentile(hist1, 99.9) << "\n";
        cout << "p99 latency = " << hdr_value_at_percentile(hist1, 99.0) << "\n";
        cout << "p90 latency = " << hdr_value_at_percentile(hist1, 90.0) << "\n";
        cout << "p50 latency = " << hdr_value_at_percentile(hist1, 50.0) << "\n";
        cout << "Max latency = " << hdr_max(hist1) << "\n";
        cout << "HDR memory : " << hdr_get_memory_size(hist1) << "\n";
        hdr_close(hist1);
    }
};

struct consumer
{
    std::thread t1;
    ll count = 0;
    hdr_histogram *hist1 = nullptr;
    consumer(MPSCQueue<UDT> &q)
    {
        int rc1 = hdr_init(HDR_MIN_NS, HDR_MAX_NS, HDR_SIG_FIGS, &hist1);
        if (rc1 != 0 || hist1 == nullptr)
        {
            cerr << "Failed to initiate the hdr\n";
        }
        t1 = std::thread(&consumer::run, this, std::ref(q));
    }
    ~consumer()
    {
        t1.join();
        print_stats();
        std::cout << "Hthis count is =" << count << "\n";
    }
    void run(MPSCQueue<UDT> &q)
    {
        pin_thread_to_cpu_core(5);
        while (!go.load(std::memory_order_relaxed))
        {
            std::this_thread::yield();
        }
        while (count < 20000000LL)
        {
            asm volatile("lfence" ::: "memory");
            uint64_t start = rdtsc();
            asm volatile("" ::: "memory");
            bool is_it = (q.dequeue() != std::nullopt);
            count += is_it;
            asm volatile("lfence" ::: "memory");
            uint64_t end = rdtsc();
            // uint64_t end2 = clock_ns();
            uint64_t cycle = (end - start);
            double time = double(cycle) / tsc_freq_hz;
            if (is_it)
                hdr_record_value(hist1, time);
        }
    }
    void print_stats()
    {
        printf("─────────────────────────────────\n");
        printf("  Iterations : for consumer\n");
        printf("─────────────────────────────────\n");
        cout << "p99.9 latency = " << hdr_value_at_percentile(hist1, 99.9) << "\n";
        cout << "p99 latency = " << hdr_value_at_percentile(hist1, 99.0) << "\n";
        cout << "p90 latency = " << hdr_value_at_percentile(hist1, 90.0) << "\n";
        cout << "p50 latency = " << hdr_value_at_percentile(hist1, 50.0) << "\n";
        cout << "Max latency = " << hdr_max(hist1) << "\n";
        cout << "HDR memory : " << hdr_get_memory_size(hist1) << "\n";
        hdr_close(hist1);
    }
};

int main()
{
    pin_thread_to_cpu_core(4);
    tsc_freq_hz = calibrate_tsc_freq_ghz();
    MPSCQueue<UDT> q;
    std::vector<producer *> pro;
    for (ll i = 0; i < 20; i++)
    {
        pro.push_back(new producer(i * 1000000LL, q));
    }
    consumer *con = new consumer(q);

    go.store(true, std::memory_order_relaxed);
    asm volatile("lfence" ::: "memory");
    uint64_t start = rdtsc();
    asm volatile("" ::: "memory");
    for (auto x : pro)
    {
        delete x;
    }
    // con->print_stats();
    con->~consumer();
    asm volatile("lfence" ::: "memory");
    uint64_t end = rdtsc();
    uint64_t cycle = (end - start);
    double time = double(cycle) / tsc_freq_hz;
    cout << "total time taken = " << time << "ns \n";
    // hdr_add(hist1,hist2);

    return 0;
}