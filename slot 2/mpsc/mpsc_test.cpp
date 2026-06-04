#include "mpsc_queue.hpp"
#include <thread>
#include <vector>
#include <pthread.h>

std::atomic<bool> go{false};

void pin_thread_to_cpu_core(int core_id){
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(),
                            sizeof(cpu_set_t),
                            &cpuset);
}

struct producer
{
    std::thread t1;
    int id;
    producer(int id_,MPSCQueue<UDT>& q): id(id_){
        t1= std::thread(&producer::run,this,std::ref(q));
    }
    ~producer(){
        t1.join();
    }
    void run(MPSCQueue<UDT>& q){
        pin_thread_to_cpu_core(id%5);
        while(!go.load(std::memory_order_relaxed)){
            std::this_thread::yield();
        }
        for(int i=0; i<20000000; i++){
            UDT a{id,i};
            q.enqueue(a);
        }
    }
};

struct consumer
{
    std::thread t1;
    int count=0;
    consumer(MPSCQueue<UDT>& q){
        t1= std::thread(&consumer::run,this,std::ref(q));
    }
    ~consumer(){
        t1.join();
        std::cout << "Hthis count is =" << count<<"\n";
    }
    void run(MPSCQueue<UDT>& q){
        pin_thread_to_cpu_core(5);
        while(!go.load(std::memory_order_relaxed)){
            std::this_thread::yield();
        }
        while(count<20000000){
            count+=(q.dequeue()!=std::nullopt);
        }
    }
};



int main(){
    MPSCQueue<UDT> q;
    std::vector<producer*> pro;
    for(int i=0; i<1; i++){
        pro.push_back(new producer(i,q));
    }
    consumer* con=new consumer(q);
    go.store(true,std::memory_order_relaxed);
    for(auto x:pro){
        delete x;
    }
    con->~consumer();
    return 0;
}
