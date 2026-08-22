#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT        8084
#define BUF_SIZE    1024
#define MAX_CLIENTS FD_SETSIZE

int main(void) {
    int listen_fd;
    struct sockaddr_in server_addr;
    int client_fds[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) client_fds[i] = -1;

    setvbuf(stdout, NULL, _IOLBF, 0);

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
    if (listen(listen_fd, 16) < 0) { perror("listen"); exit(1); }

    printf("server: listening on port %d (single-threaded, select-based)...\n", PORT);

    /* This entire server is ONE thread, ONE process. No fork(), no
     * pthread_create(). It juggles every client by asking the kernel,
     * once per loop iteration, "which of these fds actually have
     * something for me?" instead of blocking on any single one. */
    for (;;) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(listen_fd, &read_fds);
        int max_fd = listen_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_fds[i] != -1) {
                FD_SET(client_fds[i], &read_fds);
                if (client_fds[i] > max_fd) max_fd = client_fds[i];
            }
        }

        /* select() blocks until at least one watched fd is "readable" -
         * for the listening socket that means a new connection is waiting
         * to be accept()ed; for a client fd it means either data arrived
         * or the peer closed. NULL timeout = block indefinitely.
         * Note the cost this design has: we rebuild read_fds and rescan
         * every single client fd on EVERY loop iteration, even the ones
         * with nothing happening - that O(n) rescan per call is exactly
         * what epoll (next example) is built to avoid. */
        int ready = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (ready < 0) { perror("select"); break; }

        if (FD_ISSET(listen_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
            if (conn_fd < 0) {
                perror("accept");
            } else if (conn_fd >= FD_SETSIZE) {
                /* fd_set can only represent fds below FD_SETSIZE (1024 on
                 * Linux) - a real reason select() doesn't scale, separate
                 * from the O(n) rescan cost above. */
                fprintf(stderr, "server: fd %d too large for select(), rejecting\n", conn_fd);
                close(conn_fd);
            } else {
                int slot = -1;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (client_fds[i] == -1) { slot = i; break; }
                }
                if (slot == -1) {
                    printf("server: no free slot, rejecting new connection\n");
                    close(conn_fd);
                } else {
                    client_fds[slot] = conn_fd;
                    printf("server: new client %s:%d -> fd %d (slot %d)\n",
                           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
                           conn_fd, slot);
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int fd = client_fds[i];
            if (fd == -1 || !FD_ISSET(fd, &read_fds)) continue;

            char buf[BUF_SIZE];
            ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
            if (n <= 0) {
                if (n < 0) perror("recv");
                else printf("server: client on fd %d disconnected\n", fd);
                close(fd);
                client_fds[i] = -1;
                continue;
            }
            buf[n] = '\0';
            printf("server: fd %d sent \"%s\"\n", fd, buf);
            send(fd, buf, (size_t)n, 0);
        }
    }

    close(listen_fd);
    return 0;
}
