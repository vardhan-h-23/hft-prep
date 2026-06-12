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




Slot 3 — FIX Gateway streaming layer + adversarial tests. Spec, so we start clean.
This slot has two jobs, both of which close threads left open earlier:
Job 1: the TCP streaming layer. Your parse(char*, size_t) fix handled the null-terminator bug, but the multi-message-per-read and partial-message-across-reads cases are still open (status was "unverified" when we last touched it). This slot closes them with a real accumulation buffer and a parse-consume loop.
Job 2: the adversarial test suite — the part of the original "Paralyzed"-session assignment that was never discharged, written without AI assistance for the test code.
Deliverables:

tcp_server.cpp with a per-connection std::vector<char> accumulation buffer, parse-loop that handles 0/1/many complete messages per read, and the documented buffer invariant in a comment.
parse returns bytes-consumed so the caller knows how much to erase.
Four adversarial tests (Python, no LLM for test code): two-messages-in-one-read, one-message-split-across-two-reads, garbage-then-valid, malformed-BodyLength-then-valid with resync.
slot3_writeup.md documenting the buffer invariant, the resync decision, and which FIX session-layer rule you followed.

Constraints, same shape as Slot 2: no copying a reference FIX parser; narrow syntax questions only; test code written by you, not generated; LLM budget 8, logged.
Before you write any code, one question to confirm you're starting from understanding not guessing — answer in 2-3 sentences, no lookup: when a single recv() returns 250 bytes containing one-and-a-half FIX messages, what exactly does your buffer hold after you've parsed and processed the one complete message, and what happens to the half message? If you can answer that cleanly, you own the streaming model and the implementation is mechanical. If not, that's the thing to think through first.





Day 1 Morning — the accumulation buffer (~4 hrs). Replace the fixed char data_[10240] with std::vector<char> read_buffer_ as a per-tcp_connection member (per-connection, not global — each client has its own stream). In do_read, you append: resize the buffer to make room, read into the tail region (read_buffer_.data() + old_size, up to CHUNK_SIZE bytes), then resize down to old_size + bytes_actually_read. In handle_reads, the parse loop: repeatedly call parse on the buffer; while it returns a complete message, process it and erase the consumed prefix; when it returns "no complete message yet," stop looping, leave the remaining bytes in the buffer, and re-issue do_read to append more. Write the invariant as a comment in your own words above the loop.
Day 1 Afternoon — parse returns bytes-consumed (~3 hrs). Currently parse implicitly assumes one message by finding |10= and the next |. Change the signature so it returns both the parsed message (or nullopt) and the number of bytes it consumed. This makes the contract explicit: the caller erases exactly bytes_consumed from the buffer front. This is API design — the return type is now std::pair<std::optional<Fix_Message>, size_t> or a small struct. The reason this matters: without an explicit consumed-count, the caller can't know how much to erase when there are multiple messages, and you'll either drop bytes or re-parse them.
Day 1 Evening — test plan (~2 hrs). Write tests/adversarial_plan.md. For each of the four scenarios, write what bytes you'll send and what the correct server behavior is. (1) Two complete messages in one send() — both must be parsed and processed. (2) One message split across two send() calls with a delay between — must be buffered and parsed once complete. (3) Garbage bytes followed by a valid message — garbage handling defined, valid message still parsed. (4) A message with a lying BodyLength followed by a valid message — malformed one rejected, resync to the valid one. Don't write test code yet; just specify behavior. This is the spec-first discipline that made Slot 2's queue work.
Day 2 Morning — write the tests, by hand, no LLM (~4 hrs). Python, against your running server. Each test opens a socket, sends the scenario bytes, asserts the server's response or observable behavior. Ugly code is fine; correctness is the point. Run each, watch pass/fail, fix the server when a test fails. This is the discharge of the original assignment — the tests are written by you because writing them is the skill being proven, not the having-them.
Day 2 Afternoon — resync (~3 hrs). For scenario 4, implement recovery: on a BodyLength mismatch (you can't trust the message's framing), scan the buffer forward for the next 8=FIX. start-of-message marker, discard everything before it, resume parsing there. Document the choice: the strict FIX session-layer rule is to drop the connection on an integrity violation, because a corrupt stream has no defined meaning; your parser implements best-effort resync because [your defensible reason — e.g., "for a resilient gateway that shouldn't drop a client connection over a single malformed message"]. The reason has to be one you can defend, not a hand-wave.
Day 2 Evening — ship and resume (~2 hrs). Push everything. Update the FIX gateway resume line to what's now defensible: TCP streaming with per-connection accumulation buffer, FIX 4.2 parser with body-length and checksum validation, adversarial test suite covering four stream-framing edge cases. Write slot3_writeup.md with the buffer invariant, the resync decision, and the session-layer rule you followed.
Constraints: No copying a reference FIX parser or asking an LLM to generate the framing logic. Narrow syntax/API questions only. Test code written entirely by you — this constraint is the point of the slot, not a formality. LLM budget 8, logged.
Exit criteria: Streaming handles 0/1/many messages per read and partial-across-reads. Four adversarial tests pass. Buffer invariant documented in code. Q4 closes fully. Two defensible artifacts on the resume.





