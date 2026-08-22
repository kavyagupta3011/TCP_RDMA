#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT     8080
#define BUF_SIZE 1024

int main(void) {
    int listen_fd, conn_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buf[BUF_SIZE];

    /* 1. socket(): ask the kernel for a TCP socket.
     *    AF_INET    -> IPv4 address family
     *    SOCK_STREAM -> byte-stream, connection-oriented (TCP)
     *    0          -> let the kernel pick the protocol for this type (TCP) */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        exit(1);
    }

    /* Let us restart the server immediately without hitting
     * "bind: Address already in use" while the old socket sits in TIME_WAIT. */
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(1);
    }

    /* 2. Build the address we want this socket to be known as. */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;   /* listen on all local interfaces */
    server_addr.sin_port        = htons(PORT);  /* host byte order -> network byte order */

    /* 3. bind(): attach the socket to that address/port. */
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(listen_fd);
        exit(1);
    }

    /* 4. listen(): mark the socket passive (a listening socket, not one
     *    used to send/recv data directly). Backlog = 5 pending connections
     *    the kernel will queue up before it starts refusing new ones. */
    if (listen(listen_fd, 5) < 0) {
        perror("listen");
        close(listen_fd);
        exit(1);
    }

    printf("server: listening on port %d...\n", PORT);

    /* 5. accept(): block until a client completes the handshake, then
     *    return a NEW fd (conn_fd) representing that one connection.
     *    listen_fd itself is never used to send/recv data - it just
     *    keeps producing new connections. */
    conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
    if (conn_fd < 0) {
        perror("accept");
        close(listen_fd);
        exit(1);
    }

    printf("server: client connected from %s:%d\n",
           inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));

    /* 6. recv(): read whatever the client sent.
     *    Returns the number of bytes actually read (may be less than
     *    requested - see the 1_partial_io example for why that matters). */
    ssize_t n = recv(conn_fd, buf, BUF_SIZE - 1, 0);
    if (n < 0) {
        perror("recv");
    } else if (n == 0) {
        printf("server: client closed connection before sending anything\n");
    } else {
        buf[n] = '\0';
        printf("server: received \"%s\" (%zd bytes)\n", buf, n);

        /* send(): echo the same bytes back. */
        ssize_t sent = send(conn_fd, buf, n, 0);
        if (sent < 0) {
            perror("send");
        }
    }

    /* 7. close(): release both file descriptors. Closing conn_fd triggers
     *    this side's FIN toward the client. */
    close(conn_fd);
    close(listen_fd);

    printf("server: done, exiting\n");
    return 0;
}
