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
#define MAX_VERTEX_BINDINGS 8
#define MAX_VERTEX_ATTRIBUTES 8

enum builtin_render_pipeline {
    BUILTIN_RENDER_PIPELINE_UI = 0,
    BUILTIN_RENDER_PIPELINE_COUNT,
};

enum vertex_input_rate {
    VERTEX_INPUT_RATE_VERTEX,
    VERTEX_INPUT_RATE_INSTANCE,
};

enum topology {
    TOPOLOGY_TRIANGLE_LIST,
    TOPOLOGY_TRIANGLE_STRIP,
};

enum vertex_format {
    VERTEX_FORMAT_FLOAT,
    VERTEX_FORMAT_FLOAT2,
    VERTEX_FORMAT_FLOAT3,
    VERTEX_FORMAT_FLOAT4,
};

enum blend_mode {
    BLEND_MODE_NONE,
    BLEND_MODE_ALPHA,
    BLEND_MODE_ADDITIVE,
};

enum cull_mode {
    CULL_MODE_NONE,
    CULL_MODE_FRONT,
    CULL_MODE_BACK,
};

struct vertex_attribute {
    uint32_t binding;
    uint32_t location;
    uint32_t offset;
    uint32_t format;
};

struct vertex_binding {
    uint32_t binding;
    uint32_t stride;
    uint32_t input_rate;
};

struct render_pipeline_config {
    void*                    shader_fragment;
    void*                    shader_vertex;
    struct vertex_binding*   bindings;
    struct vertex_attribute* attributes;
    uint32_t                 binding_count;
    uint32_t                 attribute_count;
    uint32_t                 shader_fragment_size;
    uint32_t                 shader_vertex_size;
    uint32_t                 push_constant_size;
    uint32_t                 blend_mode;
    uint32_t                 cull_mode;
    uint32_t                 depth_test;
    uint32_t                 depth_write;
    uint32_t                 topology;
};

uint32_t render_device_init_vulkan(const struct os_window* window);
void     render_device_begin_drawing_vulkan(void);
void     render_device_end_drawing_vulkan(void);
void     render_device_terminate_vulkan(void);
void     render_device_on_resize(uint32_t width, uint32_t height);

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
