#ifndef BASIC_NATIVE_H
#define BASIC_NATIVE_H

/* WIN32 */
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


#endif /* BASIC_NATIVE_H */
