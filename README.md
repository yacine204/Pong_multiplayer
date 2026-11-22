# Pong

Two-player Pong matchmaking system using poll based TCP to set up a UDP state exchange.

## Build

```
gcc server.c -o server
gcc src/pong.c -o pong -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
```

## Usage

```
./server
./pong <server_ip> <server_port>
```

LAN only.