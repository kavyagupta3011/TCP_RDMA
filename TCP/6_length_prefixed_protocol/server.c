#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT     8086
#define MAX_MSG  65536

/* recv_all(): loop recv() until exactly len bytes are read, or the
 * connection ends. Returns 0 on success, -1 on hard error, 1 if the peer
 * closed cleanly before delivering len bytes. This is the "1_partial_io"
 * lesson turned into a reusable building block. */
static int recv_all(int fd, void *buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        ssize_t n = recv(fd, (char *)buf + got, len - got, 0);
        if (n < 0) { perror("recv"); return -1; }
        if (n == 0) return 1;
        got += (size_t)n;
    }
    return 0;
}

static int send_all(int fd, const void *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, (const char *)buf + sent, len - sent, 0);
        if (n < 0) { perror("send"); return -1; }
        sent += (size_t)n;
    }
    return 0;
}

/* recv_msg(): read one length-prefixed message off the wire:
 *
 *     [ 4-byte length, network byte order ][ that many payload bytes ]
 *
 * This is how we recover message boundaries on top of a protocol that
 * doesn't have any. The receiver ALWAYS knows exactly how many payload
 * bytes to read before it reads a single one of them - no guessing, no
 * relying on a delimiter that might appear inside binary payload data.
 *
 * Returns payload length (>= 0) on success, -1 on error, 0 if the peer
 * closed cleanly between messages (a normal way for a session to end). */
static ssize_t recv_msg(int fd, char *buf, size_t buf_cap) {
    uint32_t net_len;
    int rc = recv_all(fd, &net_len, sizeof(net_len));
    if (rc == 1) return 0;   /* closed cleanly right at a message boundary */
    if (rc < 0) return -1;

    uint32_t len = ntohl(net_len);
    if (len > buf_cap) {
        fprintf(stderr, "recv_msg: message too big (%u > %zu)\n", len, buf_cap);
        return -1;
    }

    rc = recv_all(fd, buf, len);
    if (rc != 0) return -1;  /* closed mid-message -> the framing is broken, treat as error */
    return (ssize_t)len;
}

static int send_msg(int fd, const char *buf, size_t len) {
    uint32_t net_len = htonl((uint32_t)len);
    if (send_all(fd, &net_len, sizeof(net_len)) < 0) return -1;
    if (send_all(fd, buf, len) < 0) return -1;
    return 0;
}

int main(void) {
    int listen_fd, conn_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind"); exit(1);
    }
    if (listen(listen_fd, 5) < 0) { perror("listen"); exit(1); }

    printf("server: listening on port %d (length-prefixed framing)...\n", PORT);

    conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
    if (conn_fd < 0) { perror("accept"); exit(1); }
    printf("server: client connected from %s:%d\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    char buf[MAX_MSG];
    for (;;) {
        ssize_t n = recv_msg(conn_fd, buf, sizeof(buf));
        if (n < 0) {
            fprintf(stderr, "server: framing error, dropping connection\n");
            break;
        }
        if (n == 0) {
            printf("server: client closed the connection\n");
            break;
        }

        printf("server: received message (%zd bytes): \"%.*s\"\n", n, (int)n, buf);

        if (send_msg(conn_fd, buf, (size_t)n) < 0) {
            fprintf(stderr, "server: failed to send reply\n");
            break;
        }
    }

    close(conn_fd);
    close(listen_fd);
    return 0;
}
