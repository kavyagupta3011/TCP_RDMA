// ll128_atomic_allreduce.cu
//
// Two-shot LL128 Atomic AllReduce, Section IV.B.4 and Fig. 5 of "Every us
// Matters: Achieving Near Speed-of-Light Latency in GPU Collectives"
// (arXiv:2607.16100). This is the paper's own novel contribution, and the
// most involved of the four techniques.
//
// Mechanism (paper, Fig. 5, transcribed step by step):
//
//   Phase 1 -- ReduceScatter. The input is partitioned into N chunks, one
//   owned ("target rank") per GPU. Within a chunk, threads work in groups
//   of 8 on one 128-byte cache line at a time (for FP32: 8 threads x 4
//   elements x 4 bytes = 128 bytes). Per group:
//     (1) each of the 8 threads loads its 4 assigned elements (e0..e3);
//     (2) the group's first thread ("flag carrier") moves ITS e0 into a
//         shared-memory region reserved for "displaced" elements, then
//         __syncthreads();
//     (3) a separate, small pool of "extra threads" (16, regardless of how
//         many groups there are, per the paper's FP32 example) reads those
//         displaced e0 values back out of shared memory and atomically adds
//         each one into its own dedicated scratch slot on the TARGET rank's
//         GPU (across NVLink, via a P2P pointer) -- this is the real e0
//         value, reduced, but kept separate from the flag/counter below;
//     (4) each flag carrier sets the first element of ITS OWN vector to 1
//         (repurposing the slot that used to hold e0, now that e0's real
//         value is safely on its way via the extra thread);
//     (5) all 8 threads atomically add their vector into the target rank's
//         scratch: the flag carrier's contribution of "1" accumulates into
//         a per-group COUNTER slot (reaching exactly N once every rank has
//         contributed), while all 8 threads' real e1..e3 values accumulate
//         into 3 per-group DATA slots.
//   NVLink's cache-line-atomic guarantee is what the paper leans on so that
//   concurrent atomic adds from different ranks land correctly; standard
//   CUDA atomicAdd on 4-byte floats is what we use to express each of the
//   4 lanes of that add (see the "Faithfulness" note in the README for why
//   this is an honest, documented simplification of "one 128-byte atomic").
//
//   Phase 2 -- AllGather. Each target rank's own CTA polls ITS OWN (local!)
//   counter slot per group until it equals N -- no remote read needed here,
//   because every contribution already landed locally via Phase 1's push-
//   atomics. Once ready, the CTA reconstructs the true 4-element vector
//   (e0 from the displaced slot, e1..e3 from the data slots) and pushes the
//   completed chunk out to every peer's output buffer (this rank is now the
//   SOLE writer for this chunk, so a plain LL-style flagged push -- no
//   atomics needed -- is enough); peers poll for arrival exactly as in LL.
//
// Table I's numbers for this algorithm (N GPUs, M total elements, D =
// M/N data reduced per iteration): scratch space is only D/N per GPU (vs.
// 2*N*D for one-shot LL) -- because only 1 rank's worth of extra bytes
// (the counter + a handful of displaced-element slots) is needed per group,
// not N copies -- and it costs 4 extra bytes per 128 for FP32 (~3%
// bandwidth overhead) instead of LL's 100% overhead. The tradeoff, also
// from Table I, is that it is the only one of the four techniques that is
// NON-deterministic: floating-point addition is not associative, and the
// order in which N ranks' atomicAdds land is not fixed, so repeated runs
// (or different ranks) can see very slightly different rounding of the sum.
// We verify against the CPU reference with a relative tolerance for exactly
// this reason (see verify() in gpu_common.cuh).
//
// Build:   make            (see Makefile; requires 2+ NVLink/P2P GPUs)
// Run:     ./ll128_atomic_allreduce [numGPUs] [chunkElemsPerRank] [iters]
//           chunkElemsPerRank must be a multiple of 32 (= 8 threads x 4
//           FP32 elements, i.e. one 128-byte cache line per group) and the
//           whole per-target CTA (chunkElemsPerRank/32 * 8 regular threads
//           + up to 16 extra threads) must fit in 1024 threads/block, i.e.
//           chunkElemsPerRank <= 4064. Total AllReduce size = N * chunkElemsPerRank.

