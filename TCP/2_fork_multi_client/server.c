#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT     8082
#define BUF_SIZE 1024

static void handle_client(int conn_fd, struct sockaddr_in *client_addr) {
    char buf[BUF_SIZE];
    printf("[pid %d] handling client %s:%d\n", getpid(),
           inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));

    /* Echo loop: keep going until the client closes the connection.
     * This is one fork()ed child handling ONE connection, so it can
     * block on recv() as long as it wants without affecting anyone else -
     * every other client is a completely separate process. */
    for (;;) {
        ssize_t n = recv(conn_fd, buf, sizeof(buf) - 1, 0);
        if (n < 0) { perror("recv"); break; }
        if (n == 0) {
            printf("[pid %d] client disconnected\n", getpid());
            break;
        }
        buf[n] = '\0';
        printf("[pid %d] received \"%s\"\n", getpid(), buf);
        if (send(conn_fd, buf, (size_t)n, 0) < 0) { perror("send"); break; }
    }
    close(conn_fd);
}

int main(void) {
    int listen_fd;
    struct sockaddr_in server_addr;

    /* Force line-buffered stdout instead of the default full buffering
     * (which kicks in whenever stdout isn't a live terminal - e.g. when
     * redirected to a file or a pipe, as in a lot of real deployments).
     * Without this, fork() can duplicate whatever's still sitting in the
     * stdio buffer into every child via copy-on-write, and each child then
     * flushes its own copy on exit - you'd see the SAME buffered line
     * printed once per child. Line-buffering flushes after every '\n',
     * so nothing is ever left sitting around to get duplicated. */
    setvbuf(stdout, NULL, _IOLBF, 0);

    /* Without this, every child that exits becomes a zombie until the
     * parent reaps it with wait()/waitpid(). SIG_IGN tells the kernel to
     * auto-reap children - fine here since we never need their exit status. */
    signal(SIGCHLD, SIG_IGN);

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

    printf("[pid %d] server: listening on port %d...\n", getpid(), PORT);

    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (conn_fd < 0) {
            perror("accept");
            continue; /* one bad accept() shouldn't kill the whole server */
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(conn_fd);
            continue;
        }

        if (pid == 0) {
            /* Child process: it inherited BOTH fds (listen_fd and conn_fd)
             * from the fork(), but it only needs conn_fd. Closing the
             * listening socket here matters - otherwise every child keeps
             * the listening port referenced too. */
            close(listen_fd);
            handle_client(conn_fd, &client_addr);
            exit(0);
        }

        /* Parent process: it doesn't need this specific connection, only
         * the listening socket, so it closes conn_fd and loops back to
         * accept() the next one. */
        close(conn_fd);
    }

    return 0;
}
