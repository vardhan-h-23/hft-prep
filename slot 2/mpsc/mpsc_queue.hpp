// Unbounded, non-intrusive, linked-list queue
// One consumer thread, N producer threads
// The interface should not lie to the user; 
// it offers only operations whose results are still meaningful at call-return time

#include <iostream>
#include <new>
#include <atomic>
#include <optional>
// #include <thread>
typedef long long ll;
struct UDT
{
    ll processor_id;
    ll sequence_id;

};


template <typename T>
class MPSCQueue {
private:
    // non intrusive node (linked-list)
    struct Node{
        T data; // only reading and update will happen but only by one thread at a time
        std::atomic<Node*> next; // read and updated by multiple threads at a time so need atomic 
    };
    Node* stub;
    alignas(64) std::atomic<Node*> head; //assuming 64 bit sized cache line 
    alignas(64) std::atomic<Node*> tail; //assuming 64 bit sized cache line 

public:
    MPSCQueue(){
        stub=new Node();
        stub->next.store(nullptr,std::memory_order_relaxed);
        head.store(stub,std::memory_order_relaxed);
        tail.store(stub,std::memory_order_relaxed);
    }
    ~MPSCQueue(){
        while(dequeue()!=std::nullopt);
        Node* node = head.load(std::memory_order_relaxed);
        delete node;
        node=nullptr;
    }

    // Non-copyable, non-movable. Single instance.
    MPSCQueue(const MPSCQueue&) = delete;
    MPSCQueue& operator=(const MPSCQueue&) = delete;

    // Producer-side: enqueue a value. Safe to call from any thread.
    // Returns void; cannot fail (unbounded queue, allocation may throw).
    void enqueue(T& value){
        Node* new_node = new Node();
        new_node->data = value;
        new_node->next.store(nullptr,std::memory_order_relaxed);
        Node* old_tail = tail.exchange(new_node, std::memory_order_acq_rel); // still why/?
        old_tail->next.store(new_node,std::memory_order_release);
    }

    // Consumer-side: dequeue a value. ONLY safe to call from the single
    // consumer thread. Returns std::nullopt if queue is empty OR if a
    // producer is mid-enqueue (consumer cannot distinguish these cases).
    std::optional<T> dequeue(){
        Node* old_node = head.load(std::memory_order_relaxed);
        Node* new_node = old_node->next.load(std::memory_order_acquire);
        if (new_node){
            T output= new_node->data;
            head.store(new_node,std::memory_order_relaxed);
            delete old_node;
            return output;
        }
        return std::nullopt;
    }    
};


// Draw the four state diagrams from memory of Vyukov's post
// Let's create the situations and find out/