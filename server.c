#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <poll.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "src/pong.h"

#define FAIL(msg) do {          \
    perror(msg);                \
    ret = -errno;               \
    goto out;                   \
} while (0)

#define CHECK_FAIL(condition, msg) do { \
    if (condition)                      \
        FAIL(msg);                      \
} while (0)

#define MAX_CONN 100

enum { NEW, WAITING };

typedef struct {
    struct sockaddr_in addr;
    uint16_t udp_port;
    int state;
} Client;

static void drop(struct pollfd *pl, Client *cl, nfds_t *nfds, nfds_t i) {
    close(pl[i].fd);
    pl[i] = pl[*nfds - 1];
    cl[i] = cl[*nfds - 1];
    (*nfds)--;
}

int main(void) {
    int ret = 0;
    int server_fd = -1;
    int opt = 1;
    nfds_t nfds = 0;

    struct pollfd *pl = malloc(sizeof(struct pollfd) * MAX_CONN);
    Client *cl = malloc(sizeof(Client) * MAX_CONN);
    CHECK_FAIL(!pl || !cl, "malloc");

    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    CHECK_FAIL((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1, "socket");
    CHECK_FAIL(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt) == -1, "setsockopt");
    CHECK_FAIL(bind(server_fd, (struct sockaddr *)&server, sizeof server) == -1, "bind");
    CHECK_FAIL(listen(server_fd, MAX_CONN) == -1, "listen");
    printf("Server listening on port %d\n", ntohs(server.sin_port));

    pl[nfds++] = (struct pollfd){ .fd = server_fd, .events = POLLIN };

    for (;;) {
        int ready = poll(pl, nfds, -1);
        if (ready < 0) {
            if (errno == EINTR) continue;
            FAIL("poll");
        }

        if (pl[0].revents & POLLIN) {
            struct sockaddr_in addr;
            socklen_t len = sizeof addr;
            int fd = accept(server_fd, (struct sockaddr *)&addr, &len);
            if (fd >= 0 && nfds < MAX_CONN) {
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof ip);
                printf("[%s:%d] connected\n", ip, ntohs(addr.sin_port));
                pl[nfds] = (struct pollfd){ .fd = fd, .events = POLLIN };
                cl[nfds] = (Client){ .addr = addr, .state = NEW };
                nfds++;
            } else if (fd >= 0) {
                close(fd);
            }
        }

        for (nfds_t i = 1; i < nfds; i++) {
            if (!(pl[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;

            uint16_t port;
            ssize_t n = recv(pl[i].fd, &port, sizeof port, 0);
            if (n != (ssize_t)sizeof port || cl[i].state != NEW) {
                printf("client %u left\n", (unsigned)i);
                drop(pl, cl, &nfds, i);
                i--;
                continue;
            }

            cl[i].udp_port = port;
            cl[i].state = WAITING;

            nfds_t j = 0;
            for (nfds_t k = 1; k < nfds; k++)
                if (k != i && cl[k].state == WAITING) { j = k; break; }
            if (!j) continue;

            Match to_j = { .side = LEFT,  .ip = cl[i].addr.sin_addr.s_addr, .port = cl[i].udp_port };
            Match to_i = { .side = RIGHT, .ip = cl[j].addr.sin_addr.s_addr, .port = cl[j].udp_port };
            send(pl[j].fd, &to_j, sizeof to_j, 0);
            send(pl[i].fd, &to_i, sizeof to_i, 0);
            printf("matched client %u with %u\n", (unsigned)i, (unsigned)j);

            nfds_t hi = i > j ? i : j, lo = i > j ? j : i;
            drop(pl, cl, &nfds, hi);
            drop(pl, cl, &nfds, lo);
            i = 0;
        }
    }

out:
    if (server_fd != -1) close(server_fd);
    free(pl);
    free(cl);
    return ret;
}