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

struct data
{
    // int64_t a;
    // int64_t b;
    alignas(64) int64_t a;
    alignas(64) int64_t b;
    // int64_t c;
    // int64_t d;
};

struct data d1;
void pcount(){
    pin_thread_to_cpu_core(0);
    
    for(int i=0; i<1000000000; i++){
        d1.a++;
        asm volatile("" ::: "memory");
    }
}

void scount(){
    pin_thread_to_cpu_core(1);
    for(int i=0; i<1000000000; i++){
        d1.b++;
        asm volatile("" ::: "memory");
    }
}
int main(){
    uint64_t st = getclock();
    thread t1(pcount);
    thread t2(scount);
    t1.join();
    t2.join();
    uint64_t ed = getclock();
    cout<<"time taken in ns = " <<(ed-st)<<"\n";
    return 0;
}