// idea: Pong 1v1's system manager, expose ip (or at least just test in LAN)

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <poll.h>
#include <string.h>
#include <errno.h>
#define _GNU_SOURCE
#include <threads.h>
#include <unistd.h>

#define FAIL(msg) do {          \
    perror(msg);                \
    ret = -errno;               \
    goto out;                   \
} while (0)      

#define CHECK_FAIL(condition, msg) do { \
    if(condition) \
        FAIL(msg); \
} while (0)

#define MAX_CONN 100

int main(){
    struct pollfd *poll_list = malloc(sizeof(struct pollfd)*MAX_CONN);  
    thrd_t t;
    int server_fd = -1;
    int ret = 0;
    int opt = 1;

    nfds_t nfds = 0;

    struct sockaddr_in server, client;

    struct sockaddr_in *client_list = malloc(sizeof(struct sockaddr_in)*MAX_CONN);

    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    server.sin_addr.s_addr = INADDR_ANY;

    CHECK_FAIL((server_fd = socket(AF_INET, SOCK_STREAM, 0))==-1, "socket");

    poll_list[0] = 
    (struct pollfd){
        .fd = server_fd,
        .revents = 0,
        .events = POLLIN,
    };
    nfds++;

    CHECK_FAIL((setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))==-1),"setsockopt");
    CHECK_FAIL((bind(server_fd, (struct sockaddr*)&server, sizeof(server))==-1),"bind");

    CHECK_FAIL((listen(server_fd, MAX_CONN)==-1), "listen");

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &server.sin_addr, ip, sizeof(ip));
    printf("Server listening on: %s:%d\n", ip, ntohs(server.sin_port));

    char buf[1024];
    for(;;){
        int ready = poll(poll_list, nfds, -1);
        if (ready<0){
            if (errno==EINTR) continue;
            CHECK_FAIL(1==1, "poll");
        }

        if(poll_list[0].revents & POLLIN){
            socklen_t len = sizeof(client);
            int client_fd = accept(server_fd, (struct sockaddr*)&client, &len);
            printf("[%s:%d] Connected\n",ip, ntohs(client.sin_port));
            if (client_fd >=0 && nfds < MAX_CONN){
                poll_list[nfds] = (struct pollfd){
                    .events = POLLIN,
                    .fd = client_fd,
                };
                client_list[nfds] = client; //ignore [0] to == index on poll_list, if refactor change to client_list[nfds-1]
                nfds++;
            }else if (client_fd >= 0){
                close(client_fd);
            }
        }
        
        for (nfds_t i = 1; i<nfds; i++){
            if(poll_list[i].revents & (POLLIN | POLLHUP | POLLERR)){
                inet_ntop(AF_INET, &client.sin_addr.s_addr, ip, sizeof(ip));
                ssize_t n = recv(poll_list[i].fd, buf, sizeof(buf)-1, 0);
                if (n>0) {
                    buf[strcspn(buf, "\n")] = '\0';
                    printf("[%s:%d] : %s\n", ip, ntohs(client.sin_port), buf);
                }else{
                    printf("[%s:%d] Disconnectd\n",ip, ntohs(client.sin_port));
                    close(poll_list[i].fd);               
                    poll_list[i] = poll_list[nfds - 1];  
                    client_list[i] = client_list[nfds - 1];
                    nfds--;                               
                    i--;
                }
            }
        }
        
    }       

out: 
    if(server_fd!=-1) close(server_fd);
    free(poll_list);
    return ret;
}