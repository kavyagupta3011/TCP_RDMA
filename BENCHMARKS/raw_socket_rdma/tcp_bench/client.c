#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define SERVER_IP "127.0.0.1"
#define PORT      30001
#define MAX_SIZE  (4 * 1024 * 1024)

#define MODE_LATENCY   0
#define MODE_BANDWIDTH 1
#define MODE_DONE      2
#define CTRL_MAGIC     0xB1DEC0DEu

struct ctrl_msg {
    uint32_t magic;
    uint32_t mode;
    uint32_t size;
    uint32_t iters;
};

static int recv_all(int fd, void *buf, size_t len) {
    size_t got = 0;
    char *p = (char *)buf;
    while (got < len) {
        ssize_t n = recv(fd, p + got, len - got, 0);
        if (n <= 0) return -1;
        got += (size_t)n;
    }
    return 0;
}

static int send_all(int fd, const void *buf, size_t len) {
    size_t sent = 0;
    const char *p = (const char *)buf;
    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* How many round trips to time at each size. Small messages need many
 * repeats because any single one finishes faster than the clock can
 * meaningfully resolve; large messages need fewer because each one
 * individually already takes a while. */
static uint32_t latency_iters(size_t size) {
    if (size <= 8192) return 1000;
    if (size <= 131072) return 300;
    return 50;
}

/* Aim for roughly the same total bytes moved (~8MB) at every size, so
 * small messages still get a large enough sample to time accurately and
 * huge messages don't take forever. */
static uint32_t bandwidth_iters(size_t size) {
    uint32_t iters = (uint32_t)((8UL * 1024 * 1024) / size);
    if (iters < 5) iters = 5;
    if (iters > 2000) iters = 2000;
    return iters;
}

int main(void) {
    int fd;
    struct sockaddr_in addr;

    size_t sizes[] = {
        1, 2, 4, 8, 16, 32, 64, 128, 256, 512,
        1024, 2048, 4096, 8192, 16384, 32768, 65536,
        131072, 262144, 524288, 1048576, 2097152, 4194304
    };
    int n_sizes = (int)(sizeof(sizes) / sizeof(sizes[0]));

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("connect"); exit(1); }

    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    char *buf = malloc(MAX_SIZE);
    if (!buf) { perror("malloc"); exit(1); }
    memset(buf, 'A', MAX_SIZE);

    printf("client: connected to %s:%d\n\n", SERVER_IP, PORT);

    printf("=== Latency (ping-pong, one-way = RTT / 2) ===\n");
    printf("%12s %18s\n", "Size(B)", "Latency(us)");
    for (int i = 0; i < n_sizes; i++) {
        size_t size = sizes[i];
        uint32_t iters = latency_iters(size);

        /* A couple of untimed warmup round trips - the first packets on a
         * connection can be slower, and we don't want that to skew the
         * average for tiny sizes. The server just echoes whatever count
         * we tell it, so the warmup round trips have to be included in
         * that count too - otherwise the server runs out of echoes after
         * `iters` while we're still trying to send warmup traffic, and
         * both sides deadlock waiting on each other. */
        uint32_t warmup = 2;
        uint32_t total_iters = iters + warmup;

        struct ctrl_msg ctrl = { CTRL_MAGIC, MODE_LATENCY, (uint32_t)size, total_iters };
        if (send_all(fd, &ctrl, sizeof(ctrl)) < 0) { perror("send ctrl"); exit(1); }

        for (uint32_t w = 0; w < warmup; w++) {
            send_all(fd, buf, size);
            recv_all(fd, buf, size);
        }

        double t0 = now_sec();
        for (uint32_t it = 0; it < iters; it++) {
            if (send_all(fd, buf, size) < 0) { perror("send"); exit(1); }
            if (recv_all(fd, buf, size) < 0) { perror("recv"); exit(1); }
        }
        double t1 = now_sec();

        double rtt_avg_usec = (t1 - t0) * 1e6 / iters;
        printf("%12zu %18.2f\n", size, rtt_avg_usec / 2.0);
    }

    printf("\n=== Bandwidth (streaming) ===\n");
    printf("%12s %18s\n", "Size(B)", "Bandwidth(MB/s)");
    for (int i = 0; i < n_sizes; i++) {
        size_t size = sizes[i];
        uint32_t iters = bandwidth_iters(size);

        struct ctrl_msg ctrl = { CTRL_MAGIC, MODE_BANDWIDTH, (uint32_t)size, iters };
        if (send_all(fd, &ctrl, sizeof(ctrl)) < 0) { perror("send ctrl"); exit(1); }

        double t0 = now_sec();
        for (uint32_t it = 0; it < iters; it++) {
            if (send_all(fd, buf, size) < 0) { perror("send"); exit(1); }
        }
        char ack;
        if (recv_all(fd, &ack, 1) < 0) { perror("recv ack"); exit(1); }
        double t1 = now_sec();

        double total_bytes = (double)size * (double)iters;
        double mb_per_sec = (total_bytes / (t1 - t0)) / (1024.0 * 1024.0);
        printf("%12zu %18.2f\n", size, mb_per_sec);
    }

    struct ctrl_msg done = { CTRL_MAGIC, MODE_DONE, 0, 0 };
    send_all(fd, &done, sizeof(done));

    free(buf);
    close(fd);
    return 0;
}
