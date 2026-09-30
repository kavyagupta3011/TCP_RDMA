# Builds the bidirectional communication + double buffering demo.
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

TARGET = bidir_doublebuffer
SRC = bidir_doublebuffer.cu

.PHONY: all run clean
all: $(TARGET)

$(TARGET): $(SRC) ../common/gpu_common.cuh
	$(NVCC) $(NVCCFLAGS) -o $(TARGET) $(SRC)

# Usage: make run [NFLOATS=262144] [NCHUNKS=8] [ITERS=100]
# (Always uses exactly 2 GPUs -- see the file header for why.)
run: $(TARGET)
	./$(TARGET) $(NFLOATS) $(NCHUNKS) $(ITERS)

clean:
	rm -f $(TARGET)
