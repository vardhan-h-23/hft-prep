#include <bits/stdc++.h>
#include <atomic>
#include <thread>
using namespace std;

// int get_num(){
//     static int var=0;
//     var++;
//     return var;
// }

// void func(int a,int b){
//     cout<<a<<" "<<b<<"\n";
// }

// int main()
// {   
//     std::atomic_thread_fence(std::memory_order_seq_cst);
//     func(get_num(),get_num());
//     return 0;
// }

// atomic<bool> x,y;
// atomic<int> z{0};
// atomic<bool> go{false};


// void write_x_y(){
//     while(!go)
//       this_thread::yield();
//     x.store(true,memory_order_relaxed);
//     y.store(true,memory_order_relaxed);
// }
// void read_y_x(){
//     while(!go)
//       this_thread::yield();
//     while(!y.load(memory_order_relaxed)){ 
//         if (x.load(memory_order_relaxed))
//         ++z; // seq_cst only apply the global order to the atomics with model seq_cst
//     }
// }

// int main(){
//     x=0,y=0;
//     z=0;
//     thread t1(write_x_y);
//     thread t2(read_y_x);
//     sleep(1);
//     go=true;
//     t1.join();
//     t2.join();
//     assert(z.load()!=0);
//     return 0;
// }

// struct vals
// {
//     int x;
//     int y;
//     int z;
// };

// const int loop_count=10;
// atomic<bool> go{0};
// atomic<int> x{0},y{0},z{0};

// vals values1[loop_count];
// vals values2[loop_count];
// vals values3[loop_count];
// vals values4[loop_count];
// vals values5[loop_count];

// void increment(atomic<int>* var_to_inc,vals* val){
//     while(!go)
//     this_thread::yield();
//     for(int i=0; i<loop_count; i++){    
//         val[i].x = x.load(memory_order_relaxed);
//         val[i].y = y.load(memory_order_relaxed);
//         val[i].z = z.load(memory_order_relaxed);
//         var_to_inc->store(i+1,memory_order_relaxed);
//         this_thread::yield();
//     }
// }

// void read_vals(vals* val){
//     while(!go)
//     this_thread::yield();
//     for(int i=0; i<loop_count; i++){
//         val[i].x = x.load(memory_order_relaxed);
//         val[i].y = y.load(memory_order_relaxed);
//         val[i].z = z.load(memory_order_relaxed);
//         this_thread::yield();
//     }
// }

// void print(vals* val){
//     for( int i=0; i<loop_count; i++){
//         if (i){
//             cout<<',';
//         }
//         cout<<"(" << val[i].x<<" "<<val[i].y<<" "<<val[i].z<<")";
//     }
//     cout<<std::endl;
// }



// int main(){
//     thread t1(increment,&x,values1);
//     thread t2(increment,&y,values2);
//     thread t3(increment,&z,values3);
//     thread t4(read_vals,values4);
//     thread t5(read_vals,values5);
//     sleep(1);
//     go=true;
//     t1.join();
//     t2.join();
//     t3.join();
//     t4.join();
//     t5.join();
//     print(values1);
//     print(values2);
//     print(values3);
//     print(values4);
//     print(values5);
//     return 0;
// }


atomic<bool> x{0},y{0};
atomic<bool> go{0};
atomic<int> z{0};

void func1(){
    while(!go) this_thread::yield();
    x.store(true,memory_order_relaxed);
    y.store(true,memory_order_release);
}
void func2(){
    while(!go) this_thread::yield();
    while(!y.load(memory_order_acquire)){
        if (x.load(memory_order_relaxed))
        z++;
    }
}

int main()
{
    thread t1(func1);
    thread t2(func2);
    sleep(1);
    go = true;
    t1.join();
    t2.join();
    assert(z!=0);
    return 0;
}