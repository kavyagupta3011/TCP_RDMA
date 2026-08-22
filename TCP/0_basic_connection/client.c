#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define PORT      8080
#define BUF_SIZE  1024

int main(void) {
    int sock_fd;
    struct sockaddr_in server_addr;
    char buf[BUF_SIZE];

    /* 1. socket(): same call as the server - a socket doesn't "know" yet
     *    whether it'll be used to listen or to connect. */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        exit(1);
    }

    /* 2. Build the address of the server we want to reach. */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(PORT);

    /* inet_pton(): convert "127.0.0.1" (text) into the binary struct
     * in_addr format sockets actually use. Returns 1 on success. */
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock_fd);
        exit(1);
    }

    /* 3. connect(): perform the three-way handshake (SYN / SYN-ACK / ACK).
     *    Blocks until the connection is established or fails. */
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock_fd);
        exit(1);
    }

    printf("client: connected to %s:%d\n", SERVER_IP, PORT);

    /* 4. send(): write our message onto the connection. */
    const char *msg = "hello from client";
    ssize_t sent = send(sock_fd, msg, strlen(msg), 0);
    if (sent < 0) {
        perror("send");
        close(sock_fd);
        exit(1);
    }
    printf("client: sent \"%s\" (%zd bytes)\n", msg, sent);

    /* 5. recv(): read the server's echoed reply. */
    ssize_t n = recv(sock_fd, buf, BUF_SIZE - 1, 0);
    if (n < 0) {
        perror("recv");
    } else if (n == 0) {
        printf("client: server closed the connection with no reply\n");
    } else {
        buf[n] = '\0';
        printf("client: server replied \"%s\" (%zd bytes)\n", buf, n);
    }

    /* 6. close(): tear down the connection (triggers our FIN). */
    close(sock_fd);
    return 0;
}
