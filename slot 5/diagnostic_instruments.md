LINUX UDP RECEIVE PATH — LAYER-BY-LAYER INSTRUMENTATION REFERENCE
==================================================================


LAYER 1 — NIC HARDWARE
-----------------------
What happens here:
  Raw wire bits arrive at the NIC. The hardware performs MAC/PHY validation,
  CRC checks, and applies hardware-level filtering such as RSS and flow steering.

Instrument:
  ethtool -S <iface>

Healthy reading:
  rx_errors=0, rx_crc_errors=0, rx_missed_errors=0,
  rx_fifo_errors=0, rx_no_buffer_count=0

Bad signal:
  Any non-zero value in the above counters indicates a wire or hardware problem,
  or the NIC is being overwhelmed by traffic.


LAYER 2 — NIC RX RING BUFFER (DESCRIPTOR RING)
-----------------------------------------------
What happens here:
  The NIC DMAs each incoming frame into a pre-posted memory buffer and marks
  the corresponding descriptor as done. The kernel must drain these descriptors
  fast enough to keep the ring free.

Instrument:
  ethtool -S <iface> | grep -E "drop|miss|discard"
  ethtool -g <iface>   (to check ring size)

Healthy reading:
  rx_dropped=0, ring utilisation below 50% of capacity,
  no rx_no_dma_resources counter incrementing.

Bad signal:
  rx_dropped > 0 means the ring filled up before NAPI could drain it.
  Fix: increase ring size with ethtool -G <iface> rx 4096, or fix IRQ affinity.


LAYER 3 — MSI-X IRQ DELIVERY TO CORE
--------------------------------------
What happens here:
  The NIC issues a targeted PCIe memory write directly to a specific core's
  Local APIC (LAPIC). This schedules the IRQ handler on that core without
  broadcasting to others.

Instrument:
  cat /proc/interrupts | grep <iface>
  cat /proc/irq/<N>/smp_affinity_list

Healthy reading:
  IRQ count rising on only the cores you intend. The affinity list must match
  your CPU isolation plan — never the full core range.

Bad signal:
  IRQs landing on your isolated/pinned core (e.g. core 4) cause contention.
  An affinity of 0-11 means the kernel is fan-outing IRQs across all cores,
  which is the default and is unacceptable for HFT workloads.


LAYER 4 — NAPI POLLING (SOFTIRQ CONTEXT)
-----------------------------------------
What happens here:
  The kernel raises NET_RX_SOFTIRQ. The driver's poll function runs and drains
  the RX ring up to a packet budget (default 64) per call. If the budget is
  exhausted before the ring is empty, a time_squeeze is recorded and polling
  is rescheduled.

Instrument:
  cat /proc/net/softnet_stat
  (per-CPU rows in hex: column 1 = processed, column 2 = dropped,
   column 3 = time_squeeze)

Healthy reading:
  Column 2 (dropped) is 00000000. Column 3 (time_squeeze) is either zero
  or rising very slowly under burst conditions.

Bad signal:
  time_squeeze rising rapidly means NAPI is repeatedly hitting its budget
  and being kicked off the CPU before it can drain the ring. Raise
  net.core.netdev_budget and reduce net.core.netdev_budget_usecs.


LAYER 5 — SOFTIRQ SCHEDULING
------------------------------
What happens here:
  If softirqs cannot be processed inline on the IRQ return path (because they
  have been deferred or are taking too long), the kernel wakes ksoftirqd/N to
  process them as a normal schedulable thread on core N.

Instrument:
  top -H -p $(pgrep ksoftirqd)
  pidstat -t 1 | grep softirq
  perf stat -e irq:softirq_entry,irq:softirq_exit

Healthy reading:
  ksoftirqd CPU% is near zero. Softirqs are being processed inline, not
  deferred to the ksoftirqd thread.

Bad signal:
  High ksoftirqd/4 CPU% means softirqs are piling up on core 4. Either the
  core is overloaded or something is preempting the softirq processing path.


LAYER 6 — sk_buff ALLOCATION
------------------------------
What happens here:
  For every received packet the kernel allocates an sk_buff struct from the
  skbuff_head_cache slab. This is the per-packet metadata container that
  carries the packet through the entire receive path.

Instrument:
  sudo slabtop -o | grep -E "skbuff|sock"
  cat /proc/slabinfo | grep skbuff

