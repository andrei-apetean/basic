#ifndef BASIC_H
#define BASIC_H

#include <math.h>
#include <stdint.h>

#define count_of(x) sizeof((x))/sizeof((x)[0])
#define clamp(x, min, max) ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))

/* ------------------------------------------------------------------ */
/* math                                                               */
/* ------------------------------------------------------------------ */

struct _float3 { float x, y, z; };
typedef struct _float3 float3;

struct _mat4 { float m[16]; }; /* column-major: m[col*4 + row] */
typedef struct _mat4 mat4;

static inline float3 vec3_sub(float3 a, float3 b)
{
    return (float3){ a.x-b.x, a.y-b.y, a.z-b.z };
}

static inline float vec3_dot(float3 a, float3 b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

static inline float3 vec3_cross(float3 a, float3 b)
{
    return (float3){
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x,
    };
}

static inline float3 vec3_normalize(float3 v)
{
    float len = sqrtf(vec3_dot(v, v));
    return (float3){ v.x/len, v.y/len, v.z/len };
}

static inline mat4 mat4_identity(void)
{
    mat4 m = {0};
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    return m;
}

static inline mat4 mat4_mul(mat4 a, mat4 b)
{
    mat4 r = {0};
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++)
            for (int k = 0; k < 4; k++)
                r.m[col*4+row] += a.m[k*4+row] * b.m[col*4+k];
    return r;
}

/* Vulkan perspective: depth [0..1], Y flipped via negative [1][1] */
static inline mat4 mat4_perspective(float fov_rad, float aspect,
        float near, float far)
{
    float f = 1.0f / tanf(fov_rad * 0.5f);
    mat4 m = {0};
    m.m[0]  =  f / aspect;
    m.m[5]  = -f;                            /* flip Y for Vulkan NDC */
    m.m[10] =  far / (near - far);
    m.m[11] = -1.0f;
    m.m[14] =  (near * far) / (near - far);
    return m;
}

static inline mat4 mat4_look_at(float3 eye, float3 center, float3 up)
{
    float3 f = vec3_normalize(vec3_sub(center, eye));
    float3 r = vec3_normalize(vec3_cross(f, up));
    float3 u = vec3_cross(r, f);

    mat4 m = {0};
    m.m[0]  =  r.x;  m.m[4]  =  r.y;  m.m[8]  =  r.z;
    m.m[1]  =  u.x;  m.m[5]  =  u.y;  m.m[9]  =  u.z;
    m.m[2]  = -f.x;  m.m[6]  = -f.y;  m.m[10] = -f.z;
    m.m[12] = -vec3_dot(r, eye);
    m.m[13] = -vec3_dot(u, eye);
    m.m[14] =  vec3_dot(f, eye);
    m.m[15] =  1.0f;
    return m;
}

static inline mat4 mat4_rotate_y(float angle_rad)
{
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);
    mat4 m = mat4_identity();
    m.m[0]  =  c;
    m.m[2]  = -s;
    m.m[8]  =  s;
    m.m[10] =  c;
    return m;
}

/* ------------------------------------------------------------------ */
/* input                                                              */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* windowing                                                          */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* rendering                                                          */
/* ------------------------------------------------------------------ */

#define RENDER_TARGET_SWAPCHAIN 0xFFFFFFFFu
#define INVALID_BUFFER          0xFFFFFFFFu
#define INVALID_TEXTURE         0xFFFFFFFFu

#define MAX_VERTEX_BINDINGS   8
#define MAX_VERTEX_ATTRIBUTES 8
#define MAX_TEXTURES          16

enum sampler_kind {
    SAMPLER_NEAREST = 0,
    SAMPLER_LINEAR,
    SAMPLER_COUNT,
};

enum buffer_usage {
    BUFFER_USAGE_VERTEX  = 0,
    BUFFER_USAGE_INDEX,
    BUFFER_USAGE_STORAGE,
};

enum vertex_format {
    VERTEX_FORMAT_FLOAT  = 0,
    VERTEX_FORMAT_FLOAT2,
    VERTEX_FORMAT_FLOAT3,
    VERTEX_FORMAT_FLOAT4,
    VERTEX_FORMAT_UINT
};

