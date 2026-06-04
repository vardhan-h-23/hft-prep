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





<!-- Order matters. Read Vyukov first, even though it's the application and CCIA is the foundation. Reason: Vyukov is concrete code with a concrete problem. Reading it first will surface specific questions about memory ordering that CCIA then answers. Reading CCIA first means absorbing abstract material with no anchor, and most of it won't stick. Vyukov first creates the anchors; CCIA fills them in. Counter-intuitive but it's the right sequence for this material.
The defendable-claims artifact. After Vyukov, before opening CCIA: write 5 claims you'd defend. After CCIA 5.1-5.3, write 5 more. These go into a new file in the repo — reading_notes_slot2_prep.md. Not paragraphs. Single-sentence claims, each one specific enough that I could ask "why" and you'd have a real answer. Examples of the shape: "Vyukov's MPSC uses exchange on tail instead of CAS because exchange makes the publication step a single atomic instruction with no failure case to retry." Not "Vyukov's MPSC is fast." The first is defendable, the second is fluff. If you can produce 10 of those by 10 PM, the reading served its purpose. If you can only produce 4, that's data — you need more time, not a thinner artifact.
The paper test for Vyukov specifically. After reading the post, before touching code or LLM, draw the queue's state on paper at four moments: empty, one node enqueued, two nodes enqueued, mid-enqueue (producer between the exchange and the next write — the unsafe window). If you can draw these four states from memory and explain what the consumer sees at each, you have the algorithm. If you can't, re-read. The drawing is the test, not the reading. Photograph the paper when done and commit it to the repo as vyukov_state_diagrams.jpg or similar — it's the artifact that proves the conceptual layer landed.
 -->

Slot 3

Day 1 (tomorrow, ~8 hours).
Morning (~4 hrs, deep): Draw the four state diagrams from memory of Vyukov's post. Photograph or commit. Then start implementing mpsc_queue.hpp. Write the class skeleton, head/tail members with proper alignment, dummy node initialization. Implement enqueue with memory-ordering comments per the spec. Compile.
Afternoon (~3 hrs): Implement dequeue. Write the stress test. Compile with -fsanitize=thread. Run. Fix until TSan is clean. Do not move forward until TSan is clean — this is the verification gate, same as Slot 1's HITM-to-zero criterion.
Evening (~1 hr): Write memory_orderings.md from your comments. Ship-log + sleep.
Day 2 (day after, ~8 hours).
Morning (~4 hrs): Verify the dummy-node reclamation is correct. Run stress test for 30+ minutes continuous to surface rare races. Measure performance with Slot 1 harness — latency distribution at 1, 2, 4 producer counts. Record numbers.
Afternoon (~3 hrs): Write slot2_writeup.md — measurements, observations, anything that surprised you. Then close everything and do the build-twice exercise. New file mpsc_queue_v2.hpp, from memory.
Evening (~1 hr): Diff v1 and v2. Whatever differs is what you didn't own. Note it in the writeup. Ship-log + sleep.
