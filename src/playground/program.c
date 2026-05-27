#include <basic/basic.h>
#include <stddef.h>
#include <stdio.h>
#include "../src/basic/cube_vert.h"
#include "../src/basic/cube_frag.h"
#include "../src/basic/ui_vert.h"
#include "../src/basic/ui_frag.h"
#include "../src/basic/cozette.h"

#define TITLEBAR_HEIGHT  32
#define BUTTON_WIDTH     46
#define COZETTE_ASCENT   10

#define UI_FLAG_GLYPH 1u

typedef enum {
    NODE_INVALID = 0,
    NODE_ROOT,
    NODE_TITLEBAR,
    NODE_TITLE_TEXT,
    NODE_SPACER,
    NODE_BTN_MIN,
    NODE_BTN_MIN_LABEL,
    NODE_BTN_MAX,
    NODE_BTN_MAX_LABEL,
    NODE_BTN_CLOSE,
    NODE_BTN_CLOSE_LABEL,
    NODE_CONTENT,
    NODE_SIDEBAR,
    NODE_ENTITY_LIST,
    NODE_GAME_VIEW,
    NODE_COUNT,
} node_id;

typedef enum {
    NODE_ENTITY_ROW_BG = 0,
    NODE_ENTITY_ROW_LABEL,
    NODE_ENTITY_ROW_COUNT,
} entity_row_node;

typedef enum {
    /* interaction ids — used for hit testing and dispatch */
    UI_ID_INVALID = 0,
    UI_ID_BTN_MIN,
    UI_ID_BTN_MAX,
    UI_ID_BTN_CLOSE,
    UI_ID_COUNT,
} ui_id;

typedef enum {
    /* style ids — index into g_ui_styles[] */
    STYLE_DEFAULT = 0,   /* builtin, leave as-is or override               */
    STYLE_TITLEBAR,
    STYLE_SIDEBAR,
    STYLE_BTN,
    STYLE_BTN_HOVER,
    STYLE_BTN_CLOSE_HOVER,
    STYLE_TEXT,
    STYLE_COUNT,
} style_id;

/* -------------------------------------------------------------------------
 * 2. Define styles
 * ---------------------------------------------------------------------- */

struct ui_style g_styles[STYLE_COUNT] = {
    [STYLE_DEFAULT]        = { .background = RGBA(0,0,0,0),
                               .text       = RGB(255,255,255),
                               .text_scale = 1.0f },

    [STYLE_TITLEBAR]       = { .background = RGB(38, 38, 38),
                               .text       = RGB(255,255,255),
                               .text_scale = 1.7f },

    [STYLE_SIDEBAR]        = { .background = RGB(40, 40, 40),
                               .text       = RGB(255,255,255),
                               .text_scale = 1.7f },

    [STYLE_BTN]            = { .background = RGB(38, 38, 38),
                               .text       = RGB(255,255,255),
                               .text_scale = 1.7f },
    [STYLE_BTN_HOVER]      = { .background = RGB(60, 60, 60),
                               .text       = RGB(255, 255, 255),
                               .text_scale = 2.0f },

    [STYLE_BTN_CLOSE_HOVER]= { .background = RGB(192, 38, 38),
                               .text       = RGB(255,255,255),
                               .text_scale = 1.7f },

    [STYLE_TEXT]           = { .background = RGBA(0,0,0,0),
                               .text       = RGB(255,255,255),
                               .text_scale = 1.4f },
};

struct ui_style* g_ui_styles = g_styles;

struct entity {
    const char *name;
    float       health;
};

static struct entity g_entities[] = {
    { "Goblin",      80.0f },
    { "Orc",        120.0f },
    { "Troll",      200.0f },
    { "Dragon",     999.0f },
    { "Skeleton",    40.0f },
    { "Zombie",      60.0f },
    { "Werewolf",   150.0f },
    { "Vampire",    180.0f },
    { "Test",       180.0f },
};