enum vertex_input_rate {
    VERTEX_INPUT_RATE_VERTEX   = 0,
    VERTEX_INPUT_RATE_INSTANCE,
};

enum topology {
    TOPOLOGY_TRIANGLE_LIST  = 0,
    TOPOLOGY_TRIANGLE_STRIP,
};

enum blend_mode {
    BLEND_MODE_NONE = 0,
    BLEND_MODE_ALPHA,
    BLEND_MODE_ADDITIVE,
};

enum cull_mode {
    CULL_MODE_NONE = 0,
    CULL_MODE_FRONT,
    CULL_MODE_BACK,
};

enum render_pipelines {
    RENDER_PIPELINE_BUILTIN_UI = 0,
    RENDER_PIPELINE_BUILTIN_STATIC_MESH,
    RENDER_PIPELINE_BUILTIN_SKELETAL_MESH,
    RENDER_PIPELINE_CUSTOM0,
    RENDER_PIPELINE_CUSTOM1,
    RENDER_PIPELINE_CUSTOM2,
    RENDER_PIPELINE_CUSTOM3,
    RENDER_PIPELINE_CUSTOM4,
    RENDER_PIPELINES_COUNT,
};

struct vertex_attribute {
    uint32_t           binding;
    uint32_t           location;
    uint32_t           offset;
    enum vertex_format format;
};

struct vertex_binding {
    uint32_t               binding;
    uint32_t               stride;
    enum vertex_input_rate input_rate;
};

enum render_target_format {
    RENDER_TARGET_FORMAT_RGBA8     = 0,
    RENDER_TARGET_FORMAT_RGBA16F   = 1,
    RENDER_TARGET_FORMAT_SWAPCHAIN = 2,
};

struct render_pipeline_config {
    const uint32_t*          shader_vertex;
    const uint32_t*          shader_fragment;
    uint32_t                 shader_vertex_size;
    uint32_t                 shader_fragment_size;

    struct vertex_binding    bindings[MAX_VERTEX_BINDINGS];
    struct vertex_attribute  attributes[MAX_VERTEX_ATTRIBUTES];
    uint32_t                 binding_count;
    uint32_t                 attribute_count;

    uint32_t                  push_constant_size;
    enum render_target_format target_format;  /* must match the render target */
    enum blend_mode           blend_mode;
    enum cull_mode            cull_mode;
    enum topology             topology;
    uint32_t                  depth_test;
    uint32_t                  depth_write;
    uint32_t                  clear;           /* 0 = load, 1 = clear    */
    float                     clear_color[4];  /* ignored if clear == 0  */
};

uint32_t render_target_create(uint32_t width, uint32_t height,
        enum render_target_format format);
void     render_target_destroy(uint32_t id);
void     render_pipeline_register(enum render_pipelines id,
        const struct render_pipeline_config *cfg);
uint32_t render_begin_frame(void);
void     render_end_frame(void);
void     render_composite(uint32_t source_id);
void     render_begin_pipeline(enum render_pipelines id, uint32_t target_id);
void     render_end_pipeline(void);
void     render_draw(uint32_t buffer_id, const void *push, uint32_t push_size,
                 uint32_t vertex_count, uint32_t instance_count);

uint32_t render_buffer_create(const void *data, uint32_t sz, enum buffer_usage usage);
void     render_buffer_update(uint32_t id, const void *data, uint32_t sz);
void     render_buffer_destroy(uint32_t buffer);

uint32_t render_texture_upload(const uint8_t *rgba, uint32_t width, uint32_t height);
void     render_texture_destroy(uint32_t id);

void     render_target_prepare_sample(uint32_t id);

/* ------------------------------------------------------------------ */
/* app interface                                                      */
/* ------------------------------------------------------------------ */

typedef uint32_t (*const app_fn_init)(void);
typedef void     (*const app_fn_update)(float);
typedef void     (*const app_fn_terminate)(void);
typedef void     (*const app_fn_event)(struct app_event*);

struct app_desc {
    struct window*         window;
    const app_fn_init      fn_init;
    const app_fn_update    fn_update;
    const app_fn_terminate fn_terminate;
    const app_fn_event     fn_event;
    uint32_t               target_fps;
    uint32_t               custom_decorations;
    uint32_t               titlebar_height;
};

