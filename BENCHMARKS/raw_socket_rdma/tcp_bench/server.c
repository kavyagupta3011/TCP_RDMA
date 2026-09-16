#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define PORT     30001
#define MAX_SIZE (4 * 1024 * 1024)  /* largest message size in the sweep */

#define MODE_LATENCY   0
#define MODE_BANDWIDTH 1
#define MODE_DONE      2
#define CTRL_MAGIC     0xB1DEC0DEu

/* One fixed-size control message the client sends before each phase, so
 * the server always knows exactly what to expect next - same idea as
 * 6_length_prefixed_protocol's framing, just carrying benchmark
 * parameters instead of a payload length. */
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

int main(void) {
    int listen_fd, conn_fd;
    struct sockaddr_in addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); exit(1); }
    if (listen(listen_fd, 1) < 0) { perror("listen"); exit(1); }

    printf("server: listening on port %d (TCP bandwidth/latency benchmark)...\n", PORT);

    conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
    if (conn_fd < 0) { perror("accept"); exit(1); }
    printf("server: client connected from %s:%d\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    /* Disable Nagle's algorithm. Without this, the kernel can hold small
     * outgoing packets for a short while hoping to coalesce them with more
     * data - fine for a chat app, fatal for a latency benchmark, where
     * that delay would swamp the actual number we're trying to measure. */
    int one = 1;
    setsockopt(conn_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    char *buf = malloc(MAX_SIZE);
    if (!buf) { perror("malloc"); exit(1); }

    for (;;) {
        struct ctrl_msg ctrl;
        if (recv_all(conn_fd, &ctrl, sizeof(ctrl)) < 0) {
            printf("server: client disconnected\n");
            break;
        }
        if (ctrl.magic != CTRL_MAGIC) {
            fprintf(stderr, "server: bad control message, aborting\n");
            break;
        }
        if (ctrl.mode == MODE_DONE) {
            printf("server: client signaled done\n");
            break;
        }

        if (ctrl.mode == MODE_LATENCY) {
            /* Plain echo, exactly `iters` times, exactly `size` bytes each.
             * The CLIENT does all the timing; we just bounce data back as
             * fast as we can. */
            for (uint32_t i = 0; i < ctrl.iters; i++) {
                if (recv_all(conn_fd, buf, ctrl.size) < 0) { perror("recv"); exit(1); }
                if (send_all(conn_fd, buf, ctrl.size) < 0) { perror("send"); exit(1); }
            }
        } else if (ctrl.mode == MODE_BANDWIDTH) {
            /* Receive iters * size bytes back to back with no reply per
             * message, then send one tiny ack once it has all arrived -
             * that's the client's cue to stop its clock. */
            for (uint32_t i = 0; i < ctrl.iters; i++) {
                if (recv_all(conn_fd, buf, ctrl.size) < 0) { perror("recv"); exit(1); }
            }
            char ack = 1;
            if (send_all(conn_fd, &ack, 1) < 0) { perror("send ack"); exit(1); }
        } else {
            fprintf(stderr, "server: unknown mode %u\n", ctrl.mode);
            break;
        }
    }

    free(buf);
    close(conn_fd);
    close(listen_fd);
    return 0;
}
