#ifndef BASIC_H
#define BASIC_H

#include <stdint.h>

#define count_of(x) sizeof((x))/sizeof((x)[0])
#define clamp(x, min, max) ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))

#define KEY_ANY                0
#define KEY_A                  1
#define KEY_B                  2
#define KEY_C                  3
#define KEY_D                  4
#define KEY_E                  5
#define KEY_F                  6
#define KEY_G                  7
#define KEY_H                  8
#define KEY_I                  9
#define KEY_J                  10
#define KEY_K                  11
#define KEY_L                  12
#define KEY_M                  13
#define KEY_N                  14
#define KEY_O                  15
#define KEY_P                  16
#define KEY_Q                  17
#define KEY_R                  18
#define KEY_S                  19
#define KEY_T                  20
#define KEY_U                  21
#define KEY_V                  22
#define KEY_W                  23
#define KEY_X                  24
#define KEY_Y                  25
#define KEY_Z                  26
#define KEY_1                  27
#define KEY_2                  28
#define KEY_3                  29
#define KEY_4                  30
#define KEY_5                  31
#define KEY_6                  32
#define KEY_7                  33
#define KEY_8                  34
#define KEY_9                  35
#define KEY_0                  36
#define KEY_ENTER              37
#define KEY_ESCAPE             38
#define KEY_BACKSPACE          39
#define KEY_TAB                40
#define KEY_SPACE              41
#define KEY_MINUS              42
#define KEY_EQUAL              43
#define KEY_LBRACE             44
#define KEY_RBRACE             45
#define KEY_BACKSLASH          46
#define KEY_SEMICOLON          47
#define KEY_APOSTROPHE         48
#define KEY_GRAVEACCENT        49
#define KEY_COMMA              50
#define KEY_PERIOD             51
#define KEY_SLASH              52
#define KEY_F1                 53
#define KEY_F2                 54
#define KEY_F3                 55
#define KEY_F4                 56
#define KEY_F5                 57
#define KEY_F6                 58
#define KEY_F7                 59
#define KEY_F8                 60
#define KEY_F9                 61
#define KEY_F10                62
#define KEY_F11                63
#define KEY_F12                64
#define KEY_RIGHT              65
#define KEY_LEFT               66
#define KEY_DOWN               67
#define KEY_UP                 68
#define KEY_CAPSLOCK           69
#define KEY_LCONTROL           70
#define KEY_LALT               71
#define KEY_LSHIFT             72
#define KEY_LMETA              73
#define KEY_RSHIFT             74

#define BUTTON_ANY             0
#define BUTTON_LEFT            1
#define BUTTON_RIGHT           2
#define BUTTON_MIDDLE          3

#define WINDOW_MODE_INVALID    0
#define WINDOW_MODE_WINDOWED   1
#define WINDOW_MODE_MAXIMIZED  2
#define WINDOW_MODE_FULLSCREEN 3

#define WINDOW_BACKEND_INVALID 0
#define WINDOW_BACKEND_WIN32   1
#define WINDOW_BACKEND_WAYLAND 2
#define WINDOW_BACKEND_XCB     3

#define APP_EVENT_INVALID      0
#define APP_EVENT_KEY_UP       1
#define APP_EVENT_KEY_DOWN     2
#define APP_EVENT_MOUSE_UP     3
#define APP_EVENT_MOUSE_DOWN   4
#define APP_EVENT_MOUSE_MOVE   5
#define APP_EVENT_MOUSE_SCROLL 6
#define APP_EVENT_WINDOW_SIZE  7
#define APP_EVENT_CLOSE        8

struct app_event {
    uint32_t kind;
    union {
        struct { uint32_t w, h;              } resize;
        struct { uint32_t code;              } key;
        struct { int32_t  x, y;              } mouse_move;
        struct { int32_t  x, y; int32_t btn; } mouse_button;
    };
};

typedef struct HWND__* HWND;
typedef struct HINSTANCE__* HINSTANCE;

struct window_win32 {
    HWND      hwnd;
    HINSTANCE hinstance;
};

struct wl_surface;
struct wl_display;

struct window_wl {
    struct wl_surface* surface;
    struct wl_display* display;
};

struct window_xcb {
    void* window;
    void* display;
};

struct window {
    union {
        struct window_win32 win32;
        struct window_wl    wl;
        struct window_xcb   xcb;
    };
    uint32_t            width;
    uint32_t            height;
    uint32_t            backend;
    uint32_t            mode;
    const char*         title;
};

typedef uint32_t (*const app_fn_init)(void);
typedef void     (*const app_fn_update)(void);
typedef void     (*const app_fn_terminate)(void);
typedef void     (*const app_fn_event)(struct app_event*);

struct app_desc {
    struct window*         window;
    const app_fn_init      fn_init;
    const app_fn_update    fn_update;
    const app_fn_terminate fn_terminate;
    const app_fn_event     fn_event;
    uint32_t               target_fps;
};

extern const struct app_desc* load_app(void);

void quit_app(void);

#endif /* BASIC_H */
