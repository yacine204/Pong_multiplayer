#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


int main(int argc, char* argv[]){
    if(argc<3){
        printf("Usage:\t./client <ip> <route>\n");
        return;
    }
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

    if(connect(fd, (struct sockaddr*)&server_addr, sizeof(server_addr))==0){
        printf("Connected to server\n");
    }else{
        perror("connect");
        close(fd);
        return 1;
    }
    
    char buffer[1024];
    while(1){
        if((fgets(buffer, sizeof(buffer), stdin))!=NULL){
            buffer[strcspn(buffer, "\n")] = '\0';
            send(fd, buffer,strlen(buffer), 0);
        }
    }
    close(fd);
    return 0;
}