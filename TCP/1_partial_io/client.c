#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP    "127.0.0.1"
#define PORT         8081
#define PAYLOAD_SIZE (2 * 1024 * 1024)  /* 2 MB - big enough to force partial sends */

/* send_all(): the loop every real TCP program needs around send().
 * send() only guarantees "at least 1 byte, up to len bytes" per call
 * (when it doesn't error) - never assume it sent everything we asked
 * for just because it returned >= 0. */
static int send_all(int fd, const char *buf, size_t len, int *call_count) {
    size_t sent_total = 0;
    while (sent_total < len) {
        ssize_t n = send(fd, buf + sent_total, len - sent_total, 0);
        if (n < 0) { perror("send"); return -1; }
        (*call_count)++;
        sent_total += (size_t)n;
    }
    return 0;
}

int main(void) {
    int sock_fd;
    struct sockaddr_in server_addr;

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("socket"); exit(1); }

    /* Shrink the send buffer too, for the same reason as the server. */
    int sndbuf = 8192;
    setsockopt(sock_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect"); exit(1);
    }
    printf("client: connected, sending %d bytes...\n", PAYLOAD_SIZE);

    char *payload = malloc(PAYLOAD_SIZE);
    if (!payload) { perror("malloc"); exit(1); }
    for (size_t i = 0; i < PAYLOAD_SIZE; i++) payload[i] = (char)('A' + (i % 26));

    int send_calls = 0;
    if (send_all(sock_fd, payload, PAYLOAD_SIZE, &send_calls) < 0) {
        free(payload);
        close(sock_fd);
        exit(1);
    }
    /* On Linux, a BLOCKING socket's send() typically loops inside the
     * kernel and only returns once the whole buffer is queued (it just
     * sleeps while waiting for space), so send_calls is often 1 even with
     * a tiny SO_SNDBUF - unlike recv(), which is capped by the buffer size
     * YOU pass it every single call. send() can still return early (a
     * signal interrupts it, the socket is non-blocking and hits EAGAIN,
     * partial network failure, etc.), which is exactly why send_all()'s
     * loop is still required for portable, correct code - just don't
     * expect to reliably *observe* it happen with a blocking loopback send. */
    printf("client: sent all %d bytes using %d send() call(s)\n",
           PAYLOAD_SIZE, send_calls);

    /* We're done sending. shutdown(SHUT_WR) sends a FIN on just the write
     * half, so the server's recv() sees EOF and knows we're finished -
     * we could, in principle, still read a reply on this same socket.
     * Here we just close since this demo doesn't send one. */
    shutdown(sock_fd, SHUT_WR);

    free(payload);
    close(sock_fd);
    return 0;
}
