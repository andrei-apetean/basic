#include <basic/basic.h>
#include <stdio.h>

static void on_escape_pressed(uint32_t);
static void on_any_key_pressed(uint32_t);

static struct key_bind g_keybinds[] = {
    {
        .key_code = KEY_ESCAPE,
        .key_state = KEY_STATE_DOWN,
        .on_event = on_escape_pressed,
    },
    {
        .key_code = KEY_ANY,
        .key_state = KEY_STATE_DOWN,
        .on_event = on_any_key_pressed,
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

void on_escape_pressed(uint32_t code)
{
    (void)code;
    printf("[inf] escape pressed.. goodbye\n");
    g_game_instance.exit_requested = 1;
}

static void on_any_key_pressed(uint32_t code)
{

    switch (code) {
        case KEY_UNKNOWN: printf("KEY_UNKNOWN\n"); break;
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
        case KEY_CODE_COUNT: printf("KEY_CODE_COUNT"); break;
    }

}

#include "../framework/unity.c"
