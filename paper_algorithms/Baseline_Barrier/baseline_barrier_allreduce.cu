// baseline_barrier_allreduce.cu
//
// A "conventional" one-shot AllReduce that DOES use an explicit memory
// barrier between the push and the reduce step -- i.e. exactly the design
// pattern the paper analyzes and then eliminates. This file is NOT one of
// the paper's five Table I techniques; it exists purely as a control /
// baseline so the other five programs in paper_algorithms/ have something
// concrete to be measured against, matching the paper's own methodology
// (Section III-B: "After examining several one-shot and two-shot AllReduce
// implementations ... we observe that ... these designs typically rely on
// explicit memory barriers to synchronize peers and signal data readiness";
// Fig. 3 measures exactly this barrier's cost).
//
// Mechanism: every GPU pushes its full input directly into every peer's
// scratch buffer (plain writes, no flag, no polling -- unlike every other
// file in this project, the sender here does NOT encode "is this ready"
// into the transfer at all). Correctness instead comes entirely from an
// EXPLICIT BARRIER inserted between the push and the reduce: every GPU's
// push kernel must complete before any GPU's reduce kernel is allowed to
// start reading. That barrier is implemented here as a host-side
// cross-stream synchronization -- cudaStreamSynchronize() on every GPU's
// stream, called by the host between launching the push kernels and
// launching the reduce kernels.
//
// Honesty note: the paper's own conventional baseline uses a *device-side*
// barrier (ncclLsaBarrierSession, measured directly in Fig. 3 at roughly
// 0.85-1.68us depending on GPU count) -- a GPU-initiated primitive that
// synchronizes peers without ever leaving the device. This file uses a
// *host-side* barrier instead, since that device-side primitive is part of
// NCCL's not-yet-widely-available experimental API (see the top-level
// README's "Why standard CUDA P2P instead of NCCL's device-side API"
// section). A host round-trip is a real barrier -- it has the identical
// correctness role and the identical architectural cost category (everyone
// must stop and wait) -- but it will typically cost MORE than the paper's
// own Fig. 3 numbers, not less. So: the *absolute* baseline latency you
// measure here will likely overstate the barrier's true cost relative to
// what the paper reports, but the qualitative comparison against this
// project's five barrier-free implementations is still the right
// demonstration -- barrier-based communication pays a synchronization tax
// every round; barrier-free does not.
//
// Build:   make            (see Makefile; requires 2+ NVLink/P2P GPUs)
// Run:     ./baseline_barrier_allreduce [numGPUs] [numFloats] [iters]

#include "../common/gpu_common.cuh"

// Plain, unflagged push: every GPU writes its full input directly into
// every peer's scratch buffer. On its own this is NOT safe to read from --
// there is no signal telling a peer when the write has landed. Correctness
// depends entirely on the barrier main() inserts before reduceKernel runs.
__global__ void pushKernel(const float* __restrict__ input, float** __restrict__ scratchPtrs,
                            int self, int n, int m) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= m) return;
    float v = input[i];
    for (int p = 0; p < n; ++p) {
        if (p == self) continue;
        scratchPtrs[p][(size_t)self * m + i] = v;
    }
}

// Safe to run ONLY after the host has confirmed (via the barrier) that
// every GPU's pushKernel has completed -- there is no polling here at all,
// unlike every barrier-free kernel elsewhere in this project.
__global__ void reduceKernel(const float* __restrict__ myInput,
                              const float* __restrict__ myScratch, float* __restrict__ output,
                              int self, int n, int m) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= m) return;
    float sum = myInput[i];
    for (int p = 0; p < n; ++p) {
        if (p == self) continue;
        sum += myScratch[(size_t)p * m + i];
    }
    output[i] = sum;
}