#include "../common/gpu_common.cuh"

#define GROUP_SIZE 8            // threads per 128-byte-cache-line group (paper, Fig. 5)
#define ELEMS_PER_GROUP 32      // 8 threads x 4 FP32 elements = 32 elements = 128 bytes
#define ELEMS_PER_THREAD 4
#define MAX_EXTRA_THREADS 16    // paper's fixed extra-thread pool size for FP32

// AllGather packet: one FP32 value + a duplicated-free 4-byte flag, written
// as a single aligned 8-byte store/load -- same "pack flag with data, poll
// without a barrier" idea as LL, used here for the AllGather leg (the
// second of the two Table-I-listed synchronizations for this algorithm).
struct __align__(8) AGLine {
    float data;
    unsigned int flag;
};

// ---------------------------------------------------------------------
// Phase 1: ReduceScatter.
// Grid: N CTAs (blockIdx.x = target rank r this CTA contributes to).
// Launched on every source GPU `self`; each CTA atomically adds this GPU's
// contribution for target r's chunk into target r's (remote, P2P) scratch.
// Block layout: [0, regularThreads) regular threads (groups of 8),
//               [regularThreads, regularThreads+extraThreads) extra threads.
// ---------------------------------------------------------------------
__global__ void ll128ReduceScatterKernel(
    const float* __restrict__ input,           // this source GPU's full M-element input
    float** __restrict__ regularScratchPtrs,   // [N] P2P ptrs; regularScratchPtrs[r] -> target r's
                                                // own [groupsPerTarget*4] float buffer
    float** __restrict__ displacedScratchPtrs, // [N] P2P ptrs; displacedScratchPtrs[r] -> target r's
                                                // own [groupsPerTarget] float buffer
    int self, int chunkElems, int groupsPerTarget, int regularThreads, int extraThreads) {
    int r = blockIdx.x;
    int tid = threadIdx.x;
    extern __shared__ float sDisplaced[];  // groupsPerTarget floats, this CTA's own scratch

    size_t base = (size_t)r * chunkElems;  // target r's chunk lives at this offset in `input`

    bool isRegular = tid < regularThreads;
    int group = isRegular ? (tid / GROUP_SIZE) : -1;
    int lane = isRegular ? (tid % GROUP_SIZE) : -1;
    float v0 = 0.f, v1 = 0.f, v2 = 0.f, v3 = 0.f;

    if (isRegular) {
        int idx = group * ELEMS_PER_GROUP + lane * ELEMS_PER_THREAD;
        v0 = input[base + idx + 0];
        v1 = input[base + idx + 1];
        v2 = input[base + idx + 2];
        v3 = input[base + idx + 3];
        if (lane == 0) {
            // Step (2): flag carrier moves its real e0 into shared memory
            // before repurposing its atomic contribution below.
            sDisplaced[group] = v0;
        }
    }
    __syncthreads();  // called unconditionally by the whole block (regular + extra threads)

    if (tid >= regularThreads && tid < regularThreads + extraThreads) {
        // Step (3): the small extra-thread pool reads displaced e0 values
        // back out of shared memory and reduces them into the target's
        // dedicated displaced-element scratch, across NVLink via a P2P
        // atomic add. Looped so this generalizes beyond the paper's fixed
        // 16-extra-thread FP32 example to any groupsPerTarget.
        int extraId = tid - regularThreads;
        float* displacedDst = displacedScratchPtrs[r];
        for (int g = extraId; g < groupsPerTarget; g += extraThreads) {
            atomicAdd(&displacedDst[g], sDisplaced[g]);
        }
    }

    if (isRegular) {
        // Steps (4)+(5): flag carrier's slot-0 contributes to the per-group
        // COUNTER (reaches N once every rank has added); all 8 lanes'
        // e1..e3 contribute to the per-group DATA slots. Four separate
        // 4-byte atomicAdds stand in for the paper's single cache-line
        // atomic (see README "Faithfulness" note).
        float* slot = &regularScratchPtrs[r][group * 4];
        if (lane == 0) {
            atomicAdd(&slot[0], 1.0f);  // counter contribution, NOT real data
        }
        atomicAdd(&slot[1], v1);
        atomicAdd(&slot[2], v2);
        atomicAdd(&slot[3], v3);
    }
}

