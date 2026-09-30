// gpu_common.cuh
//
// Shared host-side helpers used by all four algorithm implementations in
// paper_algorithms/ (LL, Sentinel, Bidirectional_DoubleBuffering, LL128_Atomic).
//
// These are NOT part of the paper's design -- they are ordinary CUDA
// bookkeeping (error checking, peer-access setup, CPU reference AllReduce,
// timing) factored out once so every algorithm folder can stay focused on
// the actual synchronization mechanism it is demonstrating.
//
// All four programs are single-process, multi-GPU. Direct load/store (and,
// for LL128_Atomic, atomicAdd) between GPUs is done through plain CUDA
// pointers made valid across devices by cudaDeviceEnablePeerAccess(). On a
// node where the target GPUs sit in the same NVLink domain, this is exactly
// the "load/store accessible (LSA)" symmetric-memory access pattern the
// paper describes in Fig. 2 -- we are just using the stable, public CUDA
// runtime API to get it instead of NCCL's brand-new (v2.28, experimental,
// not-yet-widely-deployed) device-side ncclLLBuffer template API shown in
// the paper's Fig. 6-8. See the top-level README for why.

#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

#include <cuda_runtime.h>

#define CUDA_CHECK(call)                                                    \
    do {                                                                    \
        cudaError_t _e = (call);                                            \
        if (_e != cudaSuccess) {                                            \
            fprintf(stderr, "CUDA error %s:%d: %s\n", __FILE__, __LINE__,   \
                    cudaGetErrorString(_e));                                \
            exit(1);                                                        \
        }                                                                    \
    } while (0)

// Enables full pairwise peer access among the first `nGpusRequested` visible
// devices (or all visible devices if nGpusRequested <= 0). Aborts with a
// clear message if any pair cannot reach each other directly (e.g. they are
// not on the same NVLink/PCIe P2P domain) -- every algorithm here depends on
// direct load/store (and, for LL128_Atomic, atomicAdd) between GPUs, so
// there is no correct fallback if P2P isn't available.
inline int enablePeerAccessAll(int nGpusRequested) {
    int deviceCount = 0;
    CUDA_CHECK(cudaGetDeviceCount(&deviceCount));
    if (deviceCount < 1) {
        fprintf(stderr, "No CUDA devices visible.\n");
        exit(1);
    }
    int n = (nGpusRequested > 0) ? std::min(nGpusRequested, deviceCount) : deviceCount;
    if (n < 2) {
        fprintf(stderr,
                "Need at least 2 GPUs for an AllReduce demo; found %d visible, "
                "requested %d.\n",
                deviceCount, nGpusRequested);
        exit(1);
    }
    for (int i = 0; i < n; ++i) {
        CUDA_CHECK(cudaSetDevice(i));
        for (int j = 0; j < n; ++j) {
            if (i == j) continue;
            int canAccess = 0;
            CUDA_CHECK(cudaDeviceCanAccessPeer(&canAccess, i, j));
            if (!canAccess) {
                fprintf(stderr,
                        "GPU %d cannot directly access GPU %d's memory (no NVLink/PCIe "
                        "P2P path between them). All GPUs used by this program must sit "
                        "in the same P2P-capable domain, matching the paper's scope "
                        "(Section III: 'GPUs residing in the same NVLink domain').\n",
                        i, j);
                exit(1);
            }
            cudaError_t e = cudaDeviceEnablePeerAccess(j, 0);
            if (e != cudaSuccess && e != cudaErrorPeerAccessAlreadyEnabled) {
                CUDA_CHECK(e);
            } else if (e == cudaErrorPeerAccessAlreadyEnabled) {
                cudaGetLastError();  // clear the sticky "already enabled" error
            }
        }
    }
    return n;
}

inline std::vector<float> makeRandomVector(size_t n, unsigned seed) {
    std::vector<float> v(n);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (size_t i = 0; i < n; ++i) v[i] = dist(rng);
    return v;
}

// CPU reference AllReduce-sum: elementwise sum of every GPU's input vector.
// This is the ground truth every algorithm's GPU output is checked against.
inline std::vector<float> referenceAllReduceSum(
    const std::vector<std::vector<float>>& perGpuInputs) {
    size_t m = perGpuInputs[0].size();
    std::vector<float> ref(m, 0.0f);
    for (const auto& in : perGpuInputs)
        for (size_t i = 0; i < m; ++i) ref[i] += in[i];
    return ref;
}

inline bool verify(const std::vector<float>& got, const std::vector<float>& ref,
                    float relTol = 1e-3f) {
    size_t bad = 0;
    for (size_t i = 0; i < ref.size(); ++i) {
        float diff = fabsf(got[i] - ref[i]);
        float scale = fmaxf(1.0f, fabsf(ref[i]));
        if (diff / scale > relTol) {
            if (bad < 5)
                fprintf(stderr, "  mismatch at %zu: got %f, want %f\n", i, got[i], ref[i]);
            bad++;
        }
    }
    if (bad) fprintf(stderr, "verify: %zu / %zu elements mismatched\n", bad, ref.size());
    return bad == 0;
}

// Wall-clock timer around a full launch+sync round (matches the latency
// benchmarking style used elsewhere in this project: time what a caller
// actually waits for, including the cross-stream synchronize).
struct WallTimer {
    std::chrono::high_resolution_clock::time_point t0;
    void begin() { t0 = std::chrono::high_resolution_clock::now(); }
    double endMs() {
        auto t1 = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count();
    }
};
