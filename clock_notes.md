what is the difference between the 
clock_gettime(MONOTONIC) and clock_gettime(MONOTONIC_RAW)?
CLOCK_MONOTONIC is adjusted by the NTP, it means it can change/slew to forward or backward and hence can impact the latency measurement with a large margin in mid run. we can even see negative latencies or anomalous spikes which are clock corrections not real latency.

CLOCK_MONOTONIC_RAW reads/follow the Hardware TSC (Time Stamp Counter), never follows NTP or get any type of correction, start at the boot. 

So Always use CLOCK_MONOTONIC_RAW for benchmarking.


rdtsc, and rdtscp
these are hardware assembly instructions direclty read the TSC.

rdtsc: CPU can execute this out of order relative to the surrounding instructions (so basically the compiler can reorder and cpu can execute not in order)

rdtscp: it wait for the prior executions to retire/execute (partly serializing), rdtscp guarantees you're reading the timestamp after everything before it (copiler can reorder but work for the cpu its serialized)

the cost of using the clock_monotonous and monotonous_raw is around 20-30ns and rdtsc/rdtscp is 1ns. use the clock when measuring something >200ns or it will introduce a big error in the latency data 