Healthy reading:
  active_objs count is stable and not growing unboundedly. num_slabs is flat
  under steady-state load.

Bad signal:
  Growing active_objs indicates a leak — skbs are not being freed after recv.
  Very high allocation churn means every packet is hitting the allocator; consider
  skb recycling or SO_BUSY_POLL to amortise this cost.


LAYER 7 — GRO AND PROTOCOL DEMUX
----------------------------------
What happens here:
  Generic Receive Offload (GRO) attempts to coalesce packets (primarily useful
  for TCP; rare for UDP). The IP layer then routes the packet and the UDP layer
  looks up the destination socket via a hash table.

Instrument:
  ethtool -k <iface> | grep -E "gro|gso"
  cat /proc/net/snmp | grep -A1 Udp:

Healthy reading:
  UDP InDatagrams rising at a rate matching the sender. InCsumErrors at zero.

Bad signal:
  InCsumErrors > 0 means checksum failures — either a checksum offload
  misconfiguration or genuine packet corruption on the wire.
  RcvbufErrors already incrementing at this stage means the socket buffer
  is overflowing before the packet even reaches the queue.


LAYER 8 — SOCKET RECEIVE QUEUE (sk_receive_queue)
---------------------------------------------------
What happens here:
  The UDP layer enqueues the sk_buff onto your socket's receive queue.
  The queue depth is bounded by SO_RCVBUF. If the queue is full when a
  packet arrives, it is silently dropped here.

Instrument:
  ss -u -n -m -p
  (shows rb = recv buffer size, r = current queued bytes, Drops = drop count)

Healthy reading:
  r: (currently queued bytes) is near zero under steady drain. Drops is zero.
  rb: matches the SO_RCVBUF value you set via setsockopt.

Bad signal:
  r: growing steadily means the application is not calling recv() fast enough.
  Drops > 0 means packets are being discarded at the socket boundary — they
  made it through the entire kernel path and were lost at the last step.


LAYER 9 — RECEIVE ERRORS (KERNEL GLOBAL COUNTERS)
---------------------------------------------------
What happens here:
  The kernel maintains global UDP drop counters for conditions such as: socket
  queue full, no socket listening on the destination port, or checksum error.

Instrument:
  cat /proc/net/udp          (drops column, per socket)
  nstat -az | grep -i udp
  cat /proc/net/snmp | grep Udp

Healthy reading:
  RcvbufErrors=0, InErrors=0, per-socket drops column = 0.

Bad signal:
  RcvbufErrors > 0 is a hard confirmation that the socket buffer was full
  when a packet arrived and the packet was lost. This is the most actionable
  drop counter — it means you must either increase SO_RCVBUF, drain faster,
  or both.


LAYER 10 — WAKEUP AND SCHEDULER
---------------------------------
What happens here:
  Once a packet is on the socket queue, the kernel marks the application task
  as runnable. If the application is sleeping on a different core, a reschedule
  IPI is fired to that core. The scheduler then context-switches the application
  back in.

Instrument:
  perf stat -a -C 4 -e irq_vectors:reschedule_entry,sched:sched_wakeup,sched:sched_switch

Healthy reading:
  Reschedule IPIs approximately equal the number of recv calls, only when
  softirq and application are intentionally on different cores. Ideally zero,
  achieved by co-locating softirq and application via RPS/RFS.

Bad signal:
  High reschedule IPI count means every packet is paying a cross-core wakeup
  penalty — typically 2–5 µs each. Fix with RPS/RFS to steer packets to the
  same core as the application, or eliminate wakeups entirely with SO_BUSY_POLL.


LAYER 11 — APPLICATION RECV CALL
----------------------------------
What happens here:
  The application calls recv / recvfrom / recvmmsg. The kernel copies the
  payload from the sk_buff into userspace memory (copy_to_user), frees the
  sk_buff back to the slab, and returns to userspace.

Instrument:
  perf stat -e syscalls:sys_enter_recvfrom,syscalls:sys_exit_recvfrom -p <pid>
  strace -c -e recvfrom -p <pid>

Healthy reading:
  Recv call count equals the sender's packet count. Using recvmmsg to batch
  multiple packets per syscall reduces overhead significantly.

