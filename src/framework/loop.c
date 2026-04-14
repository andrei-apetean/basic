#include <stdio.h>
#include "framework.h"

struct pointer {
    uint32_t x, y;
    uint32_t dx, dy;
    uint32_t buttons[BUTTON_CODE_COUNT];
};

const struct game* g_game;
const struct key_map* g_keymap;
uint32_t g_keyboard[KEY_CODE_COUNT];


uint32_t loop_init(const struct loop_config* config)
{
    uint32_t err = 0;
    g_game = config->game;
    g_keymap = config->game->config->keymap;
    err = g_game->init();
    if (err) {
        printf("[err] game initialization failed!\n");
    };
    return err;
}

void loop_update(void)
{
    g_game->update();
}

void loop_terminate(void)
{
    g_game->terminate();
}

void loop_on_event(struct os_event* ev)
{
    if (!g_keymap) return;
    switch (ev->kind) {
        case OS_EVENT_KEY_PRESS:
            for (uint32_t i = 0; i < g_keymap->bind_count; i++) {
                uint32_t code = g_keymap->binds[i].key_code;
                if (code == ev->key.code || code == KEY_ANY) {
                    g_keymap->binds[i].on_event(ev->key.code);
                }
            }
            break;
        case OS_EVENT_KEY_RELEASE:
        case OS_EVENT_POINTER_MOVE:
        case OS_EVENT_POINTER_SCROLL:
        case OS_EVENT_POINTER_BUTTON:
        case OS_EVENT_WINDOW_SIZE:
        case OS_EVENT_WINDOW_CLOSE:
        case OS_EVENT_KIND_COUNT:
            break;
    }
}
