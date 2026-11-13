#include <stdio.h>
#include <arpa/inet.h>
#include <threads.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <err.h>
#include <errno.h>
#include <time.h>

typedef struct{
    struct pollfd *poll_list;
    nfds_t *nfds;
}read_args;

#define MAX_CLIENTS 100


int read_clients_data(void *arg) {
    read_args *a = arg;
    char buf[1024];

    for (;;) {
        nfds_t n = *a->nfds;

        if (n <= 1) {                          
            struct timespec ts = { .tv_sec = 0, .tv_nsec = 10 * 1000 * 1000 };
            thrd_sleep(&ts, NULL);
            continue;
        }

        int ready = poll(&a->poll_list[1], n - 1, -1);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("poll(clients)");
            break;
        }
        if (ready == 0) continue;

        for (nfds_t i = 1; i < *a->nfds; i++) {
            if (!(a->poll_list[i].revents & (POLLIN | POLLHUP | POLLERR)))
                continue;

            ssize_t n = recv(a->poll_list[i].fd, buf, sizeof buf - 1, 0);
            if (n > 0) {
                buf[n] = '\0';
                printf("fd %d: %s\n", a->poll_list[i].fd, buf);
            } else if (n == 0) {
                close(a->poll_list[i].fd);
                a->poll_list[i] = a->poll_list[--(*a->nfds)];
                i--;
            } else {
                if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                    perror("recv");
                    close(a->poll_list[i].fd);
                    a->poll_list[i] = a->poll_list[--(*a->nfds)];
                    i--;
                }
            }
        }
    }
    return 0;
}

int main(){


    nfds_t nfds=0;
    struct pollfd *poll_list = malloc(sizeof(struct pollfd)*MAX_CLIENTS); 
    read_args *args = malloc(sizeof(read_args));
    struct sockaddr_in server;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_family = AF_INET;
    server.sin_port = htons(8080);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd<0){
        perror("socket failed");
        return 1;
    }


    poll_list[0].fd = server_fd;
    poll_list[0].events = POLLIN;
    poll_list[0].revents = 0;

    args->nfds = &nfds;
    args->poll_list= poll_list;

    int opt = 1;

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if((bind(server_fd, (struct sockaddr*)&server, sizeof(server)))<0){
        perror("binding failed");
        return 1;
    }

    
    if((listen(server_fd, 1))<0){
        perror("listen failed");
        return 1;
    }else{
        printf("Server listening on %s:%d\n",
       inet_ntoa(server.sin_addr), ntohs(server.sin_port));
    }

    thrd_t t;
    thrd_create(&t, read_clients_data, (void *)args);

    for(;;){
        int cfd = accept(server_fd, NULL, NULL);
        if (cfd < 0) { perror("accept"); continue; }

        if(cfd<MAX_CLIENTS){
            poll_list[nfds].fd = cfd;
            poll_list[nfds].events = POLLIN;
            poll_list[nfds].revents = 0;
            nfds++;                              
        } else {
            close(cfd);
        
        }
    }
    
    close(server_fd);
    free(poll_list);
    free(args);
    return 0;
}