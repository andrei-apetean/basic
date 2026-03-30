#include <basic/basic.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

struct game {
    struct game_itf itf;
};

struct game g_game = {0};
uint32_t game_init(void);
void game_update(void);
void game_terminate(void);

struct game_itf* create_game(void) {
    g_game = (struct game){
        .itf = {
        .init = game_init,
        .update = game_update,
        .terminate = game_terminate,
        },
    };
    return &g_game.itf;
}

uint32_t game_init(void){
    g_game.itf.running = 1;
    /* get the window */
    /* initialize vulkan */
    printf("game init\n");
    return 0;
}

void game_update(void) {
    static int close = 0;
    close++;
    if (close > 100) {
        struct win32_state win;
        get_win32_state(&win);
        CloseWindow(win.hwnd);
        g_game.itf.running = 0;
    }
    /* update logic */
    /* draw */
}

void game_terminate(void){
    printf("game terminate\n");
    /* shutdown vulkan */
}