// ---------------------------------------------------------------------
// Phase 2: AllGather.
// Launched on GPU `self` for its OWN chunk (rank == self): polls its own
// LOCAL regular/displaced scratch until every group's counter == N, then
// pushes the reconstructed, fully-reduced chunk to every peer's output
// (flagged so peers can poll without a barrier, exactly like LL).
// ---------------------------------------------------------------------
__global__ void ll128AllGatherKernel(const float* __restrict__ regularScratch,   // local
                                      const float* __restrict__ displacedScratch,  // local
                                      AGLine** __restrict__ outPeerPtrs,  // [N] P2P ptrs to
                                                                          // peers' output arrays
                                      AGLine* __restrict__ myOutput,      // this GPU's own output
                                      int self, int n, int chunkElems, int rankOwned,
                                      unsigned int flagVal) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;  // element index within this chunk
    if (idx >= chunkElems) return;

    int group = idx / ELEMS_PER_THREAD;
    int pos = idx % ELEMS_PER_THREAD;

    // Local poll: every rank's contribution to this group already landed
    // via atomicAdd in Phase 1, so this is a plain local spin, no remote
    // traffic -- this is the paper's "each CTA whose target rank matches
    // its own rank polls the corresponding region in scratch buffer."
    volatile const float* counterPtr = &regularScratch[group * 4 + 0];
    while (*counterPtr != (float)n) { /* spin */
    }

    float val = (pos == 0) ? displacedScratch[group] : regularScratch[group * 4 + pos];

    size_t globalIdx = (size_t)rankOwned * chunkElems + idx;
    AGLine line;
    line.data = val;
    line.flag = flagVal;

    myOutput[globalIdx] = line;  // this rank's own copy
    for (int p = 0; p < n; ++p) {
        if (p == self) continue;
        outPeerPtrs[p][globalIdx] = line;  // push to every peer (P2P store)
    }
}

// Every GPU runs this over the FULL M-element output once all N chunks'
// AllGather kernels (one per owning rank) have been launched: polls each
// slot for the current flag (already true for this rank's own chunk, since
// it wrote that with the matching flag itself) and extracts the value.
__global__ void ll128FinalizeKernel(const AGLine* __restrict__ output, float* __restrict__ result,
                                     size_t m, unsigned int flagVal) {
    size_t i = (size_t)blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= m) return;
    volatile const AGLine* src = output + i;
    AGLine line;
    do {
        line.data = src->data;
        line.flag = src->flag;
    } while (line.flag != flagVal);
    result[i] = line.data;
}

