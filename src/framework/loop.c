#include <stdio.h>
#include "framework.h"

static const struct game*      g_ref_game;
static const struct key_map*   g_ref_keymap;
static const struct os_window* g_ref_window;

uint32_t loop_init(const struct loop_config* config)
{
    uint32_t err = 0;
    g_ref_game = config->game;
    g_ref_keymap = config->game->config->keymap;
    g_ref_window = config->window;

    err = render_device_init_vulkan(g_ref_window);
    if (err) {
        printf("[err] vulkan initialization failed!\n");
    };

    err = g_ref_game->init();
    if (err) {
        printf("[err] game initialization failed!\n");
    };
    return err;
}

void loop_update(void)
{
    g_ref_game->update();
    render_device_begin_drawing_vulkan();

    render_device_end_drawing_vulkan();
}

void loop_terminate(void)
{
    g_ref_game->terminate();
    render_device_terminate_vulkan();
}

void loop_on_event(struct os_event* ev)
{
    if (!g_ref_keymap) return;
    switch (ev->kind) {
        case OS_EVENT_KEY_PRESS:
            for (uint32_t i = 0; i < g_ref_keymap->bind_count; i++) {
                uint32_t code = g_ref_keymap->binds[i].key_code;
                if (code == ev->key.code || code == KEY_ANY) {
                    g_ref_keymap->binds[i].on_event(ev->key.code);
                }
            }
            break;
        case OS_EVENT_KEY_RELEASE:
        case OS_EVENT_POINTER_MOVE:
        case OS_EVENT_POINTER_SCROLL:
        case OS_EVENT_POINTER_BUTTON:
        case OS_EVENT_WINDOW_SIZE:
            render_device_on_resize(ev->window.width, ev->window.height);
            break;
        case OS_EVENT_WINDOW_CLOSE:
        case OS_EVENT_KIND_COUNT:
            break;
    }
}
