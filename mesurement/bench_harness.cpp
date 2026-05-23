#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <hdr/hdr_histogram.h>

using namespace std;

static constexpr int64_t  WARMUP_ITERS  = 10000;
static constexpr int64_t  MEASURE_ITERS = 1000000;


static constexpr int64_t  HDR_MIN_NS    = 1;
static constexpr int64_t  HDR_MAX_NS    = 1000000000;
static constexpr int      HDR_SIG_FIGS  = 3;


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


void target_function(){
    asm volatile("":::"memory");
}


int main()
{
    hdr_histogram* hist = nullptr;
    int rc = hdr_init(HDR_MIN_NS,HDR_MAX_NS,HDR_SIG_FIGS, &hist);
    if (rc!=0 || hist==nullptr){
        cerr<<"Failed to initiate the hdr\n";
        return 1;
    }
    const double tsc_freq_hz = calibrate_tsc_freq_ghz();
    cout<<"tsc_freq_ghz = " << tsc_freq_hz<<endl;
    // warm up loop
    for(int i=0; i<WARMUP_ITERS; i++){
        target_function();
    }
    // measurement loop
    for(int i=0; i<MEASURE_ITERS; i++){
        // uint64_t start2 = clock_ns();
        asm volatile("lfence" ::: "memory");
        uint64_t start = rdtsc();
        asm volatile("" ::: "memory");   
        target_function();
        asm volatile("lfence" ::: "memory");
        uint64_t end = rdtsc();
        // uint64_t end2 = clock_ns();
        uint64_t cycle  = (end-start);
        double time = double(cycle)/tsc_freq_hz;
        // double time2 = end2-start2;
        hdr_record_value(hist,time);
        // cout<<"Time1 = " << time << "\n";
        // cout<<"Time2 = " << time2 << "\n";
    }
    printf("─────────────────────────────────\n");
    printf("  Iterations : %ld\n", MEASURE_ITERS);
    printf("─────────────────────────────────\n");
    cout<<"p99.9 latency = "<< hdr_value_at_percentile(hist, 99.9)<<"\n";
    cout<<"p99 latency = "<< hdr_value_at_percentile(hist, 99.0)<<"\n";
    cout<<"p90 latency = "<< hdr_value_at_percentile(hist, 90.0)<<"\n";
    cout<<"p50 latency = "<< hdr_value_at_percentile(hist, 50.0)<<"\n";
    cout<<"Max latency = "<< hdr_max(hist)<<"\n";
    cout<<"HDR memory : " << hdr_get_memory_size(hist)<<"\n";
    hdr_close(hist);

    return 0;
}