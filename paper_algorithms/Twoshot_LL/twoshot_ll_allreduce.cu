// twoshot_ll_allreduce.cu
//
// Two-shot AllReduce using LL synchronization for BOTH phases, Table I row
// "Two-shot (LL)" of "Every us Matters: Achieving Near Speed-of-Light
// Latency in GPU Collectives" (arXiv:2607.16100).
//
// A two-shot AllReduce (paper, Section III-A2) splits the work into:
//   Phase 1, ReduceScatter: the M-element vector is split into N partitions
//     (chunkElems = M/N each), one owned per GPU. Every GPU sends every
//     OTHER GPU exactly the slice of ITS OWN data that belongs to that
//     other GPU's partition. Each owner GPU then sums the N-1 slices it
//     receives together with its own local slice, ending up as the sole
//     holder of the fully-reduced partition it owns.
//   Phase 2, AllGather: each owner pushes its now-complete partition out to
//     every other GPU, so every GPU ends up with the full M-element result.
//
// This cuts total communication volume from one-shot's O(N*M) down to
// O(M) per Table I -- the tradeoff is 2 rounds of synchronization instead
// of 1. This file makes BOTH of those rounds barrier-free using exactly the
// same LL packed-flag-and-data trick as LL/ll_allreduce.cu: every push is a
// single aligned 16-byte atomic store carrying 2 floats + a duplicated
// epoch flag, and every receive is a busy-poll on that same line -- no
// separate barrier call anywhere in this file. This is precisely
// LL/ll_allreduce.cu's mechanism, applied twice, once per phase, each time
// operating on an M/N-sized slice instead of the full M.
//
// Verifying Table I's "4(N-1)M/N" comm-volume-per-GPU figure against this
// implementation: as a *sender*, each GPU emits (N-1) LL-packed slices of
// size M/N during ReduceScatter (2x for the flag packing = 2(N-1)M/N bytes-
// equivalent), then, if it happens to be a partition owner, (N-1) more
// LL-packed slices of size M/N during AllGather (another 2(N-1)M/N) --
// totalling 4(N-1)M/N, matching the table exactly.
//
// Build:   make            (see Makefile; requires 2+ NVLink/P2P GPUs)
// Run:     ./twoshot_ll_allreduce [numGPUs] [numFloats] [iters]
//           numFloats must be divisible by (2*numGPUs) -- rounded up if not.

#include "../common/gpu_common.cuh"

// Same 16-byte packed line as LL/ll_allreduce.cu.
struct __align__(16) LLLine {
    unsigned int data0;
    unsigned int data1;
    unsigned int flag0;
    unsigned int flag1;
};

// ---- Phase 1: ReduceScatter -------------------------------------------
//
// Grid: N "target groups" of blocksPerChunk blocks each (blockIdx.x / blocksPerChunk
// selects the target rank r whose partition this block contributes to).
// Launched on every source GPU `self`. GPU self does NOT push to itself
// (r == self): its own slice of its own partition is summed directly from
// local memory in rsReduceKernel below, no network hop needed for it.
__global__ void rsSendKernel(const float* __restrict__ input, LLLine** __restrict__ scratchPtrs,
                              int self, int n, int chunkHalf, int blocksPerChunk,
                              unsigned int flagVal) {
    int r = blockIdx.x / blocksPerChunk;
    int localBlock = blockIdx.x % blocksPerChunk;
    int i = localBlock * blockDim.x + threadIdx.x;
    if (i >= chunkHalf || r == self) return;

    size_t base = (size_t)r * chunkHalf * 2;  // GPU self's slice belonging to partition r
    LLLine line;
    line.data0 = __float_as_uint(input[base + 2 * i]);
    line.data1 = __float_as_uint(input[base + 2 * i + 1]);
    line.flag0 = flagVal;
    line.flag1 = flagVal;
    LLLine* dst = scratchPtrs[r] + (size_t)self * chunkHalf + i;
    *dst = line;
}

