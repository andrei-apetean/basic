#include <basic/basic.h>
#include <stdio.h>

static void on_escape_pressed(void);

static struct key_bind g_keybinds[] = {
    {
        .key_code = KEY_ESCAPE,
        .key_state = KEY_STATE_DOWN,
        .on_event = on_escape_pressed,
    },
};

static struct key_map g_game_keymap = {
    .binds = g_keybinds,
    .bind_count = count_of(g_keybinds),
};

static struct game_config g_config = {
    .keymap = &g_game_keymap,
};

uint32_t game_init(void);
void     game_update(void);
void     game_terminate(void);

static struct game g_game_instance = {
    .init = game_init,
    .update = game_update,
    .terminate = game_terminate,
    .config = &g_config
};

const struct game* load_game(void)
{
    return &g_game_instance;
}


uint32_t game_init(void)
{
    printf("game initialized!\n");
    return 0;
}

void game_update(void)
{

}

void game_terminate(void)
{

}

void on_escape_pressed(void) {
    printf("[inf] escape pressed.. goodbye\n");
    g_game_instance.exit_requested = 1;
}

#include "../framework/unity.c"
