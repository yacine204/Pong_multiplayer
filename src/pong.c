#include <raylib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "pong.h"

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 620
#define PADDLE_HEGHT 100
#define PADDLE_WIDTH 15
#define PADDLE_MARGIN_X 20

static void SEND_Y(int fd, int posY) {
    uint16_t v = htons((uint16_t)posY);
    send(fd, &v, sizeof v, 0);
}

static void RECV_Y(int fd, Player *opp) {
    uint16_t v;
    while (recv(fd, &v, sizeof v, MSG_DONTWAIT) == (ssize_t)sizeof v)
        opp->posY = ntohs(v);
}

static void PADDLE_UPDATE(Player *p) {
    if (IsKeyDown(KEY_W)) p->posY -= 10;
    if (IsKeyDown(KEY_S)) p->posY += 10;
    if (p->posY < 0) p->posY = 0;
    if (p->posY + PADDLE_HEGHT > SCREEN_HEIGHT) p->posY = SCREEN_HEIGHT - PADDLE_HEGHT;
}

static void PADDLE_RENDERER(const Conn *conn) {
    for (int i = 0; i < 2; i++)
        DrawRectangle(conn->party[i].posX, conn->party[i].posY,
                      PADDLE_WIDTH, PADDLE_HEGHT, WHITE);
}

static void SERVE_GAME(Conn *conn, int fd, int local) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Pong");
    SetTargetFPS(60);

    conn->party[LEFT] = (Player){ .side = LEFT, .posX = PADDLE_MARGIN_X,
        .posY = GetRandomValue(0, SCREEN_HEIGHT - PADDLE_HEGHT) };
    conn->party[RIGHT] = (Player){ .side = RIGHT,
        .posX = SCREEN_WIDTH - PADDLE_MARGIN_X - PADDLE_WIDTH,
        .posY = GetRandomValue(0, SCREEN_HEIGHT - PADDLE_HEGHT) };

    int remote = 1 - local;

    while (!WindowShouldClose()) {
        PADDLE_UPDATE(&conn->party[local]);
        SEND_Y(fd, conn->party[local].posY);
        RECV_Y(fd, &conn->party[remote]);

        BeginDrawing();
            ClearBackground(DARKGRAY);
            PADDLE_RENDERER(conn);
        EndDrawing();
    }
    CloseWindow();
}

static int read_full(int fd, void *buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        ssize_t r = recv(fd, (char *)buf + got, n - got, 0);
        if (r <= 0) return -1;
        got += r;
    }
    return 0;
}

// usage: ./pong <server_ip> <server_port>
int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <server_ip> <server_port>\n", argv[0]);
        return 1;
    }


    int tcp = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &srv.sin_addr) != 1) { fprintf(stderr, "bad server ip\n"); return 1; }
    if (connect(tcp, (struct sockaddr *)&srv, sizeof srv) < 0) { perror("connect tcp"); return 1; }


    int udp = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in me = {0};
    me.sin_family = AF_INET;
    me.sin_addr.s_addr = htonl(INADDR_ANY);
    me.sin_port = 0;
    if (bind(udp, (struct sockaddr *)&me, sizeof me) < 0) { perror("bind udp"); return 1; }

    socklen_t len = sizeof me;
    if (getsockname(udp, (struct sockaddr *)&me, &len) < 0) { perror("getsockname"); return 1; }


    uint16_t port = me.sin_port;
    if (send(tcp, &port, sizeof port, 0) != sizeof port) { perror("send port"); return 1; }


    printf("Waiting for an opponent...\n");
    Match m;
    if (read_full(tcp, &m, sizeof m) < 0) { fprintf(stderr, "server closed the connection\n"); return 1; }
    printf("Matched, You are %s\n", m.side == LEFT ? "LEFT" : "RIGHT");


    struct sockaddr_in peer = {0};
    peer.sin_family = AF_INET;
    peer.sin_addr.s_addr = m.ip;
    peer.sin_port = m.port;
    if (connect(udp, (struct sockaddr *)&peer, sizeof peer) < 0) { perror("connect udp"); return 1; }

    Conn conn = {0};
    SERVE_GAME(&conn, udp, m.side);

    close(udp);
    close(tcp);
    return 0;
}