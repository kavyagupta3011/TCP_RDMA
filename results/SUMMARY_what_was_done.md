# Project summary: low-latency AllReduce over InfiniBand RDMA

Status as of 2026-10-05. This file summarises what was implemented, what was found, how the code was
optimised, and what the measurements say. Detailed per-run numbers are in the other files in `results/`.

---

## 1. Goal and context

The paper being implemented is **"Every µs Matters: Achieving Near Speed-of-Light Latency in GPU
Collectives"** (arXiv:2607.16100).

- **Problem.** Tensor-parallel LLM inference does a tiny AllReduce (sum across GPUs) after every
  attention block and every MLP block, for every generated token. For small messages the cost is
  dominated by *latency*, not bandwidth.
- **Finding of the paper.** A large share of that latency is the **memory barrier** ("everyone says
  ready, everyone waits for everyone"), measured at over 1 µs per barrier, with two barriers per
  AllReduce (about 40% of a 5 µs operation).
- **Solution of the paper.** Remove the barriers using four ideas:
  - **LL**: pack a flag with the data so the receiver can tell when the data is valid.
  - **Sentinel**: pre-fill the receive buffer with an impossible value and poll until it changes.
  - **Double buffering + bidirectional exchange**: alternate two buffers so no barrier is needed
    between rounds.
  - **LL128 Atomic** (their own algorithm): use hardware atomic adds into one cache line, with a hidden
    counter slot that says when everyone has contributed.
- **Algorithms covered** (Table I of the paper): one-shot (every rank sends everything to everyone) and
  two-shot (ReduceScatter then AllGather, less traffic but one more synchronisation step).

**Why this implementation looks different from the paper.** The paper targets GPUs on NVLink. Our
cluster (`172.16.201.12`) has **no GPUs**, only 8 CPU compute nodes with Mellanox ConnectX-3
InfiniBand. So the five techniques were re-implemented with **RDMA verbs** (`paper_algorithms_v2/`).
The key substitution: InfiniBand has no atomic 16-byte store, but a Reliable Connection (RC)
queue pair guarantees that operations posted on the same QP are applied to remote memory **in the
order they were posted**. So "write the data, then write a small flag on the same QP" gives the
receiver the same guarantee the paper gets from NVLink atomicity. (`paper_algorithms/` is a separate
CUDA version that has never been run, since no GPU was available.)

**Test setup for all results below:** 3 ranks on `compute00/01/02`, RDMA verbs over IPoIB
`10.1.2.10-12`, 1000 iterations per measurement (100 in the very first runs), message sizes from
128 B to 256 KiB, latency measured per iteration and reported as mean / median / p99 / max.

---

## 2. What was in the repository when we started

`paper_algorithms_v2/` had seven programs, one per technique:

| Folder | Algorithm |
|---|---|
| `Baseline_Barrier` | conventional one-shot, with an explicit two-sided barrier (the "before") |
| `LL` | one-shot, data write then flag write |
| `Sentinel` | one-shot, sentinel polling |
| `Twoshot_LL` | ReduceScatter + AllGather, flag signalling |
| `Twoshot_Sentinel` | ReduceScatter + AllGather, sentinel signalling |
| `LL128_Atomic` | ReduceScatter + AllGather, completion via RDMA atomic counter |
| `Bidirectional_DoubleBuffering` | chunked 2-rank exchange (the paper's Fig. 4) |

Plus `common/rdma_common.c/.h` (connection setup, full-mesh bootstrap, helpers). Only two results
existed, from a teammate: **Sentinel ≈ 57,800 µs** and **Two-shot Sentinel ≈ 610 µs** (3 ranks, 256 KiB).

Those numbers were suspicious: the paper reports single-digit µs, and a 95x gap between one-shot and
two-shot does not fit the algorithms (at 3 ranks two-shot only moves about 3x less data).

---

## 3. Correctness bugs found and fixed

Reading the code revealed bugs that would have made results meaningless. They were found by code
review and then confirmed by behaviour on the cluster where possible.

### 3.1 Sentinel and Two-shot Sentinel never re-armed the sentinel (the main one)
- **Bug.** Receive buffers were filled with the sentinel *once* at setup. After rounds 0 and 1 both
  buffers held real data from earlier rounds, so from round 2 on the "wait until the value changes"
  loop returned immediately, without waiting for the peer. The verification still passed because every
  round sent identical input.
- **Fix.** After a value is consumed it is written back to the sentinel. This is safe because a peer
  cannot write round *i+2* into a buffer until it has received our round *i+1* data, which we only
  post after finishing round *i*.
- **Effect.** One-shot Sentinel at 256 KiB went from 57,800 µs to **542 µs**; Two-shot Sentinel from
  610 µs to **472 µs**. The old numbers did not measure a real exchange and should not be quoted.

### 3.2 Single receive buffer reused every round (LL, Twoshot_LL, LL128_Atomic, Baseline)
- **Bug.** One receive buffer per peer was reused every round, so a fast peer's next-round write could
  overwrite data that was still being read. This was invisible because every round used identical input.
- **Fix.** Buffers are double-buffered by round parity, like Sentinel already was.

### 3.3 Flag/counter waits used `!=`
- **Bug.** `while (flag != epoch)` would spin forever if a peer was already one round ahead (its flag had
  advanced past `epoch`).
- **Fix.** Changed to `while (flag < epoch)`, since flags and counters only increase. A memory fence
  (`__atomic_thread_fence(ACQUIRE)`) was added after each flag wait so the data reads cannot be hoisted
  above it.

### 3.4 LL128_Atomic waited for the wrong counter value
- **Bug.** Each peer increments its *own* per-connection counter once per round, but the code waited for
  `(it+2)*(n-1)`. That is only correct at 2 ranks; at 3 or more it would spin forever. This was found by
  reading the code (the program never reached that point because of 4.1 below).
- **Fix.** The expected value is now `it+2`.

### 3.5 Rare 2.15 s stall in the barrier (Baseline_Barrier)
- **Symptom.** At 256 floats one iteration took 2,151,641 µs, identically on all three ranks, while the
  median was 11 µs. This inflated the mean to 12,521 µs.
- **Likely cause (not proven).** `rdma_barrier()` re-posted its receive only after finishing an
  exchange, so a peer's next barrier message could arrive with no receive posted (a receiver-not-ready
  NAK) and the sender had to wait out a retry timer.
- **Fix.** Two bootstrap receives are now kept posted at all times, each in its own buffer slot.
- **Evidence.** The stall did not recur in the following runs (3 sizes in the diagnostic job, then the
  full sweep, all with 1000 iterations and maximum latencies of 65 µs or less). That is consistent with
  the fix but cannot prove it, since the stall was rare.

---

## 4. Performance optimisations

### 4.1 Non-blocking posts, inline small sends, unsignaled data writes
- **Before.** `rdma_write()` posted one signaled write and then **blocked until its completion**. With N-1
  peers this was N-1 full NIC round trips in sequence, and LL-style programs paid it twice per peer (data
  write plus flag write).
- **After.** New helpers in `common/`: `rdma_post_write()`, `rdma_post_write_ex()`, `rdma_wait_write()`,
  `rdma_post_atomic_add()`, `rdma_wait_atomic()`.
  - Writes are posted to **all peers back to back**.
  - The **data write is unsignaled** and only the trailing flag write (or atomic) is signaled. RC
    completes work requests in order, so that single completion proves both are done.
  - Payloads of up to the inline limit are sent **inline** (copied into the work request, no DMA read of
    the source buffer). The queue-pair now requests 128 B of inline space; the adapter granted 188 B.
  - Completions are collected **after** the reduce, overlapping them with waiting for incoming data.
- **Effect (Sentinel, 3 ranks, same code otherwise):** about **2x faster at 128 B** and 1.2-1.8x
  at larger sizes (table in section 5.2).
- Baseline: writes are posted together and drained, but the **barrier itself is deliberately unchanged**,
  because it is the thing being measured. The writes must be drained before the barrier since both poll the
  same completion queue.

### 4.2 Measurement quality
- Per-iteration timing with min / median / p99 / max (a mean alone hid one-off stalls).
- A barrier before the timed loop (so ranks start together) and one before closing.
- 1000 iterations per run, and 90 s timeouts in the sweep script so a hang cannot block the job.

### 4.3 What was **not** changed
- The reduce loops are still scalar, per-element CPU loops. This is the main remaining cost at large sizes
  (see 6.2).

---

## 4A. Detailed code changes: what each file did BEFORE editing and what it does NOW

This section is the "diff in words". "Before" is the code as received (kept as `*.orig.c` next to each
file); "now" is the current code. Line endings of `LL`, `Twoshot_LL`, `LL128_Atomic`, `Baseline_Barrier` and
`Bidirectional_DoubleBuffering` were also converted from Windows (CRLF) to Unix (LF) line endings while
patching, so a text diff of those five files will look larger than the real change.

### 4A.1 Shared library: `common/rdma_common.c` and `rdma_common.h`

| Area | Before | Now |
|---|---|---|
| Sending a write | Only `rdma_write()`: post one **signaled** work request, then **poll the completion queue until it finishes** before returning. | `rdma_write()` is unchanged (still available), plus **`rdma_post_write()`** (post and return) and **`rdma_wait_write()`** (collect one completion) so a caller can post to every peer first and wait later. |
| Signaled vs unsignaled | Every write was signaled. | **`rdma_post_write_ex(..., signaled)`**: the data write can be posted **unsignaled** and only the trailing flag/atomic is signaled. RC completes work requests in order, so that one completion covers both. |
| Small payloads | Queue pairs were created with `max_inline_data = 0`, so even an 8-byte flag was fetched by the NIC from memory. | The QP requests **128 B of inline data** (the adapter granted 188 B), stored in a new `conn->max_inline` field. Writes no larger than that are sent **inline** (copied into the work request itself). |
| Atomics | Only the blocking `rdma_atomic_fetch_add()` (post, poll, read the old value). | Added **`rdma_post_atomic_add()`** and **`rdma_wait_atomic()`** (non-blocking split; the old value is discarded). The blocking version is kept. |
| Bootstrap receives | **One** receive was posted for the setup/barrier messages and was re-posted only *after* each exchange finished. | **Two receives** are posted at all times, each into its own slot of the receive buffer (new `conn->boot_slot` field). This closes a race where a peer's next barrier message could arrive with no receive posted (a receiver-not-ready NAK and a long retry delay; see 3.5). |
| Error reporting | Failed posts printed `errno`, which `ibv_post_send()` does not set (it *returns* the error number), so the printed value was stale and misleading. | The post functions print the **real return code** and the request details (this is how `EINVAL` on atomics was diagnosed). |
| Timing output | Each program computed its own total and printed only an average. | New shared **`rdma_print_stats()`** prints mean, min, median, p99 and max from per-iteration measurements. |

### 4A.2 `Sentinel/sentinel_allreduce_rdma.c` (one-shot Sentinel)

Before:
```c
// setup: both receive buffers filled with the sentinel ONCE
for each round:
    for j: rdma_write(...)              // blocking: one full NIC round trip per peer, in sequence
    for i: for j: while (is_sentinel(slot[i])) {}   sum += slot[i];   // wait, then read
    total_us += elapsed;                // only the average was printed
```
- The sentinel was **never put back**, so after round 1 both buffers held stale real data and the wait loop
  returned immediately (see 3.1). Timings measured no real exchange.
- Writes to the N-1 peers were serialised.
- No start barrier (ranks could begin the timed loop at different times) and only a mean was reported.

Now:
```c
barrier with every peer                         // ranks start together
for each round:
    for j: rdma_post_write(...)                 // all peers back to back, small payloads inline
    for i: for j: while (is_sentinel(slot[i])) {}   sum += slot[i];
                                                    slot[i] = sentinel;   // RE-ARM after consume
    for j: rdma_wait_write(...)                 // collect completions AFTER the reduce
    lat[it] = elapsed;
rdma_print_stats(...);   barrier with every peer before closing
```

### 4A.3 `Twoshot_Sentinel/twoshot_sentinel_allreduce_rdma.c`

Same three changes as 4A.2, applied to both phases:
- **Before:** phase-1 and phase-2 writes were blocking and serial per peer; neither the ReduceScatter buffer nor
  the AllGather buffer was ever re-armed.
- **Now:** phase-1 writes are posted together; after the reduce the phase-1 completions are collected (so only
  one signaled write per connection is outstanding); phase-2 writes are posted together; both buffers are
  re-armed to the sentinel as each value is consumed; phase-2 completions are collected at the end.

### 4A.4 `LL/ll_allreduce_rdma.c` (one-shot LL)

Before:
```c
for each round:
    for j:  *epoch_send[j] = epoch;
            rdma_write(data -> data_recv[j]);       // blocking
            rdma_write(epoch -> flag_recv[j]);      // blocking, a second round trip per peer
    for j:  while (*flag != epoch) {}               // equality wait
    reduce from data_recv[j][i]                     // ONE receive buffer per peer, reused every round
```
- **Four serialised blocking writes per round at 3 ranks** (data + flag, times two peers).
- **One receive buffer per peer reused every round:** a fast peer's next-round data could overwrite what this
  rank was still reading (hidden because the input never changed).
- **`!=` wait** could spin forever if a peer had already advanced to the next round (3.3).

Now:
```c
par = epoch & 1;                                    // receive buffers are double-buffered by round parity
for j:  *epoch_send[j] = epoch;
        rdma_post_write_ex(data -> data_recv[j][par], signaled = 0);   // unsignaled data write
        rdma_post_write(epoch   -> flag_recv[j]);                      // signaled, 8-byte, INLINE
for j:  while (*flag < epoch) {}                    // monotonic comparison
fence(ACQUIRE);  reduce from data_recv[j][par][i]
for j:  rdma_wait_write(...)                        // one completion per peer, after the reduce
```
Result: no blocking in the send path, a single completion per peer instead of two, safe buffer reuse.

### 4A.5 `Twoshot_LL/twoshot_ll_allreduce_rdma.c`

- **Before:** per phase, blocking data write plus blocking flag write per peer; one buffer reused every round
  for both the ReduceScatter and AllGather data; `!=` waits.
- **Now:** the same pattern as LL, in both phases: unsignaled data write + signaled inline flag; the
  ReduceScatter and AllGather receive buffers are each double-buffered by round parity (six regions exchanged at
  setup instead of four); `<` waits with a fence; phase-1 completions are collected before phase 2 and
  phase-2 completions at the end.

### 4A.6 `LL128_Atomic/ll128_atomic_allreduce_rdma.c`

- **Before:** per peer, a blocking data write followed by a blocking atomic fetch-and-add (post, poll, read old
  value); `expect = (it+2)*(n-1)` with a `!=` wait; single buffers.
  - The `expect` value was wrong for 3 or more ranks: each peer adds 1 to **its own** counter per round, so the
    counter equals `it+2`, and waiting for `(it+2)*(n-1)` would never finish (3.4).
- **Now:** data write unsignaled + non-blocking atomic add (signaled); `expect = it+2`; `<` waits with a fence;
  receive buffers double-buffered by parity; completions collected with `rdma_wait_atomic()`.
- **New:** at start-up the program checks the adapter's atomic capability. On this cluster it is `NONE`, so it
  prints `SKIPPED` and exits with code 2 (the atomic cannot be posted at all; see 6.4).

### 4A.7 `Baseline_Barrier/baseline_barrier_rdma.c`

- **Before:** blocking write per peer (serial), then a barrier per peer, then the reduce; one receive buffer
  reused every round.
- **Now:** writes posted to all peers together and then **drained**; then the **barrier, unchanged** (it is the
  cost being measured, so it was deliberately not optimised); then the reduce, reading a receive buffer that is
  double-buffered by round parity. The writes must be drained *before* the barrier because the barrier polls
  the same completion queue and would otherwise swallow a write completion.
- This makes the baseline a **fairer** comparison: the only remaining difference to the barrier-free
  algorithms is the barrier itself, not slower write posting.

### 4A.8 `Bidirectional_DoubleBuffering/bidir_doublebuffer_rdma.c`

- **Before:** per chunk, a blocking data write and a blocking flag write, then a `!=` wait; a mean over iterations
  only.
- **Now:** per chunk, an unsignaled data write and a signaled inline flag write; `<` wait with a fence; one
  completion collected after the chunk is reduced; a barrier before the timed loop and before closing;
  per-iteration statistics. It still runs on exactly 2 ranks.

### 4A.9 New diagnostic program: `Atomic_Probe/atomic_probe.c`
Not an algorithm. It prints the adapter's atomic capability and the queue pair's state/access flags, then tries
four variants of an atomic post and prints each raw return code. It established that the adapter reports
`atomic_cap=0 (NONE)` and rejects every atomic with `EINVAL`.

### 4A.10 Cross-cutting changes applied to every algorithm

| Change | Why |
|---|---|
| Per-iteration timing, reporting mean / min / median / p99 / max | A mean hid one-off stalls (the 2.15 s barrier stall, for example) |
| Barrier with every peer before the timed loop | Ranks start the loop together; connection-setup skew no longer lands in the first timed round |
| Barrier with every peer before closing | A rank does not tear down its connections while a peer is still in its last round |
| Receive buffers double-buffered by round parity | A peer's next-round write can no longer overwrite data still being read |
| `<` instead of `!=` on flags and counters | A peer that is already a round ahead no longer causes an infinite wait |
| Acquire fence after each flag wait | The compiler cannot move the data reads above the flag check |
| Unsignaled data write + one signaled inline flag | One completion per peer instead of two, and no NIC fetch for the small flag |
| Post to all peers, then wait | N-1 network round trips are overlapped instead of paid one after another |

### 4A.11 What this means in practice
- The **correctness fixes** (re-arm, double buffering, `<` waits, the LL128 counter value, the barrier receive
  race) make the measurements trustworthy. They are not speed-ups; the old numbers could not be relied on.
- The **speed-up** comes from the post/wait + inline change. It was measured end to end only for Sentinel and
  Two-shot Sentinel (section 5.2); for the other algorithms no pre-optimisation numbers exist, so no
  before/after can be claimed for them.
- **Not changed:** the CPU reduce loops (per-element, scalar). They are the main remaining cost at large message
  sizes and the likely reason Two-shot Sentinel trails Two-shot LL (see 6.2).

---

## 5. Results

### 5.1 First valid result after the Sentinel fix (256 KiB messages, 3 ranks, 100 iters)

| Algorithm | Original (invalid) | After re-arm fix |
|---|---|---|
| Sentinel | ~57,800 µs | ~542 µs |
| Two-shot Sentinel | ~610 µs | ~472 µs |

At this size latency is dominated by moving 256 KiB (and the CPU reduce loop), not by synchronisation, so
it does not compare with the paper's small-message numbers.

### 5.2 Effect of the post/wait + inline optimisation (Sentinel and Two-shot Sentinel only)

Mean latency, µs, average of 3 ranks. "Before" is the re-arm-fixed code with blocking writes.

| Size | Sentinel before | Sentinel after | Speed-up | Two-shot Sent. before | Two-shot Sent. after | Speed-up |
|---|---|---|---|---|---|---|
| 128 B | 4.28 | 2.08 | 2.06x | 8.21 | 3.87 | 2.12x |
| 1 KiB | 7.00 | 4.07 | 1.72x | 10.01 | 5.58 | 1.79x |
| 4 KiB | 12.70 | 10.26 | 1.24x | 14.49 | 11.55 | 1.25x |
| 16 KiB | 43.5 | 29.8 | 1.46x | 44.1 | 32.5 | 1.35x |
| 64 KiB | 139.7 | 85.1 | 1.64x | 118.8 | 93.5 | 1.27x |

The 2.2 µs saved at 128 B matches removing one serialised round trip. The other algorithms were never
measured before this optimisation, so there is no before/after for them.

### 5.3 Final all-algorithm comparison (current code)

Mean latency, µs, average of 3 ranks, 1000 iterations. Message sizes are floats / bytes.

| Algorithm | 128 B | 1 KiB | 4 KiB | 16 KiB | 64 KiB |
|---|---|---|---|---|---|
| **Baseline (with barrier)** | 8.20 | 10.08 | 16.31 | 37.93 | 121.0 |
| LL (one-shot) | 2.36 | **3.67** | 8.99 | 27.2 | 95.5 |
| Sentinel (one-shot) | **2.10** | 4.05 | 10.2 | 29.0 | 92.0 |
| Twoshot_LL | 4.48 | 5.55 | **8.68** | **22.3** | **73.0** |
| Twoshot_Sentinel | 3.76 | 5.67 | 11.5 | 32.7 | 93.1 |
| LL128_Atomic | not supported on this hardware (see 6.4) | | | | |
| Bidirectional (2 ranks, 8-chunk pass) | 14.2 | 14.1 | 19.7 | 27.1 | 59.8 |

(Bold = best barrier-free at that size. Two-shot rows use 33 / 258 / 1026 / 4098 / 16386 floats because
the message is padded to a multiple of 3. Bidirectional uses 2 ranks and times a whole 8-chunk pass, so it
is not comparable to the others; per chunk it is about 1.8 µs at 128 B.)

All runs that executed printed `Correctness check: PASSED` on every rank.

---

## 6. What the results mean

### 6.1 The paper's central claim holds on InfiniBand
Removing the explicit barrier saves a **fixed ~6 µs** per AllReduce here. The best barrier-free
algorithm beats the barrier baseline by:

| Size | Best barrier-free | Speed-up over baseline |
|---|---|---|
| 128 B | Sentinel, 2.10 µs | **3.9x** |
| 1 KiB | LL, 3.67 µs | 2.75x |
| 4 KiB | Twoshot_LL, 8.68 µs | 1.88x |
| 16 KiB | Twoshot_LL, 22.3 µs | 1.70x |
| 64 KiB | Twoshot_LL, 73.0 µs | 1.66x |

The relative win is largest for tiny messages (the LLM decode case the paper targets) and shrinks as
data movement dominates.

### 6.2 One-shot vs two-shot
- **LL:** one-shot wins below ~4 KiB, two-shot wins above (two-shot is 0.76x the one-shot time at
  64 KiB). This matches the paper's description.
- **Sentinel:** one-shot wins or ties at every size tested.
- **Twoshot_Sentinel is much slower than Twoshot_LL** at larger sizes (93 vs 73 µs at 64 KiB; 33 vs 22 µs
  at 16 KiB). *Hypothesis, not measured:* the sentinel path checks and re-arms every float on the CPU,
  while LL waits on a single flag word. This is the largest remaining optimisation target.

### 6.3 LL vs Sentinel
Sentinel is faster at 128 B and 64 KiB; LL is faster from 1 KiB to 16 KiB. This does not exactly follow the
paper's "LL for tiny, Sentinel for larger" ordering. Our LL is a flag write after the data (relying on RC
ordering), not the paper's single 16-byte atomic flag+data store, so the mechanisms differ.