int main(int argc, char** argv) {
    int nGpusRequested = (argc > 1) ? atoi(argv[1]) : -1;
    int chunkElems = (argc > 2) ? atoi(argv[2]) : 1024;  // per-target-rank chunk size
    int iters = (argc > 3) ? atoi(argv[3]) : 50;

    if (chunkElems % ELEMS_PER_GROUP != 0) {
        chunkElems = ((chunkElems / ELEMS_PER_GROUP) + 1) * ELEMS_PER_GROUP;
    }
    int groupsPerTarget = chunkElems / ELEMS_PER_GROUP;
    int regularThreads = groupsPerTarget * GROUP_SIZE;
    int extraThreads = std::min(groupsPerTarget, MAX_EXTRA_THREADS);
    int blockThreads = regularThreads + extraThreads;
    if (blockThreads > 1024) {
        fprintf(stderr,
                "chunkElemsPerRank=%d needs %d threads/block (> 1024 CUDA max). Use a "
                "smaller chunkElemsPerRank (<= 4064). See file header for the formula.\n",
                chunkElems, blockThreads);
        return 1;
    }

    int N = enablePeerAccessAll(nGpusRequested);
    size_t M = (size_t)N * chunkElems;
    printf(
        "Two-shot LL128 Atomic AllReduce: N=%d GPUs, chunk=%d floats/rank (M=%zu floats "
        "total), %d groups/target, block=%d threads (%d regular + %d extra)\n",
        N, chunkElems, M, groupsPerTarget, blockThreads, regularThreads, extraThreads);

    std::vector<std::vector<float>> hostInputs;
    for (int g = 0; g < N; ++g) hostInputs.push_back(makeRandomVector(M, 4000 + g));
    std::vector<float> ref = referenceAllReduceSum(hostInputs);

    std::vector<float*> dInput(N);
    std::vector<float*> dRegularScratch(N), dDisplacedScratch(N);
    std::vector<float**> dRegularPtrTable(N), dDisplacedPtrTable(N);
    std::vector<AGLine*> dOutput(N);
    std::vector<AGLine**> dOutPtrTable(N);
    std::vector<float*> dResult(N);
    std::vector<cudaStream_t> stream(N);

    for (int g = 0; g < N; ++g) {
        CUDA_CHECK(cudaSetDevice(g));
        CUDA_CHECK(cudaMalloc(&dInput[g], M * sizeof(float)));
        CUDA_CHECK(cudaMemcpy(dInput[g], hostInputs[g].data(), M * sizeof(float),
                               cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMalloc(&dRegularScratch[g], (size_t)groupsPerTarget * 4 * sizeof(float)));
        CUDA_CHECK(cudaMalloc(&dDisplacedScratch[g], (size_t)groupsPerTarget * sizeof(float)));
        CUDA_CHECK(cudaMalloc(&dOutput[g], M * sizeof(AGLine)));
        CUDA_CHECK(cudaMemset(dOutput[g], 0, M * sizeof(AGLine)));  // flag=0 initially
        CUDA_CHECK(cudaMalloc(&dResult[g], M * sizeof(float)));
        CUDA_CHECK(cudaStreamCreate(&stream[g]));
    }
    std::vector<float*> hostRegularPtrs(N), hostDisplacedPtrs(N);
    std::vector<AGLine*> hostOutPtrs(N);
    for (int g = 0; g < N; ++g) {
        hostRegularPtrs[g] = dRegularScratch[g];
        hostDisplacedPtrs[g] = dDisplacedScratch[g];
        hostOutPtrs[g] = dOutput[g];
    }
    for (int g = 0; g < N; ++g) {
        CUDA_CHECK(cudaSetDevice(g));
        CUDA_CHECK(cudaMalloc(&dRegularPtrTable[g], N * sizeof(float*)));
        CUDA_CHECK(cudaMemcpy(dRegularPtrTable[g], hostRegularPtrs.data(), N * sizeof(float*),
                               cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMalloc(&dDisplacedPtrTable[g], N * sizeof(float*)));
        CUDA_CHECK(cudaMemcpy(dDisplacedPtrTable[g], hostDisplacedPtrs.data(), N * sizeof(float*),
                               cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMalloc(&dOutPtrTable[g], N * sizeof(AGLine*)));
        CUDA_CHECK(cudaMemcpy(dOutPtrTable[g], hostOutPtrs.data(), N * sizeof(AGLine*),
                               cudaMemcpyHostToDevice));
    }

    size_t sharedBytes = (size_t)groupsPerTarget * sizeof(float);
    const int agThreads = 256;
    const int agBlocks = (chunkElems + agThreads - 1) / agThreads;
    const int finThreads = 256;
    const int finBlocks = (int)((M + finThreads - 1) / finThreads);

    auto runOnce = [&](unsigned int flagVal) {
        // Reset per-target scratch (counter + data + displaced) before each
        // round -- Phase 1 uses atomicAdd, which requires a known-zero
        // starting point, exactly like the paper's ncclLLBufferInitSentinel
        // host-side prep call for buffers that need initialization.
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            CUDA_CHECK(cudaMemsetAsync(dRegularScratch[g], 0,
                                        (size_t)groupsPerTarget * 4 * sizeof(float), stream[g]));
            CUDA_CHECK(cudaMemsetAsync(dDisplacedScratch[g], 0,
                                        (size_t)groupsPerTarget * sizeof(float), stream[g]));
        }
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            CUDA_CHECK(cudaStreamSynchronize(stream[g]));
        }

        // Phase 1: ReduceScatter (N CTAs per source GPU, one per target rank).
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            ll128ReduceScatterKernel<<<N, blockThreads, sharedBytes, stream[g]>>>(
                dInput[g], dRegularPtrTable[g], dDisplacedPtrTable[g], g, chunkElems,
                groupsPerTarget, regularThreads, extraThreads);
        }
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            CUDA_CHECK(cudaStreamSynchronize(stream[g]));
        }

        // Phase 2: AllGather (each GPU handles the chunk it owns, rank == g).
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            ll128AllGatherKernel<<<agBlocks, agThreads, 0, stream[g]>>>(
                dRegularScratch[g], dDisplacedScratch[g], dOutPtrTable[g], dOutput[g], g, N,
                chunkElems, /*rankOwned=*/g, flagVal);
        }
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            CUDA_CHECK(cudaStreamSynchronize(stream[g]));
        }

        // Every GPU polls the full output vector (barrier-free) and extracts it.
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            ll128FinalizeKernel<<<finBlocks, finThreads, 0, stream[g]>>>(dOutput[g], dResult[g],
                                                                          M, flagVal);
        }
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            CUDA_CHECK(cudaStreamSynchronize(stream[g]));
        }
    };

    // Correctness check. Non-deterministic algorithm (Table I) -> tolerance-based verify().
    runOnce(1);
    std::vector<float> got(M);
    CUDA_CHECK(cudaSetDevice(0));
    CUDA_CHECK(cudaMemcpy(got.data(), dResult[0], M * sizeof(float), cudaMemcpyDeviceToHost));
    bool ok = verify(got, ref);
    printf("Correctness check (rank 0 output vs CPU reference, relTol): %s\n",
           ok ? "PASSED" : "FAILED");
    if (N > 1) {
        std::vector<float> got1(M);
        CUDA_CHECK(cudaSetDevice(N - 1));
        CUDA_CHECK(cudaMemcpy(got1.data(), dResult[N - 1], M * sizeof(float),
                               cudaMemcpyDeviceToHost));
        bool ok1 = verify(got1, ref);
        printf("Correctness check (rank %d output vs CPU reference, relTol): %s\n", N - 1,
               ok1 ? "PASSED" : "FAILED");
        ok = ok && ok1;
    }

    // Latency loop.
    WallTimer timer;
    double totalMs = 0;
    for (int it = 0; it < iters; ++it) {
        unsigned int flagVal = (unsigned int)(2 + it);
        timer.begin();
        runOnce(flagVal);
        totalMs += timer.endMs();
    }
    printf("Average two-shot LL128 Atomic AllReduce latency over %d iters: %.3f us\n", iters,
           totalMs * 1000.0 / iters);
    printf(
        "(Includes the per-iteration scratch reset needed before atomicAdd reuse -- "
        "see README.)\n");

    for (int g = 0; g < N; ++g) {
        CUDA_CHECK(cudaSetDevice(g));
        cudaFree(dInput[g]);
        cudaFree(dRegularScratch[g]);
        cudaFree(dDisplacedScratch[g]);
        cudaFree(dRegularPtrTable[g]);
        cudaFree(dDisplacedPtrTable[g]);
        cudaFree(dOutput[g]);
        cudaFree(dOutPtrTable[g]);
        cudaFree(dResult[g]);
        cudaStreamDestroy(stream[g]);
    }
    return ok ? 0 : 1;
}