Bad signal:
  Recv count less than send count means loss occurred upstream (see layers 1–9).
  Very high per-call syscall overhead means the application is paying a full
  syscall round-trip per packet — switch to recvmmsg with a batch size of
  16–64, or use SO_BUSY_POLL to avoid the syscall entirely.


LAYER 12 — APPLICATION LATENCY (END-TO-END)
---------------------------------------------
What happens here:
  Your application stamps a send timestamp at the sender and a recv timestamp
  at the receiver. The delta is the true end-to-end latency as seen by the
  application, capturing all noise from every layer above.

Instrument:
  Your HdrHistogram output — p50, p99, p99.9, and max.

Healthy reading:
  p50 to p99 ratio below 3x. p99 to max ratio below 10x.
  A unimodal distribution with no secondary spike or long tail.

Bad signal:
  p99.9 spikes indicate periodic interference from one of layers 3–10 above.
  A max value that is an order of magnitude above p99.9 points to a rare
  but severe event — most likely an SMI, a major page fault, or a context
  switch storm on the pinned core.


==================================================================
TRIAGE FLOW — WALK THIS TOP-DOWN WHEN YOU SEE DROPS
==================================================================

  1. NIC errors > 0           →  wire / duplex / cable problem
  2. NIC drops > 0, errors=0  →  ring buffer full; ethtool -G rx 4096
  3. softnet_stat dropped > 0 →  backlog overflow; raise netdev_max_backlog
  4. softnet_stat time_squeeze →  NAPI budget exhausted; raise netdev_budget
  5. UDP InCsumErrors > 0     →  checksum offload misconfigured or corruption
  6. RcvbufErrors / ss Drops  →  socket buffer full; raise SO_RCVBUF, drain faster
  7. All counters clean        →  loss is upstream (sender or network path)

  For tail latency with no drops: investigate layers 3, 5, and 10
  (IRQ affinity, ksoftirqd load, reschedule IPIs).


==================================================================
TUNING KNOBS BY LAYER
==================================================================

  Ring (2)     ethtool -G <iface> rx <N>                  default 256    →  4096
  IRQ  (3)     /proc/irq/<N>/smp_affinity_list             all CPUs       →  non-isolated cores only
  NAPI (4)     net.core.netdev_budget                      300            →  600+
  NAPI (4)     net.core.netdev_budget_usecs                8000           →  2000
  SoftIRQ (5)  net.core.netdev_max_backlog                 1000           →  30000
  RPS  (10)    /sys/class/net/<i>/queues/rx-N/rps_cpus     0              →  mask of app core
  Socket (8)   setsockopt SO_RCVBUF                        rmem_default   →  as large as needed
  Socket (8)   net.core.rmem_max                           208 KB         →  64 MB+
  Socket (10)  setsockopt SO_BUSY_POLL                     0 µs           →  50–200 µs
  App    (11)  recvmmsg batch size                         1              →  16–64


==================================================================
INTERVIEW VOCABULARY MAP
==================================================================

  "How does a packet get from wire to your application?"
      Walk layers 1 through 12 in order.

  "What is NAPI?"
      Layer 4. Polled draining of the NIC ring buffer. Replaces the
      per-packet interrupt model under high load, trading IRQ overhead
      for a polling budget per softirq invocation.

  "What is a softirq?"
      Layer 5. A deferred bottom-half handler. Runs inline on the IRQ
      return path when possible; falls back to ksoftirqd/N when backed up.

  "How do you find where packets are being dropped?"
      Walk the instrument column of layers 1 through 9 in order and look
      for the first counter that is non-zero.

  "What is RSS / RPS / RFS?"
      RSS  — NIC-level hashing to multiple hardware queues (layers 1–3).
      RPS  — Software equivalent in the kernel when the NIC has one queue (layer 4).
      RFS  — Flow-aware steering that routes packets to the same core as
             the application consuming them (layer 10).

  "What is busy-poll?"
      The application spins inside the kernel reading the NIC ring directly,
      bypassing the softirq and wakeup path. Compresses layers 4, 5, and 10
      into a single tight polling loop. Enabled via SO_BUSY_POLL.

  "Why bypass the kernel entirely?"
      Kernel bypass (OpenOnload, DPDK, AF_XDP) eliminates layers 4 through 10
      by moving ring buffer access into userspace. The application reads
      packet data directly from DMA memory without any kernel involvement
      after initial setup.