int main(int argc, char** argv) {
    int nGpusRequested = (argc > 1) ? atoi(argv[1]) : -1;
    size_t M = (argc > 2) ? (size_t)atol(argv[2]) : (1u << 16);
    int iters = (argc > 3) ? atoi(argv[3]) : 100;

    int N = enablePeerAccessAll(nGpusRequested);
    printf("Baseline (explicit host barrier) one-shot AllReduce: N=%d GPUs, M=%zu floats\n", N,
           M);

    std::vector<std::vector<float>> hostInputs;
    for (int g = 0; g < N; ++g) hostInputs.push_back(makeRandomVector(M, 7000 + g));
    std::vector<float> ref = referenceAllReduceSum(hostInputs);

    std::vector<float*> dInput(N), dOutput(N), dScratch(N);
    std::vector<float**> dScratchPtrTable(N);
    std::vector<cudaStream_t> stream(N);

    for (int g = 0; g < N; ++g) {
        CUDA_CHECK(cudaSetDevice(g));
        CUDA_CHECK(cudaMalloc(&dInput[g], M * sizeof(float)));
        CUDA_CHECK(cudaMalloc(&dOutput[g], M * sizeof(float)));
        CUDA_CHECK(cudaMemcpy(dInput[g], hostInputs[g].data(), M * sizeof(float),
                               cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMalloc(&dScratch[g], (size_t)N * M * sizeof(float)));
        CUDA_CHECK(cudaStreamCreate(&stream[g]));
    }
    std::vector<float*> hostScratchPtrs(N);
    for (int g = 0; g < N; ++g) hostScratchPtrs[g] = dScratch[g];
    for (int g = 0; g < N; ++g) {
        CUDA_CHECK(cudaSetDevice(g));
        CUDA_CHECK(cudaMalloc(&dScratchPtrTable[g], N * sizeof(float*)));
        CUDA_CHECK(cudaMemcpy(dScratchPtrTable[g], hostScratchPtrs.data(), N * sizeof(float*),
                               cudaMemcpyHostToDevice));
    }

    const int threads = 256;
    const int blocks = (int)((M + threads - 1) / threads);

    auto runOnce = [&]() {
        // Step 1: push (no signaling).
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            pushKernel<<<blocks, threads, 0, stream[g]>>>(dInput[g], dScratchPtrTable[g], g, N,
                                                           (int)M);
        }
        // ---- THE BARRIER ----
        // Every GPU's push must be confirmed complete before ANY GPU is
        // allowed to start reducing -- this synchronize-all-streams step is
        // the explicit memory barrier this file exists to demonstrate the
        // cost of.
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            CUDA_CHECK(cudaStreamSynchronize(stream[g]));
        }
        // Step 2: reduce (safe now, no polling needed -- the barrier already
        // guaranteed visibility of every peer's write).
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            reduceKernel<<<blocks, threads, 0, stream[g]>>>(dInput[g], dScratch[g], dOutput[g], g,
                                                             N, (int)M);
        }
        for (int g = 0; g < N; ++g) {
            CUDA_CHECK(cudaSetDevice(g));
            CUDA_CHECK(cudaStreamSynchronize(stream[g]));
        }
    };

    runOnce();
    std::vector<float> got(M);
    CUDA_CHECK(cudaSetDevice(0));
    CUDA_CHECK(cudaMemcpy(got.data(), dOutput[0], M * sizeof(float), cudaMemcpyDeviceToHost));
    bool ok = verify(got, ref);
    printf("Correctness check (rank 0 output vs CPU reference): %s\n",
           ok ? "PASSED" : "FAILED");

    WallTimer timer;
    double totalMs = 0;
    for (int it = 0; it < iters; ++it) {
        timer.begin();
        runOnce();
        totalMs += timer.endMs();
    }
    printf("Average barrier-based one-shot AllReduce latency over %d iters: %.3f us\n", iters,
           totalMs * 1000.0 / iters);
    printf(
        "Compare this against LL/, Sentinel/, Twoshot_LL/, Twoshot_Sentinel/ and "
        "LL128_Atomic/ at the same numFloats to see the barrier-free speedup directly.\n");

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
