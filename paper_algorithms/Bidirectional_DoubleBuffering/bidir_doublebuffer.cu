// bidir_doublebuffer.cu
//
// Chunked pairwise reduction using Bidirectional Communication & Double
// Buffering, Section IV.B.3 and Fig. 4 of "Every us Matters: Achieving Near
// Speed-of-Light Latency in GPU Collectives" (arXiv:2607.16100).
//
// Idea (paper, verbatim mechanism, Fig. 4): LL and Sentinel remove the
// barrier for a *single* exchange, but a message too large for one round of
// scratch space must be sent in chunks, and naively that reintroduces a
// barrier between chunks (to stop a chunk being overwritten before the
// receiver has read it). The paper removes that barrier too: split scratch
// into two buffers (0 and 1). Both ranks start on buffer 0. Each rank pushes
// chunk i into the peer's *current* buffer, then waits for the peer's chunk
// i to land in its own current buffer, reduces it, and only THEN advances to
// the next buffer for chunk i+1. Because each side only reuses buffer 0
// again once two chunks later (i+2), and by then it has already both sent
// its own chunk i+1 into the peer's buffer 1 and received+consumed the
// peer's chunk i from buffer 0, no separate barrier is ever needed --
// "each receive from a peer serves as an implicit permission for the next
// send," matching the paper's own words. This mechanism is independent of
// whether LL or Sentinel packs the individual chunk exchange (paper,
// caption of Fig. 4); this file uses LL-style flag packing (a chunk index
// used as an epoch), since that also avoids the extra sentinel-reset cost
// between chunks.
//
// This file demonstrates the *pairwise* (2-rank) exchange exactly as the
// paper's Fig. 4 teaches it: two GPUs jointly reduce (sum) a vector too
// large to fit in one round of scratch space, split into `numChunks`
// pieces, using a single persistent kernel per GPU (the whole chunk loop
// runs on-device, so there is no host round-trip -- and therefore no
// host-side barrier -- between chunks). Scaling this to N>2 ranks means
// running this same pairwise exchange as a sub-step of a larger schedule
// (e.g. a ring or recursive-doubling pattern across pairs); that
// composition is out of scope for this file, which focuses on getting the
// core two-rank mechanism exactly right.
//
// Build:   make            (see Makefile; requires exactly 2 NVLink/P2P GPUs)
// Run:     ./bidir_doublebuffer [numFloats] [numChunks] [iters]

#include "../common/gpu_common.cuh"

// Same 16-byte packed line as LL/ll_allreduce.cu: 2 FP32 values + a
// duplicated 4-byte flag, written/read as a single aligned 16-byte
// transaction. Here the flag doubles as the chunk's epoch (chunk index + 1),
// which is what lets buffer 0 be safely reused for chunk i+2 without a reset.
struct __align__(16) DBLine {
    unsigned int data0;
    unsigned int data1;
    unsigned int flag0;
    unsigned int flag1;
};

