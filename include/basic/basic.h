#ifndef BASIC_H
#define BASIC_H

#include <stddef.h>
#include <stdint.h>

#define unused(x) (void)(x)
#define container_of(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))

struct game_itf {
    uint32_t (*init)(void);
    void     (*update)(void);
    void     (*terminate)(void);
    uint32_t running;
};

enum WINDOW_API {
    WINDOW_API_WIN32,
    WINDOW_API_LINUX,
    WINDOW_API_COUNT,
};

/* TODO: this header is shared with the game, for now,
 * exposing this entire interface to the game 
 * might not be a good idea. 
 */

struct platform_itf {
    double (*const now)(void);
    void   (*const sleep)(uint32_t ms);
    void   (*const poll_events)(void);
    const enum WINDOW_API window_api;
};

extern const struct platform_itf platform;

extern struct game_itf* create_game(void);

uint32_t run_game(const struct game_itf* g);

#endif
