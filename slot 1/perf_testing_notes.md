Why my HITM count was only 4 (single-socket caveat)
perf c2c tracks two kinds of HITM: Local (same socket) and Remote (cross-socket). Remote HITM is the expensive one (~200+ cycles). Local HITM is cheaper (~30-40 cycles). On my single-socket i7-9750HF, Remote HITM can never happen — there's no second socket. So false sharing still hurts, but the pain shows up as Fill Buffer hits and Store L1D misses, not big HITM numbers.
On a dual-socket production server, the same code would light up the Remote HITM column and the per-access penalty would be 5-7x worse.
Interview one-liner: "perf c2c is a NUMA diagnostic — on single-socket it underreports contention severity because cross-socket coherence traffic doesn't exist. The real signal on single-socket is Fill Buffer pressure and store misses."


HITM = "Hit Modified" — load snoops another core's L1 and finds line in M state

Three reporting paths for the same false-sharing event:
  1. HITM      — snoop hits M in another L1     (~80-100 cyc)
  2. LLC hit   — line was written back to L3    (~40 cyc)
  3. FB hit    — line currently in-flight       (~20-30 cyc)

Why HITM count looks small:
  - PEBS samples ~0.02% of loads (1 in ~5000)
  - L3 inclusive cache absorbs writebacks → many become LLC hits
  - Fill buffer catches in-flight transfers → become FB hits
  - Single socket = Remote HITM always 0

Real signal on single-socket = sum of (FB hits + LLC hits on shared line + Local HITM)
On NUMA dual-socket = Remote HITM dominates and tells the truth directly

ldlat threshold: default 30 cyc. Lower with -l flag:
  sudo perf c2c record -l 10 ./binary



FlameGraph: structure identical before/after (expected — false sharing 
doesn't change call depth). Total samples 14.7B → 10.4B (~30% reduction) 
confirms shorter wall time. Kernel timer interrupt stack disappeared after 
fix — program completes before enough ticks accumulate to show in profile.