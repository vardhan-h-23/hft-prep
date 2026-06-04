#include <iostream>
#include <atomic>
#include <optional>
#include <thread>

template <class T>
class MPSC
{
private:
    struct Node
    {
        T data;
        std::atomic<Node *> next;
    };
    Node *stub;
    alignas(64) std::atomic<Node *> head;
    alignas(64) std::atomic<Node *> tail;

public:
    MPSC()
    {
        stub = new Node();
        stub->next.store(nullptr, std::memory_order_relaxed);
        head.store(stub, std::memory_order_relaxed);
        tail.store(stub, std::memory_order_relaxed);
    }
    ~MPSC()
    {
        while (dequeue() != std::nullopt);
        Node* node = head.load(std::memory_order_relaxed);
        delete node;
        node=nullptr;
    }
    void enqueue(const T&  val)
    {
        Node *new_node = new Node();
        new_node->data = val;
        new_node->next.store(nullptr, std::memory_order_relaxed);
        Node *old_node = tail.exchange(new_node, std::memory_order_acq_rel);
        old_node->next.store(new_node, std::memory_order_release);
    }
    std::optional<T> dequeue()
    {
        Node *old_head = head.load(std::memory_order_relaxed);
        Node *new_head = old_head->next.load(std::memory_order_acquire);
        if (new_head)
        {
            T tmp = new_head->data;
            head.store(new_head, std::memory_order_relaxed);
            delete old_head;
            return tmp;
        }
        return std::nullopt;
    }
};