#include <iostream>
#include <thread>
#include <pthread.h>
#include <hdr/hdr_histogram.h>


using namespace std;


inline uint64_t getclock(){
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    return static_cast<uint64_t>(t0.tv_sec)*1000000000LL + t0.tv_nsec;
}

void pin_thread_to_cpu_core(int core_id){
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(),
                            sizeof(cpu_set_t),
                            &cpuset);
}

static constexpr int64_t  WARMUP_ITERS  = 10000;
static constexpr int64_t  MEASURE_ITERS = 1000000;


static constexpr int64_t  HDR_MIN_NS    = 1;
static constexpr int64_t  HDR_MAX_NS    = 1000000000;
static constexpr int      HDR_SIG_FIGS  = 3;
hdr_histogram* hist1 = nullptr;
hdr_histogram* hist2 = nullptr;
double tsc_freq_hz;

inline int64_t clock_ns() {
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    return static_cast<int64_t>(t0.tv_sec) * 1000000000LL + t0.tv_nsec;
}
inline uint64_t  rdtsc(){
    uint32_t lo,hi;
    asm volatile("rdtsc": "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi)<<32) | lo;
}

inline uint64_t  rdtscp(){
    uint32_t lo,hi,aux;
    asm volatile("rdtscp": "=a"(lo), "=d"(hi), "=c"(aux) :: "memory");
    return (static_cast<uint64_t>(hi)<<32) | lo;
}


double calibrate_tsc_freq_ghz() {
    // Warm up
    rdtscp();
    clock_ns();

    int64_t ns_start = clock_ns();
    uint64_t tsc_start = rdtscp();

    // Sleep ~100ms — long enough for accuracy, short enough for startup
    struct timespec req = {0, 100'000'000};  // 100ms
    nanosleep(&req, nullptr);

    uint64_t tsc_end = rdtscp();
    int64_t ns_end = clock_ns();

    double tsc_delta = static_cast<double>(tsc_end - tsc_start);
    double ns_delta = static_cast<double>(ns_end - ns_start);

    // ticks per nanosecond = GHz
    return tsc_delta / ns_delta;
}


struct data
{
    int64_t a;
    int64_t b;
    // alignas(64) int64_t a;
    // alignas(64) int64_t b;
    // int64_t c;
    // int64_t d;
};

struct data d1;
void pcount(){
    pin_thread_to_cpu_core(0);
    
    for(int i=0; i<100000000; i++){
        asm volatile("lfence" ::: "memory");
        uint64_t start = rdtsc();
        asm volatile("" ::: "memory"); 
        d1.a++;
        asm volatile("lfence" ::: "memory");
        uint64_t end = rdtsc();
        // uint64_t end2 = clock_ns();
        uint64_t cycle  = (end-start);
        double time = double(cycle)/tsc_freq_hz;
        // double time2 = end2-start2;
        hdr_record_value(hist1,time);
        
    }
}

void scount(){
    pin_thread_to_cpu_core(1);
    for(int i=0; i<100000000; i++){
        asm volatile("lfence" ::: "memory");
        uint64_t start = rdtsc();
        asm volatile("" ::: "memory"); 
        d1.b++;
        asm volatile("lfence" ::: "memory");
        uint64_t end = rdtsc();
        // uint64_t end2 = clock_ns();
        uint64_t cycle  = (end-start);
        double time = double(cycle)/tsc_freq_hz;
        // double time2 = end2-start2;
        hdr_record_value(hist2,time);
    }
}
int main(){
    // uint64_t st = getclock();
    // thread t1(pcount);
    // thread t2(scount);
    // t1.join();
    // t2.join();
    // uint64_t ed = getclock();
    // cout<<"time taken in ns = " <<(ed-st)<<"\n";
    int rc1 = hdr_init(HDR_MIN_NS,HDR_MAX_NS,HDR_SIG_FIGS, &hist1);
    int rc2 = hdr_init(HDR_MIN_NS,HDR_MAX_NS,HDR_SIG_FIGS, &hist2);
    if (rc1!=0 || rc2!=0 || hist1==nullptr || hist2==nullptr){
        cerr<<"Failed to initiate the hdr\n";
        return 1;
    }
    tsc_freq_hz = calibrate_tsc_freq_ghz();
    thread t1(pcount);
    thread t2(scount);
    t1.join();
    t2.join();
    hdr_add(hist1,hist2);
    printf("─────────────────────────────────\n");
    printf("  Iterations : %ld\n", MEASURE_ITERS);
    printf("─────────────────────────────────\n");
    cout<<"p99.9 latency = "<< hdr_value_at_percentile(hist1, 99.9)<<"\n";
    cout<<"p99 latency = "<< hdr_value_at_percentile(hist1, 99.0)<<"\n";
    cout<<"p90 latency = "<< hdr_value_at_percentile(hist1, 90.0)<<"\n";
    cout<<"p50 latency = "<< hdr_value_at_percentile(hist1, 50.0)<<"\n";
    cout<<"Max latency = "<< hdr_max(hist1)<<"\n";
    cout<<"HDR memory : " << hdr_get_memory_size(hist1)<<"\n";
    hdr_close(hist1);
    // cout<<"tsc_freq_ghz = " << tsc_freq_hz<<endl;
    return 0;
}