static uint32_t g_entity_count = count_of(g_entities);

static struct ui_node g_entity_row_template[NODE_ENTITY_ROW_COUNT] = {
    [NODE_ENTITY_ROW_BG] = {
        .kind        = UI_NODE_RECT,
        .direction   = UI_ROW,
        .w = GROW, .h = FIXED(20),
        .align_cross = UI_ALIGN_CENTER,
        .pad         = 4,
        .style       = STYLE_TEXT,
        .first_child = NODE_ENTITY_ROW_LABEL,
    },
    [NODE_ENTITY_ROW_LABEL] = {
        .kind  = UI_NODE_TEXT,
        .w = HUG, .h = HUG,
        .style = STYLE_TEXT,
        .binding = {
            .base = g_entities,
            .offset = offsetof(struct entity, name),
            .kind = UI_BIND_STRING,
        },
    }
};

/* -------------------------------------------------------------------------
 * 3. Declare the tree
 * ---------------------------------------------------------------------- */

struct ui_node g_nodes[NODE_COUNT] = {
    [NODE_INVALID] = { 0 },
    [NODE_ROOT] = {
        .kind         = UI_NODE_CONTAINER,
        .direction    = UI_COLUMN,
        .w = GROW, .h = GROW,
        .pad          = 4,
        .first_child  = NODE_TITLEBAR,
    },

    [NODE_TITLEBAR] = {
        .kind         = UI_NODE_CONTAINER,
        .direction    = UI_ROW,
        .w = GROW, .h = FIXED(32),
        .style        = STYLE_TITLEBAR,
        .pad          = 8,
        .first_child  = NODE_TITLE_TEXT,
        .next_sibling = NODE_CONTENT,
    },

    [NODE_TITLE_TEXT] = {
        .kind         = UI_NODE_TEXT,
        .w = HUG,  .h = HUG,
        .style        = STYLE_TEXT,
        .text         = "basic-v",
        .next_sibling = NODE_SPACER,
    },

    [NODE_SPACER] = {
        .kind         = UI_NODE_RECT,
        .w = GROW, .h = GROW,
        /* transparent, pushes buttons to the right */
        .next_sibling = NODE_BTN_MIN,
    },
    [NODE_BTN_MIN] = {
        .kind         = UI_NODE_RECT,
        .w = FIXED(46), .h = FIXED(32),
        .style        = STYLE_BTN,
        .style_hover  = STYLE_BTN_HOVER,
        .id           = UI_ID_BTN_MIN,
        .first_child  = NODE_BTN_MIN_LABEL,
        .next_sibling = NODE_BTN_MAX,
        .align_main  = UI_ALIGN_CENTER,
        .align_cross = UI_ALIGN_CENTER,
    },
    [NODE_BTN_MIN_LABEL] = {
        .kind         = UI_NODE_TEXT,
        .w = HUG, .h = HUG,
        .style        = STYLE_TEXT,
        .text         = "-",
    },

    [NODE_BTN_MAX] = {
        .kind         = UI_NODE_RECT,
        .w = FIXED(46), .h = FIXED(32),
        .style        = STYLE_BTN,
        .style_hover  = STYLE_BTN_HOVER,
        .id           = UI_ID_BTN_MAX,
        .first_child  = NODE_BTN_MAX_LABEL,
        .next_sibling = NODE_BTN_CLOSE,
        .align_main  = UI_ALIGN_CENTER,
        .align_cross = UI_ALIGN_CENTER,
    },
    [NODE_BTN_MAX_LABEL] = {
        .kind         = UI_NODE_TEXT,
        .w = HUG, .h = HUG,
        .style        = STYLE_TEXT,
        .text         = "+",
    },