### 6.4 LL128_Atomic cannot run on this cluster
- Atomic_Probe (a small diagnostic program) reported **`atomic_cap = NONE`**: this ConnectX-3 adapter /
  firmware does not support RDMA atomics. All four tested atomic variants (fetch-and-add with the library
  buffer, with a fresh buffer, compare-and-swap, fetch-and-add after an inline write) returned `EINVAL`.
  The QP itself was fine (state RTS, remote-atomic access enabled).
- So this is a **hardware limit, not a code bug**. `LL128_Atomic` now detects this at start-up, prints a
  clear `SKIPPED` message and exits with code 2 instead of crashing.
- Even on atomic-capable InfiniBand, only the completion *counter* could be an atomic. IB atomics work on
  64-bit integers only, so the floating-point sums would still be done on the CPU. The paper's
  hardware-summed-float property is not reproducible over InfiniBand.

### 6.5 Honesty notes for the meeting
- The paper's 1.404 µs "speed-of-light" is an **NVLink / GPU L2** figure and is **not a valid target** for
  this CPU/InfiniBand cluster. We have **not yet measured this fabric's own floor** (e.g. `ib_write_lat`),
  so we cannot yet say how close 2.1 µs is to the hardware limit.
- The 128 B one-shot Sentinel at ~2.1 µs is the best result so far; quote median/p99 alongside the mean,
  since there are occasional outliers (about 10-20 µs maximum at small sizes).