// Launched on every GPU for its OWN owned partition (r == self, implicitly):
// polls the N-1 incoming slices in its own scratch and sums them with its
// own local slice, producing this GPU's fully-reduced owned partition.
__global__ void rsReduceKernel(const float* __restrict__ myInput,
                                const LLLine* __restrict__ myScratch,
                                float* __restrict__ reducedPartition, int self, int n,
                                int chunkHalf, unsigned int flagVal) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= chunkHalf) return;

    size_t base = (size_t)self * chunkHalf * 2;  // GPU self's own slice of ITS OWN partition
    float sum0 = myInput[base + 2 * i];
    float sum1 = myInput[base + 2 * i + 1];

    for (int p = 0; p < n; ++p) {
        if (p == self) continue;
        volatile const LLLine* src = myScratch + (size_t)p * chunkHalf + i;
        LLLine line;
        do {
            line.data0 = src->data0;
            line.data1 = src->data1;
            line.flag0 = src->flag0;
            line.flag1 = src->flag1;
        } while (line.flag0 != flagVal || line.flag1 != flagVal);
        sum0 += __uint_as_float(line.data0);
        sum1 += __uint_as_float(line.data1);
    }
    reducedPartition[2 * i] = sum0;
    reducedPartition[2 * i + 1] = sum1;
}

// ---- Phase 2: AllGather -------------------------------------------------
//
// Launched on GPU `self` (the owner of its reducedPartition). Pushes the
// completed partition to every peer's output-line array (and writes its own
// copy locally too), LL-flagged so peers can poll without a barrier.
__global__ void agSendKernel(const float* __restrict__ reducedPartition,
                              LLLine** __restrict__ outLinePtrs, int self, int n, int chunkHalf,
                              unsigned int flagVal) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= chunkHalf) return;

    LLLine line;
    line.data0 = __float_as_uint(reducedPartition[2 * i]);
    line.data1 = __float_as_uint(reducedPartition[2 * i + 1]);
    line.flag0 = flagVal;
    line.flag1 = flagVal;

    size_t idx = (size_t)self * chunkHalf + i;  // position within the global M/2-line array
    outLinePtrs[self][idx] = line;              // this GPU's own copy
    for (int p = 0; p < n; ++p) {
        if (p == self) continue;
        outLinePtrs[p][idx] = line;  // push to every peer
    }
}

// Every GPU polls its full output-line array (own chunk + N-1 peers' pushed
// chunks) and extracts plain floats.
__global__ void agFinalizeKernel(const LLLine* __restrict__ myOutputLines,
                                  float* __restrict__ result, size_t mHalf,
                                  unsigned int flagVal) {
    size_t i = (size_t)blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= mHalf) return;
    volatile const LLLine* src = myOutputLines + i;
    LLLine line;
    do {
        line.data0 = src->data0;
        line.data1 = src->data1;
        line.flag0 = src->flag0;
        line.flag1 = src->flag1;
    } while (line.flag0 != flagVal || line.flag1 != flagVal);
    result[2 * i] = __uint_as_float(line.data0);
    result[2 * i + 1] = __uint_as_float(line.data1);
}

