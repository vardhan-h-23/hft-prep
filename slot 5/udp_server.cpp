#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <cstring> 
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

int main(){
    hdr_histogram* hist = nullptr;
    int rc = hdr_init(HDR_MIN_NS,HDR_MAX_NS,HDR_SIG_FIGS, &hist);
    if (rc!=0 || hist==nullptr){
        cerr<<"Failed to initiate the hdr\n";
        return 1;
    }
    const double tsc_freq_hz = calibrate_tsc_freq_ghz();
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket<0){
        std::cerr<<"scoket creation failed\n";
        return 1;
    }
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(8080);
    // sin.sin_zero;
    bind(udp_socket,(const struct sockaddr*)(&sin),sizeof(sin));
    std::vector<char> buffer_;
    buffer_.resize(10240);
    struct sockaddr src_addr;
    socklen_t addrlen=sizeof(src_addr);
    int i=0;
    while (i<1000000){
        int bytes_received = recvfrom(udp_socket,buffer_.data(),buffer_.size(),MSG_DONTWAIT,&src_addr,&addrlen);
        if (bytes_received<=0)
        continue;
        asm volatile("lfence" ::: "memory");
        uint64_t end = rdtsc();
        uint64_t start;
        std::memcpy(&start,buffer_.data(),sizeof(start));
        uint64_t cycle  = (end-start);
        double time = double(cycle)/tsc_freq_hz;
        hdr_record_value(hist,time);
        i++;
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
    // std::cout<<bytes_received<<"\n";
    // if (bytes_received<0){
    //     std::cerr<<"error receiving the string failed\n";
    //     return 1;
    // }
    // for( int i=0; i<bytes_received; i++){
    //     std::cout<<buffer_[i];
    // }
    // std::vector<char> send_buffer_{'r', 'e', 'c', 'e', 'i', 'v', 'e', 'd'};
    // sendto(udp_socket,send_buffer_.data(),send_buffer_.size(),0,&src_addr,addrlen);
    close(udp_socket);
    return 0;
}
