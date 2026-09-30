// twoshot_sentinel_allreduce.cu
//
// Two-shot AllReduce using Sentinel synchronization for BOTH phases, Table I
// row "Two-shot (Sentinel)" of "Every us Matters: Achieving Near
// Speed-of-Light Latency in GPU Collectives" (arXiv:2607.16100).
//
// Structurally identical to Twoshot_LL/twoshot_ll_allreduce.cu (same
// ReduceScatter-then-AllGather two-phase decomposition; see that file's
// header for the full mechanism explanation) -- the only difference is
// *how* each phase's receiver detects that data has arrived. Sentinel
// writes data directly, full width, no flag; the receiver polls for the
// value to stop matching a reserved sentinel pattern (see
// Sentinel/sentinel_allreduce.cu for the one-shot version of the same
// idea). Table I's numbers for this row -- 2(N-1)M/N comm volume per GPU,
// exactly half of Two-shot LL's 4(N-1)M/N -- come directly from Sentinel
// not paying LL's 2x flag-packing tax in either phase.
//
// The cost, exactly as in the one-shot Sentinel case: every scratch region
// used for detection (the ReduceScatter scratch AND the AllGather output
// array) must be reset to the sentinel value before each round, since
// Sentinel's flag isn't self-clearing the way LL's epoch is. That reset is
// included in every timed iteration here, for the same reason it is in
// Sentinel/sentinel_allreduce.cu.
//
// Build:   make            (see Makefile; requires 2+ NVLink/P2P GPUs)
// Run:     ./twoshot_sentinel_allreduce [numGPUs] [numFloats] [iters]
//           numFloats must be divisible by numGPUs -- rounded up if not.

#include "../common/gpu_common.cuh"

__device__ __forceinline__ unsigned int sentinelBits() { return 0x7fdead00u; }

__global__ void resetSentinelKernel(unsigned int* __restrict__ buf, size_t count) {
    size_t i = (size_t)blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= count) return;
    buf[i] = sentinelBits();
}

// ---- Phase 1: ReduceScatter ----------------------------------------------
// Grid: N "target groups" of blocksPerChunk blocks each, same layout as
// Twoshot_LL. GPU self writes its slice of partition r directly (full
// width, no packing) into r's scratch; self doesn't write to itself (r ==
// self is summed from local memory directly in rsReduceKernel).
__global__ void rsSendKernel(const float* __restrict__ input,
                              unsigned int** __restrict__ scratchPtrs, int self, int n,
                              int chunkElems, int blocksPerChunk) {
    int r = blockIdx.x / blocksPerChunk;
    int localBlock = blockIdx.x % blocksPerChunk;
    int i = localBlock * blockDim.x + threadIdx.x;
    if (i >= chunkElems || r == self) return;

    size_t base = (size_t)r * chunkElems;
    unsigned int bits = __float_as_uint(input[base + i]);
    scratchPtrs[r][(size_t)self * chunkElems + i] = bits;
}

__global__ void rsReduceKernel(const float* __restrict__ myInput,
                                const unsigned int* __restrict__ myScratch,
                                float* __restrict__ reducedPartition, int self, int n,
                                int chunkElems) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= chunkElems) return;

    size_t base = (size_t)self * chunkElems;
    float sum = myInput[base + i];
    for (int p = 0; p < n; ++p) {
        if (p == self) continue;
        volatile const unsigned int* src = myScratch + (size_t)p * chunkElems + i;
        unsigned int bits;
        do {
            bits = *src;
        } while (bits == sentinelBits());
        sum += __uint_as_float(bits);
    }
    reducedPartition[i] = sum;
}

// ---- Phase 2: AllGather --------------------------------------------------
__global__ void agSendKernel(const float* __restrict__ reducedPartition,
                              unsigned int** __restrict__ outPtrs, int self, int n,
                              int chunkElems) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= chunkElems) return;
    unsigned int bits = __float_as_uint(reducedPartition[i]);
    size_t idx = (size_t)self * chunkElems + i;
    outPtrs[self][idx] = bits;  // own copy
    for (int p = 0; p < n; ++p) {
        if (p == self) continue;
        outPtrs[p][idx] = bits;
    }
}

__global__ void agFinalizeKernel(const unsigned int* __restrict__ myOutput,
                                  float* __restrict__ result, size_t m) {
    size_t i = (size_t)blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= m) return;
    volatile const unsigned int* src = myOutput + i;
    unsigned int bits;
    do {
        bits = *src;
    } while (bits == sentinelBits());
    result[i] = __uint_as_float(bits);
}