    [NODE_BTN_CLOSE] = {
        .kind         = UI_NODE_RECT,
        .w = FIXED(46), .h = FIXED(32),
        .style        = STYLE_BTN,
        .style_hover  = STYLE_BTN_CLOSE_HOVER,
        .id           = UI_ID_BTN_CLOSE,
        .first_child  = NODE_BTN_CLOSE_LABEL,
        .align_main  = UI_ALIGN_CENTER,
        .align_cross = UI_ALIGN_CENTER,
    },
    [NODE_BTN_CLOSE_LABEL] = {
        .kind         = UI_NODE_TEXT,
        .w = HUG, .h = HUG,
        .style        = STYLE_TEXT,
        .text         = "x",
    },

    [NODE_CONTENT] = {
        .kind         = UI_NODE_CONTAINER,
        .direction    = UI_ROW,
        .w = GROW, .h = GROW,
        .first_child  = NODE_SIDEBAR,
    },

    [NODE_ENTITY_LIST] = {
        .kind               = UI_NODE_LIST,
        .w = GROW, .h = GROW,
        .row_template       = g_entity_row_template,
        .row_template_count = NODE_ENTITY_ROW_COUNT,
        .list_data          = g_entities,
        .list_data_stride   = sizeof(struct entity),
        .list_count         = &g_entity_count,
        .list_item_h        = 20.0f,
        .gap                = 2.0f,
        .scrollable         = 1,
    },
    [NODE_SIDEBAR] = {
        .kind         = UI_NODE_CONTAINER,
        .direction    = UI_COLUMN,
        .w = FIXED(200), .h = GROW,
        .style        = STYLE_SIDEBAR,
        .pad          = 8,
        .gap          = 4,
        .first_child  = NODE_ENTITY_LIST,
        .next_sibling = NODE_GAME_VIEW,
    },

    [NODE_GAME_VIEW] = {
        .kind         = UI_NODE_IMAGE,
        .w = GROW, .h = GROW,
        /* .tex_index set in app_init after render_target_create */
    },
};

/* provide the extern globals ui.c needs */
struct ui_node  *g_ui_nodes = g_nodes;   /* rename or use directly */
uint16_t         g_ui_root  = NODE_ROOT;


struct cube_vertex {
    float x, y, z;
    float nx, ny, nz;
};

static const struct cube_vertex cube_verts[] = {
    /* front  (+z) */
    {-0.5f,-0.5f, 0.5f, 0,0,1}, { 0.5f,-0.5f, 0.5f, 0,0,1}, { 0.5f, 0.5f, 0.5f, 0,0,1},
    { 0.5f, 0.5f, 0.5f, 0,0,1}, {-0.5f, 0.5f, 0.5f, 0,0,1}, {-0.5f,-0.5f, 0.5f, 0,0,1},
    /* back   (-z) */
    { 0.5f,-0.5f,-0.5f, 0,0,-1},{-0.5f,-0.5f,-0.5f, 0,0,-1},{-0.5f, 0.5f,-0.5f, 0,0,-1},
    {-0.5f, 0.5f,-0.5f, 0,0,-1},{ 0.5f, 0.5f,-0.5f, 0,0,-1},{ 0.5f,-0.5f,-0.5f, 0,0,-1},
    /* left   (-x) */
    {-0.5f,-0.5f,-0.5f,-1,0,0},{-0.5f,-0.5f, 0.5f,-1,0,0},{-0.5f, 0.5f, 0.5f,-1,0,0},
    {-0.5f, 0.5f, 0.5f,-1,0,0},{-0.5f, 0.5f,-0.5f,-1,0,0},{-0.5f,-0.5f,-0.5f,-1,0,0},
    /* right  (+x) */
    { 0.5f,-0.5f, 0.5f, 1,0,0},{ 0.5f,-0.5f,-0.5f, 1,0,0},{ 0.5f, 0.5f,-0.5f, 1,0,0},
    { 0.5f, 0.5f,-0.5f, 1,0,0},{ 0.5f, 0.5f, 0.5f, 1,0,0},{ 0.5f,-0.5f, 0.5f, 1,0,0},
    /* top    (+y) */
    {-0.5f, 0.5f, 0.5f, 0,1,0},{ 0.5f, 0.5f, 0.5f, 0,1,0},{ 0.5f, 0.5f,-0.5f, 0,1,0},
    { 0.5f, 0.5f,-0.5f, 0,1,0},{-0.5f, 0.5f,-0.5f, 0,1,0},{-0.5f, 0.5f, 0.5f, 0,1,0},
    /* bottom (-y) */
    {-0.5f,-0.5f,-0.5f, 0,-1,0},{ 0.5f,-0.5f,-0.5f, 0,-1,0},{ 0.5f,-0.5f, 0.5f, 0,-1,0},
    { 0.5f,-0.5f, 0.5f, 0,-1,0},{-0.5f,-0.5f, 0.5f, 0,-1,0},{-0.5f,-0.5f,-0.5f, 0,-1,0},
};