Slot 5 structure — 2 days.
Day 1: Build bottom-up understanding by building a thing.
Write a UDP echo server in C++ on your laptop. Raw sockets, not boost::asio. The server receives a UDP packet, timestamps the receive (using clock_gettime CLOCK_MONOTONIC_RAW — you already know this from Slot 1), echoes it back, timestamps the send. The client sends 100k packets, records round-trip times, feeds them into your bench_harness HdrHistogram infrastructure from Slot 1. This gives you a baseline latency distribution on the kernel path.
Then answer these questions from measurement, not from reading: what does the p50 look like? What does the p99 look like? What's the ratio? Where is the jitter coming from? Use perf stat on the server process — you already know how from Slot 1. Count context switches, page faults, cache misses. Use cat /proc/interrupts before and after to see which IRQs hit your core.
Day 1 exit criteria: UDP echo server compiles and runs. HdrHistogram latency distribution printed. perf stat output saved. /proc/interrupts diff saved. All in the hft-prep repo.
Day 2: Build the conceptual layer on top of the measurement.
Now that you have numbers, study the path a packet actually takes from wire to your application. This is the stack you need to be able to draw on a whiteboard:
NIC receives frame → DMA writes to ring buffer in kernel memory → NIC raises hardware interrupt → interrupt handler schedules softirq → softirq (NET_RX_SOFTIRQ) processes packets from ring buffer → socket buffer (sk_buff) allocated → packet delivered to socket receive queue → your recv() call copies from kernel buffer to userspace.
That is the kernel path. Every step is a latency source. Your job on Day 2 is to map each step to a measurable instrument: ethtool -S for NIC-level drops, /proc/net/softnet_stat for softirq backlog, /proc/interrupts for interrupt routing, ss -unmp for socket buffer state.
Then: what does kernel bypass (OpenOnload) actually change? It replaces the bottom half of that stack. The NIC DMAs directly into userspace-mapped memory. No interrupt, no softirq, no sk_buff allocation, no kernel-to-userspace copy. Your polling thread reads directly from the ring buffer. That's it. That's the entire mechanism. Everything else (memory management, scheduling, TLB, SMIs) still runs through the kernel. That's why "kernel bypass means the kernel can't cause spikes" is wrong — and that was the Part B you couldn't answer.
Day 2 exit criteria: A writeup (slot5_writeup.md) that covers the full kernel receive path with each step labeled, what OpenOnload removes, what it doesn't remove, and the five latency spike sources on an isolated core (timer interrupts / tick, SMIs, IRQ affinity, page faults, TLB shootdowns) with detection command and fix for each. Plus: one-paragraph answer to "why doesn't kernel bypass fully isolate you from the kernel" — written in interview-answer structure (essence first, mechanism second, landing line).


Slot 5 question bank for articulation (to be drilled at end of Day 2):

Q1: Walk me through what happens between a UDP packet hitting the NIC and your application seeing the data. Where does kernel bypass change the path?
Q2: Your feed handler on an isolated core shows periodic 50–100μs spikes. Give me your diagnostic procedure, layer by layer.
Q3: Your colleague says kernel bypass means the kernel can't cause latency spikes. Why is that wrong? Give me two specific examples.
These three questions cover what you'll face in screening rounds. If you can answer all three cold in 90 seconds each with the essence-mechanism-point structure from Slot 4, this gap closes.
What this slot is not. It is not "read the OpenOnload paper." The original plan said "UDP echo + measurement, then read the paper after measurement." The paper comes after you have numbers and a mental model to hang it on. If you read the paper first, you'll memorize vocabulary without the physical intuition, and you'll produce the same recited answers that got you screened out before. Build first, read second.
Relationship to your daily shadowing/fluency sub-slot. The 30–45 minute daily drill runs in parallel. Day 1 morning: shadowing + one spoken compression drill from Slot 4 bank. Then switch to UDP echo server build. Day 2 evening: record yourself answering Q1 from the Slot 5 bank cold. That recording becomes your week's baseline for the fluency checkpoint.
Ready to start, or do you want to push back on the scope?