extern const struct app_desc* load_app(void);

void quit_app(void);


/*------------------------------------------------------------------------*/
/* ui                                                                     */
/* ---------------------------------------------------------------------- */

#define INVALID_TEXTURE  0xFFFFFFFFu

/* -------------------------------------------------------------------------
 * Sizing macros
 *
 * GROW  = 0.0f  — fill remaining space in parent
 * HUG   = -1.0f — shrink to fit children
 * FIXED(n)      — exact pixel size
 * ---------------------------------------------------------------------- */

#define GROW       0.0f
#define HUG       -1.0f
#define FIXED(n)  ((float)(n))

#define UI_IS_GROW(v)  ((v) == 0.0f)
#define UI_IS_HUG(v)   ((v) <  0.0f)
#define UI_IS_FIXED(v) ((v) >  0.0f)

/* -------------------------------------------------------------------------
 * Color macros — packed uint32_t RGBA
 * ---------------------------------------------------------------------- */

#define RGBA(r,g,b,a) \
    (((uint32_t)(r) << 24) | ((uint32_t)(g) << 16) | \
     ((uint32_t)(b) <<  8) |  (uint32_t)(a))
#define RGB(r,g,b) RGBA(r, g, b, 255)

/* -------------------------------------------------------------------------
 * Data binding macro
 *
 * Usage: BIND(&g_player, health, UI_BIND_INT)
 * ---------------------------------------------------------------------- */

#define BIND(ptr, type, field, k) \
    (struct ui_binding){ (ptr), offsetof(type, field), (k) }

/* -------------------------------------------------------------------------
 * Enums
 * ---------------------------------------------------------------------- */
typedef enum {
    UI_ALIGN_START = 0,  // default, current behavior
    UI_ALIGN_CENTER,
    UI_ALIGN_END,
} ui_align;

typedef enum {
    UI_NODE_CONTAINER,   /* layout only — background from style            */
    UI_NODE_RECT,        /* same as container but no children expected     */
    UI_NODE_IMAGE,       /* textured quad                                  */
    UI_NODE_TEXT,        /* text string, static or bound                   */
    UI_NODE_LIST,        /* virtual children via list_emit callback        */
} ui_node_kind;

typedef enum {
    UI_ROW,
    UI_COLUMN,
} ui_direction;

typedef enum {
    UI_BIND_NONE,
    UI_BIND_INT,
    UI_BIND_FLOAT,
    UI_BIND_STRING,
} ui_bind_kind;

typedef enum {
    UI_EVENT_MOUSE_MOVE,
    UI_EVENT_MOUSE_DOWN,
    UI_EVENT_MOUSE_UP,
    UI_EVENT_SCROLL,
    UI_EVENT_KEY_DOWN,
} ui_event_kind;

/* -------------------------------------------------------------------------
 * Structs
 * ---------------------------------------------------------------------- */

struct ui_binding {
    void        *base;     /* pointer to owning struct                     */
    uint32_t     offset;   /* offsetof field within that struct            */
    ui_bind_kind kind;
};

/*
 * ui_style — visual properties, indexed by user-defined enum
 * Slot 0 is the builtin default: white text, transparent background
 */
struct ui_style {
    uint32_t background;   /* packed RGBA                                  */
    uint32_t border;       /* packed RGBA                                  */
    uint32_t text;         /* packed RGBA                                  */
    float    border_width;
    float    text_scale;
    uint32_t tex_index;    /* background texture, INVALID_TEXTURE = none   */
};

/*
 * ui_instance — one GPU draw instance
 * Must match the vertex layout registered with the UI pipeline
 */
struct ui_instance {
    float    x, y, w, h;
    float    r, g, b, a;
    float    u0, v0, u1, v1;
    uint32_t tex_index;
    uint32_t flags;        /* UI_FLAG_GLYPH etc                            */
    uint32_t _pad[2];
};

#define UI_FLAG_GLYPH 1u   /* alpha-only sample, rgb from tint             */