static uint32_t g_cube_buffer;
static uint32_t g_ui_buffer;
static float    g_time;
static struct window g_window;
static uint32_t g_game_view;
uint32_t g_atlas_texture;

static int32_t g_mouse_x;
static int32_t g_mouse_y;


/* textured glyph helper */
static inline struct ui_instance ui_glyph(float x, float y,
        float r, float g, float b, float a,
        float u0, float v0, float u1, float v1,
        uint32_t tex_index, float scale)
{
    return (struct ui_instance){
        .x = x, .y = y,
        .w = COZETTE_GLYPH_W * scale, .h = COZETTE_GLYPH_H * scale,
        .r = r, .g = g, .b = b, .a = a,
        .u0 = u0, .v0 = v0, .u1 = u1, .v1 = v1,
        .flags = UI_FLAG_GLYPH,
        .tex_index = tex_index,
    };
}


uint32_t draw_text(struct ui_instance *dst, const char *text, float scale,
        float x, float y, float r, float g, float b, float a,
        uint32_t tex_index)
{

    uint32_t n = 0;
  while (*text) {
        float u0, v0, u1, v1;
        cozette_glyph_uv((int)*text, &u0, &v0, &u1, &v1);
        dst[n++] = ui_glyph(x, y, r, g, b, a, u0, v0, u1, v1, tex_index, scale);
        x += COZETTE_GLYPH_W * scale;
        text++;
    }
    return n;
}


uint32_t app_init(void);
void     app_update(float dt);
void     app_terminate(void);
void     app_event(struct app_event*);

static struct app_desc g_my_app = {
    .window = &g_window,
    .fn_init = app_init,
    .fn_update = app_update,
    .fn_event = app_event,
    .fn_terminate = app_terminate,
    .custom_decorations = 1,
    .titlebar_height = 32,
};

const struct app_desc* load_app(void)
{
    return &g_my_app;
}

