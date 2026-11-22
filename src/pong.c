#include <raylib.h>

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 620

// for each connection render a screen and for each client render their paddle depending on their position.

int main(){
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Pong");
    SetTargetFPS(60);
    while(!WindowShouldClose()){
        BeginDrawing();
            ClearBackground(DARKGRAY);
        EndDrawing();
    }
    
}