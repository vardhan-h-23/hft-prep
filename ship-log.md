# this doc contain my deliverables in detail like end to end analysis of everything 


Day 22 May.
Morning block (~4 hours, deep work): Install and verify the tools. Linux perf (sudo apt install linux-tools-generic), bpftrace, HdrHistogram (C++ library: HdrHistogram_c or use hdrhist header-only), FlameGraph scripts from Brendan Gregg's GitHub. Sanity check each tool runs against /bin/ls. Read Brendan Gregg's "Linux perf Examples" page — it's the canonical fast-reference. Don't try to learn perf comprehensively. Learn perf stat, perf record -g, perf report, and perf c2c. Four commands. That's the slot's perf budget.
Afternoon block (~3 hours, lighter): Write bench_harness.cpp — a small framework that runs a target function in a tight loop, records each iteration's latency using clock_gettime(CLOCK_MONOTONIC_RAW), feeds the values into HdrHistogram, and at the end prints p50/p90/p99/p99.9/max. ~150 lines. The target function for now is a no-op; you're testing the harness, not anything else.
Evening block (~2 hours, consolidation): Write clock_notes.md — a short reference for yourself comparing clock_gettime(MONOTONIC), clock_gettime(MONOTONIC_RAW), rdtsc, and rdtscp. Cost in ns, what each measures, when to use which. This is the kind of doc that pays back ten times when an interviewer asks "how would you measure this?"


May 23 2026
Morning block (~4 hours, deep work): Take a deliberately-bad piece of code — write one or use a textbook example — and profile it three ways. A function that does adjacent-counter increments on two threads (the Q3 setup) is perfect for this. Run perf stat -e cache-misses,LLC-load-misses,task-clock,context-switches. Run perf c2c record and read the report — find the HITM column. Run perf record -g + FlameGraph and produce an SVG. Each command, every flag, understand why it's there.
Afternoon block (~3 hours): Fix the bad code (add alignas(64)), re-run all three measurements, and write slot1_writeup.md with a before/after table: throughput, p99, cache-miss rate, HITM events per second. The numbers proving you fixed it ARE the slot's exit criterion. If your perf c2c report doesn't show HITM dropping to near-zero, you didn't fix it — you just believe you did. This is the Q1/Q3 pattern, caught for real this time.
Evening block (~2 hours): Ship-log + Slot 2 prep. Read the first 20 pages of Vyukov's MPSC algorithm description at 1024cores.net. Just read; no implementation yet.
Slot 1 exit criteria. Three artifacts in your repo: bench_harness.cpp that compiles and runs, slot1_writeup.md with before/after measurements of false-sharing fix, and a flame graph SVG. The skills line perf, perf c2c, HdrHistogram, ThreadSanitizer, flame graphs clears its [PENDING] marker on the resume.

