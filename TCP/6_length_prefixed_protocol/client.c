#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define PORT      8086
#define MAX_MSG   65536

/* Same framing helpers as server.c - see that file for the full
 * explanation of why length-prefixing exists at all. */

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

static ssize_t recv_msg(int fd, char *buf, size_t buf_cap) {
    uint32_t net_len;
    int rc = recv_all(fd, &net_len, sizeof(net_len));
    if (rc == 1) return 0;
    if (rc < 0) return -1;

    uint32_t len = ntohl(net_len);
    if (len > buf_cap) {
        fprintf(stderr, "recv_msg: message too big (%u > %zu)\n", len, buf_cap);
        return -1;
    }

    rc = recv_all(fd, buf, len);
    if (rc != 0) return -1;
    return (ssize_t)len;
}

static int send_msg(int fd, const char *buf, size_t len) {
    uint32_t net_len = htonl((uint32_t)len);
    if (send_all(fd, &net_len, sizeof(net_len)) < 0) return -1;
    if (send_all(fd, buf, len) < 0) return -1;
    return 0;
}

int main(void) {
    int sock_fd;
    struct sockaddr_in server_addr;

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("socket"); exit(1); }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect"); exit(1);
    }
    printf("client: connected\n");

    /* Three messages of very different sizes on purpose - a naive
     * "just call recv() once and hope" client would mangle these by
     * merging parts of different messages together, or splitting one
     * message across two reads with no way to tell. The 4-byte length
     * prefix is what makes each message's boundary unambiguous regardless
     * of how the bytes actually arrive off the wire. */
    const char *short_msg = "hi";

    char medium_msg[500];
    memset(medium_msg, 'B', sizeof(medium_msg));

    char large_msg[9000]; /* bigger than a single typical recv() chunk */
    memset(large_msg, 'C', sizeof(large_msg));

    struct { const char *data; size_t len; } messages[] = {
        { short_msg,  strlen(short_msg) },
        { medium_msg, sizeof(medium_msg) },
        { large_msg,  sizeof(large_msg) },
    };

    char reply[MAX_MSG];
    for (size_t i = 0; i < 3; i++) {
        if (send_msg(sock_fd, messages[i].data, messages[i].len) < 0) {
            fprintf(stderr, "client: send_msg failed\n");
            break;
        }
        printf("client: sent message %zu (%zu bytes)\n", i + 1, messages[i].len);

        ssize_t n = recv_msg(sock_fd, reply, sizeof(reply));
        if (n < 0) { fprintf(stderr, "client: recv_msg failed\n"); break; }
        if (n == 0) { printf("client: server closed the connection\n"); break; }

        int matches = (size_t)n == messages[i].len &&
                      memcmp(reply, messages[i].data, (size_t)n) == 0;
        printf("client: got echo of %zd bytes back, matches original: %s\n",
               n, matches ? "yes" : "NO");
    }

    close(sock_fd);
    return 0;
}