- The cause of the old 57.8 ms Sentinel figure is not known. The fixed code does not reproduce it, and the old
  code never actually waited for data.
- The barrier-stall explanation (3.5) is a likely cause, not a proven one.

---

## 7. Files changed or added

Code (`paper_algorithms_v2/`):
- `common/rdma_common.c/.h`: inline-send support (`max_inline`), `rdma_post_write`, `rdma_post_write_ex`,
  `rdma_wait_write`, `rdma_post_atomic_add`, `rdma_wait_atomic`, `rdma_print_stats`; two-slot bootstrap
  receives (`boot_slot`); error messages now print the real `ibv_post_send` return code.
- `Sentinel/`, `Twoshot_Sentinel/`: re-arm fix, post/wait + inline, per-iteration stats, barriers.
- `LL/`, `Twoshot_LL/`, `LL128_Atomic/`, `Baseline_Barrier/`, `Bidirectional_DoubleBuffering/`:
  parity double-buffering, `<` waits, fences, post/wait + inline, unsignaled data writes, per-iteration
  stats, start/end barriers; `LL128_Atomic` also gets the corrected counter value and the no-atomics check.
- `Atomic_Probe/atomic_probe.c` (new): diagnostic only.
- Backups of earlier versions sit next to each file: `*.orig.c` (as received), `*.fixed_v1.*` (re-arm fix
  only), `*.fixed_v2.*` and `*.fixed_v3.*` (common code before later steps).