/*
 * ui_node — one element in the flat tree array
 *
 * Tree structure is encoded as a linked list:
 *   first_child   — index of first child, UI_NODE_NONE if leaf
 *   next_sibling  — index of next sibling, UI_NODE_NONE if last
 *
 * Declare children array above parent, pass pointer:
 *   static uint16_t g_titlebar_children[] = { NODE_TEXT, NODE_BTN, UI_NODE_NONE };
 *   [NODE_TITLEBAR] = { .first_child = NODE_TEXT, ... }
 *
 * Or wire up in app_init via ui_set_children().
 */
struct ui_node {
    /* --- layout -------------------------------------------------------- */
    ui_node_kind  kind;
    ui_direction  direction;
    float         w, h;        /* GROW / HUG / FIXED(n)                   */
    float         pad;         /* inner padding on all sides               */
    float         gap;         /* spacing between children                 */

    /* --- appearance ---------------------------------------------------- */
    uint16_t      style;       /* index into g_ui_styles[]                 */
    uint16_t      style_hover; /* swapped in when hovered, 0 = no change   */
    ui_align      align_main;   // along direction axis (x for ROW, y for COLUMN)
    ui_align      align_cross;  // perpendicular axis

    /* --- identity ------------------------------------------------------ */
    uint16_t      id;          /* UI_ID_NONE = not interactive             */

    /* --- tree ---------------------------------------------------------- */
    uint16_t      first_child;
    uint16_t      next_sibling;

    /* --- content ------------------------------------------------------- */
    const char   *text;        /* static label for UI_NODE_TEXT            */
    uint32_t      tex_index;   /* for UI_NODE_IMAGE                        */
    struct ui_binding binding; /* live data binding, overrides text        */

    /* --- list (UI_NODE_LIST only) -------------------------------------- */
    struct ui_node  *row_template;       // pointer to template array
    uint32_t         row_template_count; // number of nodes in template
    void            *list_data;          // pointer to data array
    uint32_t         list_data_stride;   // sizeof each element
    uint32_t        *list_count;
    float            list_item_h;

    /* --- scroll -------------------------------------------------------- */
    uint8_t       scrollable;
    float         scroll_y;

    /* --- solved (written by ui_solve, read by ui_emit) ----------------- */
    float         solved_x, solved_y;
    float         solved_w, solved_h;
    float         content_h;  /* total height of children, for scroll     */
};

/*
 * ui_event — forwarded from app_event
 */
struct ui_event {
    ui_event_kind kind;
    union {
        struct { int32_t mouse_x, mouse_y; };   /* MOUSE_MOVE / DOWN / UP */
        struct { float   scroll_dy; };           /* SCROLL                 */
        struct { uint32_t key; };                /* KEY_DOWN               */
    };
};

/* -------------------------------------------------------------------------
 * User-provided globals (define these in your application)
 * ---------------------------------------------------------------------- */

extern struct ui_node  *g_ui_nodes;    /* your flat node array             */
extern struct ui_style *g_ui_styles;   /* your style array                 */
extern uint16_t         g_ui_root;     /* index of root node               */

/* -------------------------------------------------------------------------
 * API
 * ---------------------------------------------------------------------- */

/*
 * ui_solve — resolve all sizes and positions for the tree
 * Call once per frame before ui_emit
 */
void     ui_solve(uint16_t root, float x, float y, float w, float h);

/*
 * ui_emit — walk tree and write ui_instance[] for GPU submission
 * Returns number of instances written
 */
uint32_t ui_emit(uint16_t root, struct ui_instance *dst);

/*
 * ui_event — forward input events from app_event
 */
void     ui_event(struct ui_event *e);

/*
 * ui_on_click — register a click handler for a node id
 * id must be < 256
 */
void     ui_on_click(uint16_t id, void (*fn)(void *userdata), void *userdata);

static inline void rgba_unpack(uint32_t color,
        float *r, float *g, float *b, float *a)
{
    *r = ((color >> 24) & 0xFF) / 255.0f;
    *g = ((color >> 16) & 0xFF) / 255.0f;
    *b = ((color >>  8) & 0xFF) / 255.0f;
    *a = ((color >>  0) & 0xFF) / 255.0f;
}

/*
 * Query current hover/focus state
 */
uint16_t ui_hovered(void);
uint16_t ui_focused(void);
#endif /* BASIC_H */
