# Builds the two-shot LL AllReduce demo.
ARCH ?= native
NVCC ?= nvcc
NVCCFLAGS = -O3 -std=c++17 -arch=$(ARCH)

TARGET = twoshot_ll_allreduce
SRC = twoshot_ll_allreduce.cu

.PHONY: all run clean
all: $(TARGET)

$(TARGET): $(SRC) ../common/gpu_common.cuh
	$(NVCC) $(NVCCFLAGS) -o $(TARGET) $(SRC)

# Usage: make run [NGPUS=4] [NFLOATS=65536] [ITERS=100]
run: $(TARGET)
	./$(TARGET) $(NGPUS) $(NFLOATS) $(ITERS)

clean:
	rm -f $(TARGET)
