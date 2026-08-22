#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT       8081
#define RECV_CHUNK 4096   /* deliberately small vs. the payload the client sends */

int main(void) {
    int listen_fd, conn_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Shrink the receive buffer on purpose. The kernel enforces its own
     * minimum and doubles whatever you ask for internally, but this still
     * makes partial reads happen reliably once the payload is a few MB. */
    int rcvbuf = 8192;
    setsockopt(listen_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind"); exit(1);
    }
    if (listen(listen_fd, 5) < 0) { perror("listen"); exit(1); }

    printf("server: listening on port %d...\n", PORT);

    conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
    if (conn_fd < 0) { perror("accept"); exit(1); }
    printf("server: client connected from %s:%d\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    char buf[RECV_CHUNK];
    size_t total_bytes = 0;
    int recv_calls = 0;

    /* Read until the client signals it's done sending (recv() returns 0,
     * i.e. the peer sent a FIN). There's no message boundary here - we
     * don't know how many bytes are coming, only that we keep reading
     * until EOF. That's TCP's byte-stream nature in action. */
    for (;;) {
        ssize_t n = recv(conn_fd, buf, sizeof(buf), 0);
        if (n < 0) { perror("recv"); break; }
        if (n == 0) {
            printf("server: recv() returned 0 -> client is done sending (EOF)\n");
            break;
        }
        recv_calls++;
        total_bytes += (size_t)n;
        if (recv_calls <= 5 || recv_calls % 100 == 0) {
            printf("server: recv() call #%d returned %zd bytes (running total %zu)\n",
                   recv_calls, n, total_bytes);
        }
    }

    printf("server: total %zu bytes received across %d recv() calls "
           "(no single recv() returned the whole payload)\n",
           total_bytes, recv_calls);

    close(conn_fd);
    close(listen_fd);
    return 0;
}