Run script (repo root): **one** script, `run_all.pbs`, replaces the five single-purpose scripts that were used
during the investigation (`run_sentinel`, `run_sentinel_sizes`, `run_all_sizes`, `run_diag`, `run_probe`).
What to run is chosen at submit time with environment variables (`PROGRAMS`, `SIZES`, `ITERS`, `TIMEOUT`,
`BASE`; lists are colon-separated) and the rank count with `-l nodes=N`. The usage block at the top of the file
has examples, e.g. the original 256 KiB run is `qsub -v SIZES=65536,ITERS=100 run_all.pbs` and the atomic
diagnostic is `qsub -v PROGRAMS=Atomic_Probe run_all.pbs`. Every run has a hard timeout (a hang shows as
`exit=124`), and result lines are filtered so the output file stays readable.

Results (`results/`):
- `2026-10-05_sentinel_fix_*`: first valid runs after the re-arm fix.
- `2026-10-05_small_size_sweep.txt` and `..._v2_postwrite.txt`: Sentinel/Two-shot Sentinel before and after
  the post/wait optimisation.
- `2026-10-05_all_algorithms_partial_and_findings.txt`: LL128 finding and bug findings.
- `2026-10-05_all_algorithms_size_sweep.txt`: the complete comparison table with medians and observations.

