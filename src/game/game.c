
#include <basic/basic.h>
#include <stdio.h>

struct game g_game = {0};
uint32_t game_init(void);
void game_update(void);
void game_terminate(void);

struct game* create_game(void) {
    g_game = (struct game){
        .init = game_init,
        .update = game_update,
        .terminate = game_terminate,
    };
    return &g_game;
}

uint32_t game_init(void){
    g_game.running = 1;
    printf("game init\n");
    return 0;
}

void game_update(void) {
    printf("game update\n");
}

void game_terminate(void){
    printf("game terminate\n");
}

