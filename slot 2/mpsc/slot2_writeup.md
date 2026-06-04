What I accomplished:
Read and implemented the MPSC queue;
Created state diagram on the rough page for cases with 
zero node
one node
and multiple nodes
Basically what happen if program gets preumpted and other thread win the race, will there be any discrepencies after preupmtion on any step
Wrote the logic of using each memory_ordering at each step.

I find the latencies of operations enque and deque for 20 Producers and 1 Consumer 
(where each producer is triggered to enque 1e6 entries and consumer have to consume concurrently all the entries of the queue)
and net latency of the whole run for queues:
[I scheduled 20 threads over 4 cores and consumer on 1 core and main thread on 1 core
I had limitation of 6 cores in my cpu and 8GB ram]

1. Created by me
2. MPMC of boost library (benchmark)
3. Simple queue proteted by mutex (raw rockbottom)

The details are in file new_comparison_report.txt; 
for my queue 
total time taken = 2.44716e+09 ns ~ 2.44 seconds
for boost's MPMC
total time taken = 5.51318e+09 ns ~ 5.5 seconds

for Simple queue
total time taken = 8.08849e+09 ns ~ 8 seconds

I think its remarkable for the current usecase;

Other perf testing results you can check in output_2e7.txt
which are crucial and we can work on improving them in the further iterations
TSAN run went fine there was no issue reported;


Now here are some findings i tested 2 cases: 
case1 : scheduled 20 threads over 4 cores and consumer on 1 core & main thread on 1 core
case2 : scheduled 20 threads over 1 core and consumer on 1 core & main thread on 1 core

throughput-vs-producers:
I found that the result of net-time taken was close and comparable, which is against the hypothesis that multiple parallel processing/scheduling threads on different core will always improve the latency 

more parallelism is not free. Spreading work across cores adds coherence cost
Sometimes the coherence cost exceeds the parallelism benefit, and the "more parallel" configuration runs slower in total


For a single contended cache line — which is exactly what a queue's tail pointer is — adding cores past a small number often makes total throughput worse, not better, because every core now fights for the one line. 
This is the fundamental scaling limit of any single-point-of-contention structure, and it is why MPSC queues plateau and then degrade as producer count rises.
I measured that degradation;