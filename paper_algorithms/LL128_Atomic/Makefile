# Builds the two-shot LL128 Atomic AllReduce demo.
#
# ARCH selects the target GPU architecture. `native` (default, needs
# CUDA 12.0+) asks nvcc to auto-detect the GPU(s) actually present on this
# machine. If your nvcc is older, or `native` fails, override it, e.g.:
#   make ARCH=sm_80     # A100
#   make ARCH=sm_90     # H100 / H200
#   make ARCH=sm_100    # B200/GB200
ARCH ?= native
NVCC ?= nvcc
NVCCFLAGS = -O3 -std=c++17 -arch=$(ARCH)

TARGET = ll128_atomic_allreduce
SRC = ll128_atomic_allreduce.cu

.PHONY: all run clean
all: $(TARGET)

$(TARGET): $(SRC) ../common/gpu_common.cuh
	$(NVCC) $(NVCCFLAGS) -o $(TARGET) $(SRC)

# Usage: make run [NGPUS=4] [CHUNKELEMS=1024] [ITERS=50]
# chunkElemsPerRank must be a multiple of 32 and <= 4064 -- see the file header.
run: $(TARGET)
	./$(TARGET) $(NGPUS) $(CHUNKELEMS) $(ITERS)

clean:
	rm -f $(TARGET)
