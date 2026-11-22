#ifndef PONG_H
#define PONG_H

#include <stdint.h>

enum { LEFT, RIGHT };

typedef struct {
    int side;
    int posX, posY;
} Player;

typedef struct {
    Player party[2];
    int score[2];
} Conn;

typedef struct __attribute__((packed)) {
    uint8_t  side;   
    uint32_t ip;     
    uint16_t port;   
} Match;

#endif