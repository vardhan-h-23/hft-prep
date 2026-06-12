#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <cstring> 


// inline int64_t clock_ns() {
//     struct timespec t0;
//     clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
//     return static_cast<int64_t>(t0.tv_sec) * 1000000000LL + t0.tv_nsec;
// }
inline uint64_t  rdtsc(){
    uint32_t lo,hi;
    asm volatile("rdtsc": "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi)<<32) | lo;
}

// inline uint64_t  rdtscp(){
//     uint32_t lo,hi,aux;
//     asm volatile("rdtscp": "=a"(lo), "=d"(hi), "=c"(aux) :: "memory");
//     return (static_cast<uint64_t>(hi)<<32) | lo;
// }


int main(){
    int client_socket = socket(AF_INET, SOCK_DGRAM,0);
    if (client_socket<0){
        std::cerr<<"scoket creation failed\n";
        return 1;
    }
    sockaddr_in target;
    target.sin_family=AF_INET;
    target.sin_port=htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &target.sin_addr);
    std::vector<char> send_buffer_(10240);
    socklen_t sz=sizeof(target);
    int i=0;
    while (i<1000000){
        uint64_t start = rdtsc();
        asm volatile("" ::: "memory");   
        std::memcpy(send_buffer_.data(),&start,sizeof(start));
        sendto(client_socket,send_buffer_.data(),sizeof(send_buffer_),0,(const struct sockaddr*)&target,sz);
        i++;
    }
    
    close(client_socket);
}