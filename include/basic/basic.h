#ifndef BASIC_H
#define BASIC_H

#include <stddef.h>
#include <stdint.h>

#define unused(x) (void)(x)
#define container_of(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))

/*************************************************************************
 *********************** native
 ************************************************************************/

enum WINDOW_API {
    WINDOW_API_WIN32,
    WINDOW_API_WAYLAND,
    WINDOW_API_COUNT,
};

typedef struct HWND__* HWND;
typedef struct HINSTANCE__* HINSTANCE;
struct win32_state {
    HWND hwnd;
    HINSTANCE hinstance;
};

void get_win32_state(struct win32_state* s);

/* WAYLAND */
struct wl_display;
struct wl_surface;

struct wayland_state {
    struct wl_display* display;
    struct wl_surface* surface;
};

void get_wayland_state(struct wayland_state);

/* Other platforms... if any... */

/*************************************************************************
 *********************** game
 ************************************************************************/

struct game_itf {
    uint32_t (*init)(void);
    void     (*update)(void);
    void     (*terminate)(void);
    uint32_t running;
};

extern struct game_itf* create_game(void);

/*************************************************************************
 *********************** platform
 ************************************************************************/

/* TODO: this header is shared with the game, for now,
 * exposing this entire interface to the game 
 * might not be a good idea. Maybe move to basic_internal.h?
 */
struct platform_itf {
    double (*const now)(void);
    void   (*const sleep)(uint32_t ms);
    void   (*const poll_events)(void);
    const enum WINDOW_API window_api;
};

extern const struct platform_itf platform;

uint32_t run_game(const struct game_itf* g);

#endif
