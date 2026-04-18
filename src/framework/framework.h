#ifndef BASIC_FRAMEWORK_H
#define BASIC_FRAMEWORK_H

#include <basic/basic.h>

#include <stdint.h>

/*******************************************************************
 *
 *
 *                     operating system
 *
 *
 ******************************************************************/

enum os_event_kind {
    OS_EVENT_KEY_PRESS,
    OS_EVENT_KEY_RELEASE,
    OS_EVENT_POINTER_MOVE,
    OS_EVENT_POINTER_SCROLL,
    OS_EVENT_POINTER_BUTTON,
    OS_EVENT_WINDOW_SIZE,
    OS_EVENT_WINDOW_CLOSE,
    OS_EVENT_KIND_COUNT,
};

struct os_event {
    enum os_event_kind kind;
    union {
        struct {
            uint32_t code;
            uint32_t utf8;
        } key;
        struct {
            uint32_t x, y;
            uint32_t dx, dy;
        } cursor;
        struct {
            uint32_t width, height;
        } window;
    };
};

enum window_backend {
    WINDOW_BACKEND_WIN32 = 0,
    WINDOW_BACKEND_WAYLAND,
    WINDOW_BACKEND_XCB,
    WINDOW_BACKEND_COUNT,
};

typedef struct HWND__* HWND;
typedef struct HINSTANCE__* HINSTANCE;

struct os_window_win32 {
    HWND      hwnd;
    HINSTANCE hinstance;
};

struct wl_surface;
struct wl_display;

struct os_window_wl {
    struct wl_surface* surface;
    struct wl_display* display;
};

struct os_window_xcb {
    void* window;
    void* display;
};

struct os_window {
    enum window_backend backend;
    uint32_t            width;
    uint32_t            height;
    union {
        struct os_window_win32 win32;
        struct os_window_wl    wl;
        struct os_window_xcb   x11;
    };
};

/*******************************************************************
 *
 *
 *                     rendering
 *
 *
 ******************************************************************/

uint32_t render_device_init_vulkan(const struct os_window* window);
void     render_device_begin_drawing_vulkan(void);
void     render_device_end_drawing_vulkan(void);
void     render_device_terminate_vulkan(void);

/*******************************************************************
 *
 *
 *                     rendering
 *
 *
 ******************************************************************/

struct loop_config {
    const struct game*      game;
    const struct os_window* window;
};

uint32_t loop_init(const struct loop_config* config);
void     loop_update(void);
void     loop_terminate(void);
void     loop_on_event(struct os_event*);

#endif /* BASIC_FRAMEWORK_H */
