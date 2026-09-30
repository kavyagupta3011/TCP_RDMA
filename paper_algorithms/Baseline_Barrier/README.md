# Baseline: Explicit-Barrier AllReduce (not one of the paper's 5 techniques)

This folder is the control group. It is not a row in Table I -- it exists
so the other five folders (`LL/`, `Sentinel/`, `Twoshot_LL/`,
`Twoshot_Sentinel/`, `LL128_Atomic/`) have something to be measured
*against*, which is what "how did the paper improve latency" actually
means: an improvement is only a number if you also have the "before."

## The idea, technically

This is the design the paper's Section III-B examines and then replaces:
push data to peers, then use an **explicit barrier** to confirm every push
has landed before anyone is allowed to read, then reduce. No packed flag,
no sentinel polling -- correctness here comes entirely from the barrier,
not from anything about the transfer itself.

The paper measures its own barrier's cost directly (Fig. 3): a
device-side primitive (`ncclLsaBarrierSession`) costing roughly 0.85-1.7us
depending on GPU count, and it flags that on a ~5us small-message AllReduce,
two such barrier calls can eat up to ~40% of total latency. That
measurement is the entire motivation for the other five techniques in this
project.

**Read this before comparing numbers:** this file's barrier is a **host-side**
one (`cudaStreamSynchronize()` on every GPU's stream, between the push
kernels and the reduce kernels) rather than the paper's device-side
primitive, for the same reason explained in the top-level README (that
device-side API is experimental/not reliably available). A host round-trip
is architecturally the same *kind* of cost -- everyone stops and waits --
but it will typically be slower in absolute terms than the paper's own
Fig. 3 numbers. So: don't quote this file's absolute latency as "the
paper's barrier cost." Do use it for the relative comparison it's built
for -- barrier-based vs. barrier-free, on your own hardware, at the same
message sizes.

## The idea, simply

Two people pass notes, but this time, instead of the note itself signaling
"I'm done writing," person A has to explicitly shout "ready!" and wait for
person B to shout "ready!" back before either one is allowed to read what's
in front of them -- even if the note actually landed on the table a moment
ago. All five other folders in this project remove that shout-and-wait step
entirely; this one keeps it, on purpose, so you can measure what it costs.

## Build & run

```
make
./baseline_barrier_allreduce [numGPUs] [numFloats] [iters]
./baseline_barrier_allreduce 4 65536 100
```

Run this with the same `numGPUs` and `numFloats` as any of the other five
folders and diff the two "Average ... latency" lines -- that difference,
on your actual node, is your own empirical version of the paper's Fig. 1
speedup claim.
