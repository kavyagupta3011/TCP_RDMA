TARGET = ring_allreduce_rdma
SRC = ring_allreduce_rdma.c
CFLAGS = -O2 -Wall -Wextra -std=c11
LIBS = -lrdmacm -libverbs -lm

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(SRC)
	gcc $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)