uint32_t app_init(void)
{
    g_game_view = render_target_create(1980, 1080, RENDER_TARGET_FORMAT_RGBA8);

    g_atlas_texture = render_texture_upload(cozette_atlas,
            COZETTE_ATLAS_W, COZETTE_ATLAS_H);

    /* cube pipeline — renders 3d world into game view */
    render_pipeline_register(RENDER_PIPELINE_BUILTIN_STATIC_MESH,
        &(struct render_pipeline_config){
            .shader_vertex        = (const uint32_t*)cube_vert_spv,
            .shader_vertex_size   = sizeof(cube_vert_spv),
            .shader_fragment      = (const uint32_t*)cube_frag_spv,
            .shader_fragment_size = sizeof(cube_frag_spv),
            .bindings = {
                { .binding = 0, .stride = sizeof(struct cube_vertex),
                  .input_rate = VERTEX_INPUT_RATE_VERTEX },
            },
            .binding_count  = 1,
            .attributes = {
                { .binding = 0, .location = 0,
                  .offset = offsetof(struct cube_vertex, x),
                  .format = VERTEX_FORMAT_FLOAT3 },
                { .binding = 0, .location = 1,
                  .offset = offsetof(struct cube_vertex, nx),
                  .format = VERTEX_FORMAT_FLOAT3 },
            },
            .attribute_count      = 2,
            .push_constant_size   = sizeof(mat4),
            .target_format        = RENDER_TARGET_FORMAT_RGBA8,
            .blend_mode           = BLEND_MODE_NONE,
            .cull_mode            = CULL_MODE_BACK,
            .topology             = TOPOLOGY_TRIANGLE_LIST,
            .depth_test           = 1,
            .depth_write          = 1,
            .clear                = 1,
            .clear_color          = { 0.15f, 0.15f, 0.15f, 1.0f },
        });

    /* ui pipeline — renders directly to swapchain on top of composite */
    render_pipeline_register(RENDER_PIPELINE_BUILTIN_UI,
        &(struct render_pipeline_config){
            .shader_vertex        = (const uint32_t*)ui_vert_spv,
            .shader_vertex_size   = sizeof(ui_vert_spv),
            .shader_fragment      = (const uint32_t*)ui_frag_spv,
            .shader_fragment_size = sizeof(ui_frag_spv),
            .bindings = {
                { .binding = 0, .stride = sizeof(struct ui_instance),
                  .input_rate = VERTEX_INPUT_RATE_INSTANCE },
            },
            .binding_count  = 1,
            .attributes = {
                { .binding=0, .location=0, .offset=offsetof(struct ui_instance, x),
                    .format=VERTEX_FORMAT_FLOAT4 },   // x y w h
                { .binding=0, .location=1, .offset=offsetof(struct ui_instance, r),
                    .format=VERTEX_FORMAT_FLOAT4 },   // r g b a
                { .binding=0, .location=2, .offset=offsetof(struct ui_instance, u0),
                    .format=VERTEX_FORMAT_FLOAT4 },   // u0 v0 u1 v1
                // tex_index as raw bits — read as uint in shader
                { .binding=0, .location=3,
                    .offset=offsetof(struct ui_instance, tex_index),
                    .format=VERTEX_FORMAT_UINT },
                { .binding=0, .location=4,
                    .offset=offsetof(struct ui_instance, flags),
                    .format=VERTEX_FORMAT_UINT },
            },
            .attribute_count = 5,
            .push_constant_size   = sizeof(float) * 2,
            .target_format        = RENDER_TARGET_FORMAT_SWAPCHAIN,
            .blend_mode           = BLEND_MODE_ALPHA,
            .cull_mode            = CULL_MODE_NONE,
            .topology             = TOPOLOGY_TRIANGLE_STRIP,
            .depth_test           = 0,
            .depth_write          = 0,
            .clear                = 1,
            .clear_color          = { 0.15, 0.15f, 0.15f, 1.0f },
        });

    /* geometry buffer — device local, uploaded once */
    g_cube_buffer = render_buffer_create(cube_verts, sizeof(cube_verts),
            BUFFER_USAGE_VERTEX);

    /* ui buffer — host visible, updated each frame */
    g_ui_buffer = render_buffer_create(NULL,
            sizeof(struct ui_instance) * 1024, BUFFER_USAGE_VERTEX);

    g_ui_nodes[NODE_GAME_VIEW].tex_index = g_game_view;

    printf("game initialized!\n");
    return 0;
}