// One persistent kernel per GPU. `myScratch` is this GPU's own 2-buffer
// scratch area (peers write into it); `peerScratch` is a direct P2P pointer
// into the OTHER GPU's scratch area (this GPU writes into it). The device
// loop below walks every chunk without returning to the host, which is what
// makes this genuinely barrier-free across the whole message, not just
// within one chunk.
__global__ void bidirDoubleBufferKernel(const float* __restrict__ input,
                                         DBLine* __restrict__ myScratch,
                                         DBLine* __restrict__ peerScratch,
                                         float* __restrict__ output, int chunkElems,
                                         int numChunks) {
    int chunkHalf = chunkElems / 2;
    int i = blockIdx.x * blockDim.x + threadIdx.x;  // element-pair index within one chunk
    if (i >= chunkHalf) return;

    for (int c = 0; c < numChunks; ++c) {
        int buf = c & 1;                        // Buffer 0 on even chunks, Buffer 1 on odd
        unsigned int flagVal = (unsigned int)(c + 1);
        size_t base = (size_t)c * chunkElems;

        // Step (paper Fig. 4, arrow): push this rank's chunk c into the
        // peer's *current* buffer.
        DBLine out;
        out.data0 = __float_as_uint(input[base + 2 * i]);
        out.data1 = __float_as_uint(input[base + 2 * i + 1]);
        out.flag0 = flagVal;
        out.flag1 = flagVal;
        DBLine* dst = peerScratch + (size_t)buf * chunkHalf + i;
        *dst = out;

        // Step: wait for the peer's chunk c to land in *our* current buffer.
        // This wait is the entire synchronization for this chunk -- no
        // separate barrier call.
        volatile DBLine* src = myScratch + (size_t)buf * chunkHalf + i;
        DBLine in;
        do {
            in.data0 = src->data0;
            in.data1 = src->data1;
            in.flag0 = src->flag0;
            in.flag1 = src->flag1;
        } while (in.flag0 != flagVal || in.flag1 != flagVal);

        // Reduce (sum) local chunk c with the peer's chunk c.
        output[base + 2 * i] = __uint_as_float(out.data0) + __uint_as_float(in.data0);
        output[base + 2 * i + 1] = __uint_as_float(out.data1) + __uint_as_float(in.data1);

        // No reset, no extra barrier before the loop reuses this buffer at
        // chunk c+2: by construction every thread only advances past this
        // point once it has both sent into the peer's buffer `buf` for
        // chunk c and consumed the peer's data out of its OWN buffer `buf`
        // for chunk c. The peer's loop offers the same guarantee
        // symmetrically, so together the two loops behave exactly like
        // credit-based flow control with a credit of 1 buffer -- "each
        // receive serves as an implicit permission for the next send"
        // (paper, Section IV.B.3).
    }
}

