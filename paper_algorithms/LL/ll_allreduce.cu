// ll_allreduce.cu
//
// One-shot AllReduce using the LL (low-latency) synchronization technique,
// Section IV.B.1 of "Every us Matters: Achieving Near Speed-of-Light Latency
// in GPU Collectives" (arXiv:2607.16100).
//
// Idea (paper, verbatim mechanism): conventional designs signal data
// arrival with an explicit flag plus a separate memory barrier. LL removes
// the barrier by packing an 8-byte flag together with 8-byte data and
// writing both in a single 16-byte atomic store, so the receiver can tell
// data is valid just by checking the flag it read *in the same transaction*
// -- no barrier needed. This halves effective payload bandwidth (8 of every
// 16 bytes are the flag) and needs 2*N*D scratch space per Table I, which is
// why the paper says LL is "mostly suitable for very small messages."
//
// This file implements exactly that, for a one-shot (single round,
// O(1)-synchronization) AllReduce-sum across N GPUs in push mode:
//   1. Every GPU pushes its full input vector directly into every peer's
//      scratch buffer, each 16-byte "line" holding 2 FP32 values + a
//      duplicated 4-byte flag (== the current epoch number).
//   2. Every GPU polls its own scratch buffer (no remote reads needed here
//      -- the data already arrived) until each peer's line shows the
//      current epoch, then reduces (sums) its own input with what arrived.
// Because the flag carries an *epoch* number that increments every call,
// there is no need to ever reset the scratch buffer between calls (unlike
// Sentinel) -- a stale line from a previous round simply won't match the
// new epoch.
//
// Data movement between GPUs is plain, peer-access-enabled CUDA pointers
// (see ../common/gpu_common.cuh for why), which is what actually backs the
// paper's "symmetric memory, load/store accessible" abstraction on an
// NVLink-connected node.
//
// Build:   make            (see Makefile; requires 2+ NVLink/P2P GPUs)
// Run:     ./ll_allreduce [numGPUs] [numFloats] [iters]

#include "../common/gpu_common.cuh"

// One 16-byte "line": 2 packed FP32 data values (as raw bits) + a duplicated
// 4-byte flag. A 16-byte store/load that is naturally aligned to 16 bytes is
// treated as a single indivisible transaction by the GPU memory system, so a
// receiver can never observe "half old, half new" -- this is the same
// hardware property NCCL's real LL protocol relies on.
struct __align__(16) LLLine {
    unsigned int data0;
    unsigned int data1;
    unsigned int flag0;
    unsigned int flag1;
};

// Launched on source GPU `self`. Pushes this GPU's data to every peer's
// scratch buffer, packed with the current epoch as the LL flag.
__global__ void llSendKernel(const float* __restrict__ input,
                              LLLine** __restrict__ scratchPtrs, int self, int n,
                              int mHalf, unsigned int flagVal) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= mHalf) return;

    LLLine line;
    line.data0 = __float_as_uint(input[2 * i]);
    line.data1 = __float_as_uint(input[2 * i + 1]);
    line.flag0 = flagVal;
    line.flag1 = flagVal;

    for (int p = 0; p < n; ++p) {
        if (p == self) continue;
        // scratchPtrs[p] is a direct P2P pointer into peer p's scratch
        // buffer; row `self` within it is this GPU's private lane to peer p.
        LLLine* dst = scratchPtrs[p] + (size_t)self * mHalf + i;
        *dst = line;  // single aligned 16-byte store (push)
    }
}

// Launched on GPU `self`. Polls its own (local) scratch buffer -- filled by
// peers' llSendKernel -- and reduces once every peer's line shows flagVal.
__global__ void llRecvReduceKernel(const float* __restrict__ myInput,
                                    const LLLine* __restrict__ myScratch,
                                    float* __restrict__ output, int self, int n,
                                    int mHalf, unsigned int flagVal) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= mHalf) return;

    float sum0 = myInput[2 * i];
    float sum1 = myInput[2 * i + 1];

    for (int p = 0; p < n; ++p) {
        if (p == self) continue;
        volatile const LLLine* src = myScratch + (size_t)p * mHalf + i;
        LLLine line;
        // Busy-poll (volatile forces a fresh load every iteration) until the
        // flag matches the current epoch -- this IS the synchronization;
        // there is no separate barrier call anywhere in this algorithm.
        do {
            line.data0 = src->data0;
            line.data1 = src->data1;
            line.flag0 = src->flag0;
            line.flag1 = src->flag1;
        } while (line.flag0 != flagVal || line.flag1 != flagVal);

        sum0 += __uint_as_float(line.data0);
        sum1 += __uint_as_float(line.data1);
    }

    output[2 * i] = sum0;
    output[2 * i + 1] = sum1;
}

