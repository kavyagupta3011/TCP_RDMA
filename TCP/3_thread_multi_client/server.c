#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT     8083
#define BUF_SIZE 1024

typedef struct {
    int conn_fd;
    struct sockaddr_in addr;
} client_ctx_t;

static void *handle_client(void *arg) {
    /* Ownership of this heap block was handed to us by main() - we free it
     * ourselves once we're done, see the note down in main() about why it's
     * heap-allocated per connection instead of a stack/loop variable. */
    client_ctx_t *ctx = (client_ctx_t *)arg;
    int conn_fd = ctx->conn_fd;
    char buf[BUF_SIZE];

    printf("[tid %lu] handling client %s:%d\n", (unsigned long)pthread_self(),
           inet_ntoa(ctx->addr.sin_addr), ntohs(ctx->addr.sin_port));

    for (;;) {
        ssize_t n = recv(conn_fd, buf, sizeof(buf) - 1, 0);
        if (n < 0) { perror("recv"); break; }
        if (n == 0) {
            printf("[tid %lu] client disconnected\n", (unsigned long)pthread_self());
            break;
        }
        buf[n] = '\0';
        printf("[tid %lu] received \"%s\"\n", (unsigned long)pthread_self(), buf);
        if (send(conn_fd, buf, (size_t)n, 0) < 0) { perror("send"); break; }
    }

    close(conn_fd);
    free(ctx);
    return NULL;
}

int main(void) {
    int listen_fd;
    struct sockaddr_in server_addr;

    setvbuf(stdout, NULL, _IOLBF, 0); /* see 2_fork_multi_client for why */

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

    printf("server: listening on port %d (thread-per-connection)...\n", PORT);

    for (;;) {
        client_ctx_t *ctx = malloc(sizeof(client_ctx_t));
        if (!ctx) { perror("malloc"); continue; }

        socklen_t addr_len = sizeof(ctx->addr);
        ctx->conn_fd = accept(listen_fd, (struct sockaddr *)&ctx->addr, &addr_len);
        if (ctx->conn_fd < 0) {
            perror("accept");
            free(ctx);
            continue;
        }

        /* Heap-allocate a fresh context per connection and hand ownership
         * to the new thread. A classic bug here is instead passing the
         * address of ONE reused stack/loop variable to every
         * pthread_create() call - all threads would end up reading and
         * racing on the SAME memory as the next connection overwrites it.
         * A separate malloc() per connection avoids that entirely; the
         * thread itself frees it when done (see handle_client above). */
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, ctx) != 0) {
            perror("pthread_create");
            close(ctx->conn_fd);
            free(ctx);
            continue;
        }

        /* We're never going to pthread_join() this thread, so detach it -
         * its resources are released automatically the moment it returns
         * instead of leaking until some other thread joins it. */
        pthread_detach(tid);
    }

    return 0;
}