---

## 8. How to reproduce

1. Copy the changed files to the cluster with `scp` (destination `student4@172.16.201.12:~/IMT2023016/...`).
2. On the cluster: `sed -i 's/\r$//' run_all.pbs` (the script must have Unix line endings), then build
   all seven: `for d in LL Sentinel Twoshot_LL Twoshot_Sentinel LL128_Atomic Baseline_Barrier Bidirectional_DoubleBuffering; do (cd paper_algorithms_v2/$d && make -B); done`.
3. `qsub run_all.pbs` (add `-v PROGRAMS=...,SIZES=...` to run a subset); wait until `qstat -u student4` no longer lists the job; read the `.out` file
   named in the `qsub` output (under `/home/student4/.pbs_route/output/`).

---

## 9. Suggested next steps

1. **Scale beyond 3 ranks** (the cluster has 8 compute nodes). Two-shot's traffic advantage should grow with
   the rank count, which is the scaling behaviour the paper reports. Needs no code changes.
2. **Optimise the Sentinel CPU loops** (peer-major streaming, one check per cache line, batched re-arm) to
   close the gap between Twoshot_Sentinel and Twoshot_LL.
3. **Measure the InfiniBand floor** with `ib_write_lat` to state how close the results are to the fabric's limit.
4. **Decide on an LL128 emulation** using RDMA write-with-immediate as the hardware-delivered completion
   signal. It would not be the paper's mechanism (no atomic add) and would have to be labelled as an emulation.