int main(int argc, char** argv) {
    int nGpusRequested = (argc > 1) ? atoi(argv[1]) : -1;
    size_t M = (argc > 2) ? (size_t)atol(argv[2]) : (1u << 16);  // elements
    int iters = (argc > 3) ? atoi(argv[3]) : 100;
    if (M % 2) M++;  // LL packs 2 floats per 16B line
    size_t mHalf = M / 2;

    int N = enablePeerAccessAll(nGpusRequested);
    printf("LL one-shot AllReduce: N=%d GPUs, M=%zu floats (%.1f KB/GPU input)\n", N, M,
           M * sizeof(float) / 1024.0);

    std::vector<std::vector<float>> hostInputs;
    for (int g = 0; g < N; ++g) hostInputs.push_back(makeRandomVector(M, 1000 + g));
    std::vector<float> ref = referenceAllReduceSum(hostInputs);

    std::vector<float*> dInput(N), dOutput(N);
    std::vector<LLLine*> dScratch(N);
    std::vector<LLLine**> dScratchPtrTable(N);
    std::vector<cudaStream_t> stream(N);

    for (int g = 0; g < N; ++g) {
        CUDA_CHECK(cudaSetDevice(g));
        CUDA_CHECK(cudaMalloc(&dInput[g], M * sizeof(float)));
        CUDA_CHECK(cudaMalloc(&dOutput[g], M * sizeof(float)));
        CUDA_CHECK(cudaMemcpy(dInput[g], hostInputs[g].data(), M * sizeof(float),
                               cudaMemcpyHostToDevice));
        // Scratch: N rows (one per possible source, row `g` unused) x mHalf lines.
        CUDA_CHECK(cudaMalloc(&dScratch[g], (size_t)N * mHalf * sizeof(LLLine)));
        CUDA_CHECK(cudaMemset(dScratch[g], 0, (size_t)N * mHalf * sizeof(LLLine)));
        CUDA_CHECK(cudaStreamCreate(&stream[g]));
    }
    // Each GPU keeps a device-resident table of all N scratch-buffer
    // pointers so its send kernel can address every peer directly.
    std::vector<LLLine*> hostScratchPtrs(N);
    for (int g = 0; g < N; ++g) hostScratchPtrs[g] = dScratch[g];
    for (int g = 0; g < N; ++g) {
        CUDA_CHECK(cudaSetDevice(g));
        CUDA_CHECK(cudaMalloc(&dScratchPtrTable[g], N * sizeof(LLLine*)));
        CUDA_CHECK(cudaMemcpy(dScratchPtrTable[g], hostScratchPtrs.data(),
                               N * sizeof(LLLine*), cudaMemcpyHostToDevice));
    }

    const int threads = 256;
    const int blocks = (int)((mHalf + threads - 1) / threads);

    auto runOnce = [&](unsigned epoch) {
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            llSendKernel<<<blocks, threads, 0, stream[g]>>>(
                dInput[g], dScratchPtrTable[g], g, N, (int)mHalf, epoch);
        }
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            llRecvReduceKernel<<<blocks, threads, 0, stream[g]>>>(
                dInput[g], dScratch[g], dOutput[g], g, N, (int)mHalf, epoch);
        }
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            CUDA_CHECK(cudaStreamSynchronize(stream[g]));
        }
    };

    // Correctness check (epoch 1; scratch was memset to 0 so no stale match).
    runOnce(1);
    std::vector<float> got(M);
    CUDA_CHECK(cudaSetDevice(0));
    CUDA_CHECK(cudaMemcpy(got.data(), dOutput[0], M * sizeof(float), cudaMemcpyDeviceToHost));
    bool ok = verify(got, ref);
    printf("Correctness check (rank 0 output vs CPU reference): %s\n",
           ok ? "PASSED" : "FAILED");
    if (N > 1) {
        std::vector<float> got1(M);
        CUDA_CHECK(cudaSetDevice(N - 1));
        CUDA_CHECK(
            cudaMemcpy(got1.data(), dOutput[N - 1], M * sizeof(float), cudaMemcpyDeviceToHost));
        bool ok1 = verify(got1, ref);
        printf("Correctness check (rank %d output vs CPU reference): %s\n", N - 1,
               ok1 ? "PASSED" : "FAILED");
        ok = ok && ok1;
    }

    // Latency loop.
    WallTimer timer;
    double totalMs = 0;
    for (int it = 0; it < iters; ++it) {
        unsigned epoch = (unsigned)(2 + it);  // keep distinct from the epoch=1 warmup above
        timer.begin();
        runOnce(epoch);
        totalMs += timer.endMs();
    }
    printf("Average one-shot LL AllReduce latency over %d iters: %.3f us\n", iters,
           totalMs * 1000.0 / iters);

    for (int g = 0; g < N; ++g) {
        CUDA_CHECK(cudaSetDevice(g));
        cudaFree(dInput[g]);
        cudaFree(dOutput[g]);
        cudaFree(dScratch[g]);
        cudaFree(dScratchPtrTable[g]);
        cudaStreamDestroy(stream[g]);
    }
    return ok ? 0 : 1;
}
