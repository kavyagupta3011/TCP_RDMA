# Builds the explicit-barrier baseline AllReduce (not one of the paper's 5
# techniques -- a reference point for measuring their speedup against).
ARCH ?= native
NVCC ?= nvcc
NVCCFLAGS = -O3 -std=c++17 -arch=$(ARCH)

TARGET = baseline_barrier_allreduce
SRC = baseline_barrier_allreduce.cu

.PHONY: all run clean
all: $(TARGET)

$(TARGET): $(SRC) ../common/gpu_common.cuh
	$(NVCC) $(NVCCFLAGS) -o $(TARGET) $(SRC)

# Usage: make run [NGPUS=4] [NFLOATS=65536] [ITERS=100]
run: $(TARGET)
	./$(TARGET) $(NGPUS) $(NFLOATS) $(ITERS)

clean:
	rm -f $(TARGET)
