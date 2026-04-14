#ifndef BASIC_H
#define BASIC_H

#include <stdint.h>

#define count_of(x) sizeof((x))/sizeof((x)[0])

enum key_state {
    KEY_STATE_UP = 0,
    KEY_STATE_DOWN,
    KEY_STATE_COUNT,
};

enum key_code {
    KEY_UNKNOWN = 0,
    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_0,
    KEY_ENTER,
    KEY_ESCAPE,
    KEY_BACKSPACE,
    KEY_TAB,
    KEY_SPACE,
    KEY_MINUS,
    KEY_EQUAL,
    KEY_LBRACE,
    KEY_RBRACE,
    KEY_BACKSLASH,
    KEY_SEMICOLON,
    KEY_APOSTROPHE,
    KEY_GRAVEACCENT,
    KEY_COMMA,
    KEY_PERIOD,
    KEY_SLASH,
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_RIGHT,
    KEY_LEFT,
    KEY_DOWN,
    KEY_UP,
    KEY_CAPSLOCK,
    KEY_LCONTROL,
    KEY_LALT,
    KEY_LSHIFT,
    KEY_LMETA,
    KEY_RSHIFT,
    KEY_ANY,
    KEY_CODE_COUNT,
};

enum button_code {
    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_MIDDLE,
    BUTTON_CODE_COUNT,
};

struct key_bind {
    uint32_t key_code;
    uint32_t key_state;
    void (*on_event)(uint32_t key_code);
};

struct key_map {
    struct key_bind* binds;
    uint32_t bind_count;
};

struct game_config {
    struct key_map* keymap;
};

struct game {
    uint32_t (*const init)(void);
    void     (*const update)(void);
    void     (*const terminate)(void);
    uint32_t exit_requested;

    const struct game_config* config;
};


extern const struct game* load_game(void);


#endif /* BASIC_H */
