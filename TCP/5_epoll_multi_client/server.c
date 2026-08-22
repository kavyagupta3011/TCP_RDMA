#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#define PORT       8085
#define BUF_SIZE   1024
#define MAX_EVENTS 32

int main(void) {
    int listen_fd, epoll_fd;
    struct sockaddr_in server_addr;

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

    /* epoll_create1(0): ask the kernel for an epoll instance, itself
     * represented by a file descriptor you can epoll_ctl()/epoll_wait() on. */
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) { perror("epoll_create1"); exit(1); }

    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN;   /* level-triggered: "tell me whenever this fd is readable" */
    ev.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
        perror("epoll_ctl: listen_fd"); exit(1);
    }

    printf("server: listening on port %d (single-threaded, epoll-based)...\n", PORT);

    for (;;) {
        /* epoll_wait() blocks until one or more registered fds have an
         * event ready, and returns ONLY those fds (via `events`) - unlike
         * select()'s fd_set, we never rescan fds that have nothing
         * happening. That's what lets epoll scale to tens of thousands of
         * connections where select()'s O(n)-per-call rescan falls over. */
        int n_ready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n_ready < 0) { perror("epoll_wait"); break; }

        for (int i = 0; i < n_ready; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
                if (conn_fd < 0) { perror("accept"); continue; }

                printf("server: new client %s:%d -> fd %d\n",
                       inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), conn_fd);

                struct epoll_event client_ev;
                client_ev.events = EPOLLIN;
                client_ev.data.fd = conn_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &client_ev) < 0) {
                    perror("epoll_ctl: conn_fd");
                    close(conn_fd);
                }
                continue;
            }

            char buf[BUF_SIZE];
            ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
            if (n <= 0) {
                if (n < 0) perror("recv");
                else printf("server: client on fd %d disconnected\n", fd);

                /* Must deregister from epoll BEFORE closing - epoll_ctl
                 * on an already-closed fd (or worse, one the kernel has
                 * since reused for something unrelated) is a real bug class. */
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
                continue;
            }

            buf[n] = '\0';
            printf("server: fd %d sent \"%s\"\n", fd, buf);
            send(fd, buf, (size_t)n, 0);
        }
    }

    close(epoll_fd);
    close(listen_fd);
    return 0;
}
