TARGET = baseline_barrier_rdma
SRC = baseline_barrier_rdma.c ../common/rdma_common.c
CFLAGS = -O2 -Wall -Wextra -std=c11
LIBS = -lrdmacm -libverbs -lm

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(SRC) ../common/rdma_common.h
	gcc $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)