void app_update(float dt)
{
    g_time += dt * 10.0f;

    /* mvp for rotating cube */
    mat4 proj  = mat4_perspective(70.0f * (3.14159f / 180.0f),
                            ((float)g_window.width-200) / ((float)g_window.height - 32),
                            0.1f, 100.0f);
    mat4 view  = mat4_look_at(
                            (float3){ 0.0f, 1.5f, 3.0f },
                            (float3){ 0.0f, 0.0f, 0.0f },
                            (float3){ 0.0f, 1.0f, 0.0f });
    mat4 model = mat4_rotate_y(g_time);
    mat4 mvp   = mat4_mul(proj, mat4_mul(view, model));

    /* frame */
    uint32_t err = render_begin_frame();
    if (err) return;

    { /* 3d world */
        render_begin_pipeline(RENDER_PIPELINE_BUILTIN_STATIC_MESH, g_game_view);
        render_draw(g_cube_buffer, &mvp, sizeof(mvp), count_of(cube_verts), 1);
        render_end_pipeline();
    }

    render_target_prepare_sample(g_game_view);

    { /* ui */
        struct ui_instance ui[1024];
        uint32_t n = 0;
        ui_solve(NODE_ROOT, 0, 0, g_window.width, g_window.height);
        n = ui_emit(NODE_ROOT, ui);


        render_buffer_update(g_ui_buffer, ui, sizeof(struct ui_instance) * n);

        float screen[2] = { g_window.width, g_window.height };
        render_begin_pipeline(RENDER_PIPELINE_BUILTIN_UI, RENDER_TARGET_SWAPCHAIN);
        render_draw(g_ui_buffer, screen, sizeof(screen), 4, n);
        render_end_pipeline();
    }

    render_end_frame();
}

void app_event(struct app_event* event)
{
    switch (event->kind) {
        case APP_EVENT_KEY_DOWN: {
            if ( event->key.code == KEY_ESCAPE) quit_app();
            break;
        }

        case APP_EVENT_MOUSE_MOVE:
            ui_event(&(struct ui_event){
                .kind    = UI_EVENT_MOUSE_MOVE,
                .mouse_x = event->mouse_move.x,
                .mouse_y = event->mouse_move.y,
            });
            break;
        case APP_EVENT_MOUSE_DOWN:
            ui_event(&(struct ui_event){
                .kind    = UI_EVENT_MOUSE_DOWN,
                .mouse_x = g_mouse_x,
                .mouse_y = g_mouse_y,
            });
            break;
        // case APP_EVENT_MOUSE_SCROLL:
        //     ui_event(&(struct ui_event){
        //         .kind      = UI_EVENT_SCROLL,
        //         .scroll_dy = event->mouse_move.dy,
        //     });
        //     break;
    }
}

void app_terminate(void)
{
    render_buffer_destroy(g_cube_buffer);
    render_buffer_destroy(g_ui_buffer);
    render_target_destroy(g_game_view);
    printf("game terminate!\n");
}