int main(int argc, char** argv) {
    int nGpusRequested = (argc > 1) ? atoi(argv[1]) : -1;
    size_t M = (argc > 2) ? (size_t)atol(argv[2]) : (1u << 16);
    int iters = (argc > 3) ? atoi(argv[3]) : 100;

    int N = enablePeerAccessAll(nGpusRequested);
    if (M % N) M += N - (M % N);
    size_t chunkElems = M / N;

    printf(
        "Two-shot Sentinel AllReduce: N=%d GPUs, M=%zu floats total, chunk=%zu floats/rank "
        "(%.1f KB/GPU total input)\n",
        N, M, chunkElems, M * sizeof(float) / 1024.0);

    std::vector<std::vector<float>> hostInputs;
    for (int g = 0; g < N; ++g) hostInputs.push_back(makeRandomVector(M, 6000 + g));
    std::vector<float> ref = referenceAllReduceSum(hostInputs);

    std::vector<float*> dInput(N), dReduced(N), dResult(N);
    std::vector<unsigned int*> dRsScratch(N), dOutBuf(N);
    std::vector<unsigned int**> dRsPtrTable(N), dOutPtrTable(N);
    std::vector<cudaStream_t> stream(N);

    for (int g = 0; g < N; ++g) {
        CUDA_CHECK(cudaSetDevice(g));
        CUDA_CHECK(cudaMalloc(&dInput[g], M * sizeof(float)));
        CUDA_CHECK(cudaMemcpy(dInput[g], hostInputs[g].data(), M * sizeof(float),
                               cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMalloc(&dReduced[g], chunkElems * sizeof(float)));
        CUDA_CHECK(cudaMalloc(&dResult[g], M * sizeof(float)));
        CUDA_CHECK(cudaMalloc(&dRsScratch[g], (size_t)N * chunkElems * sizeof(unsigned int)));
        CUDA_CHECK(cudaMalloc(&dOutBuf[g], M * sizeof(unsigned int)));
        CUDA_CHECK(cudaStreamCreate(&stream[g]));
    }
    std::vector<unsigned int*> hostRsPtrs(N), hostOutPtrs(N);
    for (int g = 0; g < N; ++g) {
        hostRsPtrs[g] = dRsScratch[g];
        hostOutPtrs[g] = dOutBuf[g];
    }
    for (int g = 0; g < N; ++g) {
        CUDA_CHECK(cudaSetDevice(g));
        CUDA_CHECK(cudaMalloc(&dRsPtrTable[g], N * sizeof(unsigned int*)));
        CUDA_CHECK(cudaMemcpy(dRsPtrTable[g], hostRsPtrs.data(), N * sizeof(unsigned int*),
                               cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMalloc(&dOutPtrTable[g], N * sizeof(unsigned int*)));
        CUDA_CHECK(cudaMemcpy(dOutPtrTable[g], hostOutPtrs.data(), N * sizeof(unsigned int*),
                               cudaMemcpyHostToDevice));
    }

    const int threads = 256;
    const int blocksPerChunk = (int)((chunkElems + threads - 1) / threads);
    const int rsGrid = blocksPerChunk * N;
    const int reduceBlocks = blocksPerChunk;
    const int agBlocks = blocksPerChunk;
    const int finBlocks = (int)((M + threads - 1) / threads);
    const int rsResetBlocks = (int)(((size_t)N * chunkElems + threads - 1) / threads);
    const int outResetBlocks = (int)((M + threads - 1) / threads);

    auto runOnce = [&]() {
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            resetSentinelKernel<<<rsResetBlocks, threads, 0, stream[g]>>>(
                dRsScratch[g], (size_t)N * chunkElems);
            resetSentinelKernel<<<outResetBlocks, threads, 0, stream[g]>>>(dOutBuf[g], M);
        }
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            CUDA_CHECK(cudaStreamSynchronize(stream[g]));
        }
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            rsSendKernel<<<rsGrid, threads, 0, stream[g]>>>(dInput[g], dRsPtrTable[g], g, N,
                                                             (int)chunkElems, blocksPerChunk);
        }
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            rsReduceKernel<<<reduceBlocks, threads, 0, stream[g]>>>(
                dInput[g], dRsScratch[g], dReduced[g], g, N, (int)chunkElems);
        }
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            agSendKernel<<<agBlocks, threads, 0, stream[g]>>>(dReduced[g], dOutPtrTable[g], g, N,
                                                               (int)chunkElems);
        }
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            agFinalizeKernel<<<finBlocks, threads, 0, stream[g]>>>(dOutBuf[g], dResult[g], M);
        }
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            CUDA_CHECK(cudaStreamSynchronize(stream[g]));
        }
    };

    runOnce();
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
        timer.begin();
        runOnce();
        totalMs += timer.endMs();
    }
    printf("Average two-shot Sentinel AllReduce latency over %d iters: %.3f us\n", iters,
           totalMs * 1000.0 / iters);
    printf("(Includes the per-iteration sentinel-reset pass for both phases -- see README.)\n");

    for (int g = 0; g < N; ++g) {
        CUDA_CHECK(cudaSetDevice(g));
        cudaFree(dInput[g]);
        cudaFree(dReduced[g]);
        cudaFree(dResult[g]);
        cudaFree(dRsScratch[g]);
        cudaFree(dOutBuf[g]);
        cudaFree(dRsPtrTable[g]);
        cudaFree(dOutPtrTable[g]);
        cudaStreamDestroy(stream[g]);
    }
    return ok ? 0 : 1;
}
