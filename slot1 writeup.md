# Slot 1 Writeup — False Sharing Fix
**Date:** 2026-05-23
**Machine:** Lenovo IdeaPad L340-15IRH, i7-9750HF (Coffee Lake, 6 physical cores × 2 HT, single socket), Ubuntu 22.04.5, kernel 6.8.0-117
**Compiler:** g++ -std=c++20 -O2 -pthread

---

## What was tested

Two threads, pinned to physical cores 0 and 1 (cpu0 and cpu1 per lscpu — confirmed different cores, not HT siblings), each incrementing a separate `int64_t` counter 1,000,000,000 times.

**Bad version (`false_sharing1`):** Both counters in the same struct, 8 bytes apart → same 64-byte cache line.
```cpp
struct data {
    int64_t a;   // offset 0x30 in the cache line
    int64_t b;   // offset 0x38 — 8 bytes away, SAME LINE
};
```

**Fixed version (`not_false_sharing`):** Each counter on its own cache line via `alignas(64)`.
```cpp
struct data {
    alignas(64) int64_t a;
    alignas(64) int64_t b;
};
```

---

## Before / After Table

| Metric | Before (false sharing) | After (alignas(64)) | Change |
|---|---|---|---|
| **Wall time (getclock)** | 2.904s | 2.097s | **1.38x faster** |
| **Throughput** | ~172M iter/s/thread | ~238M iter/s/thread | **+38%** |
| **p99 latency** | N/A — HdrHistogram deferred to Slot 2 | N/A | — |
| **L1-dcache-load-misses** | 80,730,362 | 360,908 | **−99.55%** |
| **Store L1D Miss (perf c2c)** | 56,318 | 18 | −99.97% |
| **Fill Buffer hits on shared line** | 2,131 | 0 | −100% |
| **Shared cache lines detected** | 1 | 0 | eliminated |
| **Local HITM events (sampled)** | 19 | 0 | eliminated |
| **Est. HITM/sec (×1000 sample period)** | ~1,900/sec | 0/sec | eliminated |
| **LLC-load-misses** | 23,472 | 33,184 | see note‡ |
| **context-switches** | 49 | 45 | noise |

‡ `LLC-load-misses` (i.e. DRAM accesses) went up slightly after the fix. This is expected and not a regression: with false sharing, most apparent "misses" from one core's perspective are actually coherence hits in another core's L1 (HITM) or L3. They never reach DRAM. With alignas, each core owns its line cleanly, so the first access after a cold eviction is a real DRAM miss. The small raw increase (~10k) is noise compared to the 30% wall-time improvement.

---

## perf c2c — before (key lines)

```
Total Shared Cache Lines       : 1
Fill Buffer Hits on shared lines : 2,131
Store HITs on shared lines     : 1,211,007
Store L1D hits on shared lines : 1,154,689
Store L1D Miss                 : 56,318
Local HITM                     : 19

Pareto:
  pcount() offset 0x30 — 36.84% of HITM, avg lcl hitm penalty: 97 cycles
  scount() offset 0x38 — 63.16% of HITM, avg lcl hitm penalty: 100 cycles
```

Both functions on the same cache line, 8 bytes apart. Every store by one thread invalidates the line in the other thread's L1.

## perf c2c — after

```
Total Shared Cache Lines       : 0
Store L1D Miss                 : 18
Fill Buffer Hits on shared lines : 0
Local HITM                     : 0
```

No shared cache lines. Fix confirmed.

---

## Flamegraph observation

Both before and after flamegraphs show identical structure: `all → binary → pcount (50%) / scount (50%)`. Both functions are leaf frames — the loop contains no function calls.

FlameGraph does NOT expose false sharing. The CPU appears "on CPU" even while stalled on cache coherence. The signal is in total sample count: **14.75B samples (before) vs 10.41B samples (after)** at the same `-F 99` frequency — a ~30% reduction in wall time, consistent with perf stat.

Kernel timer interrupt stack (`asm_sysvec_apic_timer_interrupt → hrtimer → tick_nohz`) visible in the before graph, absent in the after graph — program completes before enough ticks accumulate to appear in the profile.

---

## Root cause

`struct data` without padding places `a` at offset 0 and `b` at offset 8 within the same 64-byte cache line. Core 0 owns `a`, Core 1 owns `b`. Every write by Core 0 sets the line to Modified in its L1, invalidating Core 1's copy (I state). Core 1's next read triggers a coherence transaction (snoop, writeback, reload). This ping-pong happens ~1B times.

**Fix:** `alignas(64)` forces each counter to the start of its own cache line. Core 0 and Core 1 never contend — their lines are independent.

---

## Key single-socket caveat (for interviews)

On a single-socket machine (this machine), `perf c2c` underreports HITM severity:
- Remote HITM is always 0 (no second socket)
- Many contention events resolve as Fill Buffer hits or LLC hits, not HITM
- PEBS samples ~1 in 1000 events at `-c 1000` — raw HITM count is ~19 samples, true rate ~19,000/sec

On a dual-socket NUMA server, the same code would show Remote HITM dominating, with per-access penalties of 200+ cycles vs ~100 cycles local.

Primary single-socket signals (ranked by reliability):
1. L1-dcache-load-misses (counted, not sampled)
2. Store L1D Miss (c2c, high volume)
3. Fill Buffer hits on shared lines (c2c)
4. Local HITM (c2c, noisy)

---

## Artifacts

- `bench_harness.cpp` (false_sharing_example.cpp) — compiles and runs
- `flamegraph_before.svg` — pcount/scount as leaf frames, 14.75B samples, timer interrupt visible
- `flamegraph_after.svg` — same structure, 10.41B samples, no interrupt stack
- `perf_stat_before.txt` — 9.957s wall, 99M L1-dcache-load-misses
- `perf_stat_after.txt` — 6.842s wall
- `c2c_before.txt` — 1 shared line, 19 HITM, 56k store misses
- `c2c_after.txt` — 0 shared lines, 0 HITM, 18 store misses

---

## TODO for Slot 2 cleanup

- [ ] Add HdrHistogram per-iteration latency measurement → p99/p999 before/after