/*
void on_escape_pressed(uint32_t code)
{
    (void)code;
    printf("[inf] escape pressed.. goodbye\n");
    g_game_instance.exit_requested = 1;
}

static void on_any_key_pressed(uint32_t code)
{

    switch (code) {
        case KEY_A: printf("KEY_A\n"); break;
        case KEY_B: printf("KEY_B\n"); break;
        case KEY_C: printf("KEY_C\n"); break;
        case KEY_D: printf("KEY_D\n"); break;
        case KEY_E: printf("KEY_E\n"); break;
        case KEY_F: printf("KEY_F\n"); break;
        case KEY_G: printf("KEY_G\n"); break;
        case KEY_H: printf("KEY_H\n"); break;
        case KEY_I: printf("KEY_I\n"); break;
        case KEY_J: printf("KEY_J\n"); break;
        case KEY_K: printf("KEY_K\n"); break;
        case KEY_L: printf("KEY_L\n"); break;
        case KEY_M: printf("KEY_M\n"); break;
        case KEY_N: printf("KEY_N\n"); break;
        case KEY_O: printf("KEY_O\n"); break;
        case KEY_P: printf("KEY_P\n"); break;
        case KEY_Q: printf("KEY_Q\n"); break;
        case KEY_R: printf("KEY_R\n"); break;
        case KEY_S: printf("KEY_S\n"); break;
        case KEY_T: printf("KEY_T\n"); break;
        case KEY_U: printf("KEY_U\n"); break;
        case KEY_V: printf("KEY_V\n"); break;
        case KEY_W: printf("KEY_W\n"); break;
        case KEY_X: printf("KEY_X\n"); break;
        case KEY_Y: printf("KEY_Y\n"); break;
        case KEY_Z: printf("KEY_Z\n"); break;
        case KEY_1: printf("KEY_1\n"); break;
        case KEY_2: printf("KEY_2\n"); break;
        case KEY_3: printf("KEY_3\n"); break;
        case KEY_4: printf("KEY_4\n"); break;
        case KEY_5: printf("KEY_5\n"); break;
        case KEY_6: printf("KEY_6\n"); break;
        case KEY_7: printf("KEY_7\n"); break;
        case KEY_8: printf("KEY_8\n"); break;
        case KEY_9: printf("KEY_9\n"); break;
        case KEY_0: printf("KEY_0\n"); break;
        case KEY_ENTER: printf("KEY_ENTER\n"); break;
        case KEY_ESCAPE: printf("KEY_ESCAPE\n"); break;
        case KEY_BACKSPACE: printf("KEY_BACKSPACE\n"); break;
        case KEY_TAB: printf("KEY_TAB\n"); break;
        case KEY_SPACE: printf("KEY_SPACE\n"); break;
        case KEY_MINUS: printf("KEY_MINUS\n"); break;
        case KEY_EQUAL: printf("KEY_EQUAL\n"); break;
        case KEY_LBRACE: printf("KEY_LBRACE\n"); break;
        case KEY_RBRACE: printf("KEY_RBRACE\n"); break;
        case KEY_BACKSLASH: printf("KEY_BACKSLASH\n"); break;
        case KEY_SEMICOLON: printf("KEY_SEMICOLON\n"); break;
        case KEY_APOSTROPHE: printf("KEY_APOSTROPHE\n"); break;
        case KEY_GRAVEACCENT: printf("KEY_GRAVEACCENT\n"); break;
        case KEY_COMMA: printf("KEY_COMMA\n"); break;
        case KEY_PERIOD: printf("KEY_PERIOD\n"); break;
        case KEY_SLASH: printf("KEY_SLASH\n"); break;
        case KEY_F1: printf("KEY_F1\n"); break;
        case KEY_F2: printf("KEY_F2\n"); break;
        case KEY_F3: printf("KEY_F3\n"); break;
        case KEY_F4: printf("KEY_F4\n"); break;
        case KEY_F5: printf("KEY_F5\n"); break;
        case KEY_F6: printf("KEY_F6\n"); break;
        case KEY_F7: printf("KEY_F7\n"); break;
        case KEY_F8: printf("KEY_F8\n"); break;
        case KEY_F9: printf("KEY_F9\n"); break;
        case KEY_F10: printf("KEY_F10\n"); break;
        case KEY_F11: printf("KEY_F11\n"); break;
        case KEY_F12: printf("KEY_F12\n"); break;
        case KEY_RIGHT: printf("KEY_RIGHT\n"); break;
        case KEY_LEFT: printf("KEY_LEFT\n"); break;
        case KEY_DOWN: printf("KEY_DOWN\n"); break;
        case KEY_UP: printf("KEY_UP\n"); break;
        case KEY_CAPSLOCK: printf("KEY_CAPSLOCK\n"); break;
        case KEY_LCONTROL: printf("KEY_LCONTROL\n"); break;
        case KEY_LALT: printf("KEY_LALT\n"); break;
        case KEY_LSHIFT: printf("KEY_LSHIFT\n"); break;
        case KEY_LMETA: printf("KEY_LMETA\n"); break;
        case KEY_RSHIFT: printf("KEY_RSHIFT\n"); break;
        case KEY_ANY: printf("KEY_ANY\n"); break;
    }

}
*/
