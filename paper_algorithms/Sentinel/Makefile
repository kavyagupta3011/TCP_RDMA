# Builds the one-shot Sentinel AllReduce demo.
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

TARGET = sentinel_allreduce
SRC = sentinel_allreduce.cu

.PHONY: all run clean
all: $(TARGET)

$(TARGET): $(SRC) ../common/gpu_common.cuh
	$(NVCC) $(NVCCFLAGS) -o $(TARGET) $(SRC)

# Usage: make run [NGPUS=4] [NFLOATS=65536] [ITERS=100]
run: $(TARGET)
	./$(TARGET) $(NGPUS) $(NFLOATS) $(ITERS)

clean:
	rm -f $(TARGET)