int main(int argc, char** argv) {
    size_t M = (argc > 1) ? (size_t)atol(argv[1]) : (1u << 18);  // total elements (both ranks)
    int numChunks = (argc > 2) ? atoi(argv[2]) : 8;
    int iters = (argc > 3) ? atoi(argv[3]) : 100;

    if ((long long)M % numChunks != 0) {
        M = ((M / numChunks) + 1) * numChunks;  // round up so chunks divide evenly
    }
    int chunkElems = (int)(M / numChunks);
    if (chunkElems % 2) {
        chunkElems++;
        M = (size_t)chunkElems * numChunks;  // LL-style packing needs an even chunk size
    }
    int chunkHalf = chunkElems / 2;

    int N = enablePeerAccessAll(2);
    if (N != 2) {
        fprintf(stderr,
                "This file demonstrates the paper's Fig. 4 PAIRWISE mechanism; it needs "
                "exactly 2 GPUs (found/usable: %d). See the file header for why.\n",
                N);
        return 1;
    }

    printf(
        "Bidirectional Communication & Double Buffering: 2 GPUs, M=%zu floats total, "
        "%d chunks of %d floats each\n",
        M, numChunks, chunkElems);

    std::vector<std::vector<float>> hostInputs;
    hostInputs.push_back(makeRandomVector(M, 3000));
    hostInputs.push_back(makeRandomVector(M, 3001));
    std::vector<float> ref = referenceAllReduceSum(hostInputs);

    float *dInput0, *dInput1, *dOutput0, *dOutput1;
    DBLine *dScratch0, *dScratch1;  // each: 2 buffers x chunkHalf lines
    cudaStream_t stream0, stream1;

    CUDA_CHECK(cudaSetDevice(0));
    CUDA_CHECK(cudaMalloc(&dInput0, M * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&dOutput0, M * sizeof(float)));
    CUDA_CHECK(cudaMemcpy(dInput0, hostInputs[0].data(), M * sizeof(float),
                           cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMalloc(&dScratch0, 2 * (size_t)chunkHalf * sizeof(DBLine)));
    CUDA_CHECK(cudaMemset(dScratch0, 0, 2 * (size_t)chunkHalf * sizeof(DBLine)));
    CUDA_CHECK(cudaStreamCreate(&stream0));

    CUDA_CHECK(cudaSetDevice(1));
    CUDA_CHECK(cudaMalloc(&dInput1, M * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&dOutput1, M * sizeof(float)));
    CUDA_CHECK(cudaMemcpy(dInput1, hostInputs[1].data(), M * sizeof(float),
                           cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMalloc(&dScratch1, 2 * (size_t)chunkHalf * sizeof(DBLine)));
    CUDA_CHECK(cudaMemset(dScratch1, 0, 2 * (size_t)chunkHalf * sizeof(DBLine)));
    CUDA_CHECK(cudaStreamCreate(&stream1));

    const int threads = 256;
    const int blocks = (chunkHalf + threads - 1) / threads;

    auto runOnce = [&]() {
        CUDA_CHECK(cudaSetDevice(0));
        // GPU 0's kernel writes into GPU 1's scratch (dScratch1, a P2P pointer)
        // and reads its own (dScratch0).
        bidirDoubleBufferKernel<<<blocks, threads, 0, stream0>>>(
            dInput0, dScratch0, dScratch1, dOutput0, chunkElems, numChunks);
        CUDA_CHECK(cudaSetDevice(1));
        bidirDoubleBufferKernel<<<blocks, threads, 0, stream1>>>(
            dInput1, dScratch1, dScratch0, dOutput1, chunkElems, numChunks);

        CUDA_CHECK(cudaSetDevice(0));
        CUDA_CHECK(cudaStreamSynchronize(stream0));
        CUDA_CHECK(cudaSetDevice(1));
        CUDA_CHECK(cudaStreamSynchronize(stream1));
    };

    // Correctness check.
    runOnce();
    std::vector<float> got0(M), got1(M);
    CUDA_CHECK(cudaSetDevice(0));
    CUDA_CHECK(cudaMemcpy(got0.data(), dOutput0, M * sizeof(float), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaSetDevice(1));
    CUDA_CHECK(cudaMemcpy(got1.data(), dOutput1, M * sizeof(float), cudaMemcpyDeviceToHost));
    bool ok0 = verify(got0, ref);
    bool ok1 = verify(got1, ref);
    printf("Correctness check (GPU 0 output vs CPU reference): %s\n", ok0 ? "PASSED" : "FAILED");
    printf("Correctness check (GPU 1 output vs CPU reference): %s\n", ok1 ? "PASSED" : "FAILED");
    bool ok = ok0 && ok1;

    // Latency loop. Note flags are chunk-index-based (1..numChunks) and reset
    // to 0 only once above -- exactly like LL, no per-iteration reset is
    // needed EXCEPT that re-running with the same chunk indices across
    // iterations would collide with the previous iteration's flags, so we
    // memset scratch back to 0 between timed iterations here (cheap, and
    // outside the point being measured: within a single call the whole
    // point is that no such reset occurs between chunks).
    WallTimer timer;
    double totalMs = 0;
    for (int it = 0; it < iters; ++it) {
        CUDA_CHECK(cudaSetDevice(0));
        CUDA_CHECK(cudaMemsetAsync(dScratch0, 0, 2 * (size_t)chunkHalf * sizeof(DBLine), stream0));
        CUDA_CHECK(cudaSetDevice(1));
        CUDA_CHECK(cudaMemsetAsync(dScratch1, 0, 2 * (size_t)chunkHalf * sizeof(DBLine), stream1));
        CUDA_CHECK(cudaSetDevice(0));
        CUDA_CHECK(cudaStreamSynchronize(stream0));
        CUDA_CHECK(cudaSetDevice(1));
        CUDA_CHECK(cudaStreamSynchronize(stream1));

        timer.begin();
        runOnce();
        totalMs += timer.endMs();
    }
    printf(
        "Average bidirectional double-buffered reduction latency over %d iters "
        "(%d chunks, one persistent kernel launch per GPU per iter): %.3f us\n",
        iters, numChunks, totalMs * 1000.0 / iters);

    CUDA_CHECK(cudaSetDevice(0));
    cudaFree(dInput0);
    cudaFree(dOutput0);
    cudaFree(dScratch0);
    cudaStreamDestroy(stream0);
    CUDA_CHECK(cudaSetDevice(1));
    cudaFree(dInput1);
    cudaFree(dOutput1);
    cudaFree(dScratch1);
    cudaStreamDestroy(stream1);
    return ok ? 0 : 1;
}
