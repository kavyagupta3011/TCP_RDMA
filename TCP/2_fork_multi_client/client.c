#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define PORT      8082
#define BUF_SIZE  1024

int main(void) {
    int sock_fd;
    struct sockaddr_in server_addr;
    char buf[BUF_SIZE];

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("socket"); exit(1); }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect"); exit(1);
    }
    printf("[pid %d] client: connected\n", getpid());

    /* Send a few messages on the SAME connection - one round trip each.
     * This is what makes the fork() server design (one child handling a
     * whole connection's lifetime, in a loop) worth demonstrating instead
     * of a single request/response. We run several copies of this client at
     * once to see multiple child pids
     * handled concurrently by the server. */
    for (int i = 1; i <= 3; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "message #%d from pid %d", i, getpid());

        if (send(sock_fd, msg, strlen(msg), 0) < 0) { perror("send"); break; }

        ssize_t n = recv(sock_fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        printf("[pid %d] server echoed: \"%s\"\n", getpid(), buf);

        usleep(100 * 1000); /* 100ms - just spreads the messages out visibly */
    }

    close(sock_fd);
    return 0;
}