int main(int argc, char** argv) {
    int nGpusRequested = (argc > 1) ? atoi(argv[1]) : -1;
    size_t M = (argc > 2) ? (size_t)atol(argv[2]) : (1u << 16);
    int iters = (argc > 3) ? atoi(argv[3]) : 100;

    int N = enablePeerAccessAll(nGpusRequested);
    // M must be divisible by N (chunk boundary) and each chunk must be even (LL packing).
    if (M % N) M += N - (M % N);
    size_t chunkElems = M / N;
    if (chunkElems % 2) {
        chunkElems++;
        M = chunkElems * N;
    }
    size_t chunkHalf = chunkElems / 2;
    size_t mHalf = M / 2;

    printf(
        "Two-shot LL AllReduce: N=%d GPUs, M=%zu floats total, chunk=%zu floats/rank "
        "(%.1f KB/GPU total input)\n",
        N, M, chunkElems, M * sizeof(float) / 1024.0);

    std::vector<std::vector<float>> hostInputs;
    for (int g = 0; g < N; ++g) hostInputs.push_back(makeRandomVector(M, 5000 + g));
    std::vector<float> ref = referenceAllReduceSum(hostInputs);

    std::vector<float*> dInput(N), dReduced(N), dResult(N);
    std::vector<LLLine*> dRsScratch(N), dOutLines(N);
    std::vector<LLLine**> dRsPtrTable(N), dOutPtrTable(N);
    std::vector<cudaStream_t> stream(N);

    for (int g = 0; g < N; ++g) {
        CUDA_CHECK(cudaSetDevice(g));
        CUDA_CHECK(cudaMalloc(&dInput[g], M * sizeof(float)));
        CUDA_CHECK(cudaMemcpy(dInput[g], hostInputs[g].data(), M * sizeof(float),
                               cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMalloc(&dReduced[g], chunkElems * sizeof(float)));
        CUDA_CHECK(cudaMalloc(&dResult[g], M * sizeof(float)));
        CUDA_CHECK(cudaMalloc(&dRsScratch[g], (size_t)N * chunkHalf * sizeof(LLLine)));
        CUDA_CHECK(cudaMemset(dRsScratch[g], 0, (size_t)N * chunkHalf * sizeof(LLLine)));
        CUDA_CHECK(cudaMalloc(&dOutLines[g], mHalf * sizeof(LLLine)));
        CUDA_CHECK(cudaMemset(dOutLines[g], 0, mHalf * sizeof(LLLine)));
        CUDA_CHECK(cudaStreamCreate(&stream[g]));
    }
    std::vector<LLLine*> hostRsPtrs(N), hostOutPtrs(N);
    for (int g = 0; g < N; ++g) {
        hostRsPtrs[g] = dRsScratch[g];
        hostOutPtrs[g] = dOutLines[g];
    }
    for (int g = 0; g < N; ++g) {
        CUDA_CHECK(cudaSetDevice(g));
        CUDA_CHECK(cudaMalloc(&dRsPtrTable[g], N * sizeof(LLLine*)));
        CUDA_CHECK(cudaMemcpy(dRsPtrTable[g], hostRsPtrs.data(), N * sizeof(LLLine*),
                               cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMalloc(&dOutPtrTable[g], N * sizeof(LLLine*)));
        CUDA_CHECK(cudaMemcpy(dOutPtrTable[g], hostOutPtrs.data(), N * sizeof(LLLine*),
                               cudaMemcpyHostToDevice));
    }

    const int threads = 256;
    const int blocksPerChunk = (int)((chunkHalf + threads - 1) / threads);
    const int rsGrid = blocksPerChunk * N;
    const int reduceBlocks = blocksPerChunk;
    const int agBlocks = blocksPerChunk;
    const int finBlocks = (int)((mHalf + threads - 1) / threads);

    auto runOnce = [&](unsigned int flagVal) {
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            rsSendKernel<<<rsGrid, threads, 0, stream[g]>>>(
                dInput[g], dRsPtrTable[g], g, N, (int)chunkHalf, blocksPerChunk, flagVal);
        }
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            rsReduceKernel<<<reduceBlocks, threads, 0, stream[g]>>>(
                dInput[g], dRsScratch[g], dReduced[g], g, N, (int)chunkHalf, flagVal);
        }
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            agSendKernel<<<agBlocks, threads, 0, stream[g]>>>(dReduced[g], dOutPtrTable[g], g, N,
                                                               (int)chunkHalf, flagVal);
        }
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            agFinalizeKernel<<<finBlocks, threads, 0, stream[g]>>>(dOutLines[g], dResult[g],
                                                                    mHalf, flagVal);
        }
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            CUDA_CHECK(cudaStreamSynchronize(stream[g]));
        }
    };

    runOnce(1);
    std::vector<float> got(M);
    CUDA_CHECK(cudaSetDevice(0));
    CUDA_CHECK(cudaMemcpy(got.data(), dResult[0], M * sizeof(float), cudaMemcpyDeviceToHost));
    bool ok = verify(got, ref);
    printf("Correctness check (rank 0 output vs CPU reference): %s\n",
           ok ? "PASSED" : "FAILED");
    if (N > 1) {
        std::vector<float> got1(M);
        CUDA_CHECK(cudaSetDevice(N - 1));
        CUDA_CHECK(
            cudaMemcpy(got1.data(), dResult[N - 1], M * sizeof(float), cudaMemcpyDeviceToHost));
        bool ok1 = verify(got1, ref);
        printf("Correctness check (rank %d output vs CPU reference): %s\n", N - 1,
               ok1 ? "PASSED" : "FAILED");
        ok = ok && ok1;
    }

    WallTimer timer;
    double totalMs = 0;
    for (int it = 0; it < iters; ++it) {
        unsigned int flagVal = (unsigned int)(2 + it);
        timer.begin();
        runOnce(flagVal);
        totalMs += timer.endMs();
    }
    printf("Average two-shot LL AllReduce latency over %d iters: %.3f us\n", iters,
           totalMs * 1000.0 / iters);

    for (int g = 0; g < N; ++g) {
        CUDA_CHECK(cudaSetDevice(g));
        cudaFree(dInput[g]);
        cudaFree(dReduced[g]);
        cudaFree(dResult[g]);
        cudaFree(dRsScratch[g]);
        cudaFree(dOutLines[g]);
        cudaFree(dRsPtrTable[g]);
        cudaFree(dOutPtrTable[g]);
        cudaStreamDestroy(stream[g]);
    }
    return ok ? 0 : 1;
}
