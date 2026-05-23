# this doc contain my deliverables in detail like end to end analysis of everything 


Day 22 May.
Morning block (~4 hours, deep work): Install and verify the tools. Linux perf (sudo apt install linux-tools-generic), bpftrace, HdrHistogram (C++ library: HdrHistogram_c or use hdrhist header-only), FlameGraph scripts from Brendan Gregg's GitHub. Sanity check each tool runs against /bin/ls. Read Brendan Gregg's "Linux perf Examples" page — it's the canonical fast-reference. Don't try to learn perf comprehensively. Learn perf stat, perf record -g, perf report, and perf c2c. Four commands. That's the slot's perf budget.
Afternoon block (~3 hours, lighter): Write bench_harness.cpp — a small framework that runs a target function in a tight loop, records each iteration's latency using clock_gettime(CLOCK_MONOTONIC_RAW), feeds the values into HdrHistogram, and at the end prints p50/p90/p99/p99.9/max. ~150 lines. The target function for now is a no-op; you're testing the harness, not anything else.
Evening block (~2 hours, consolidation): Write clock_notes.md — a short reference for yourself comparing clock_gettime(MONOTONIC), clock_gettime(MONOTONIC_RAW), rdtsc, and rdtscp. Cost in ns, what each measures, when to use which. This is the kind of doc that pays back ten times when an interviewer asks "how would you measure this?"


