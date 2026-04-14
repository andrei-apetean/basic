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
    KEY_ESCAPE,
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
    void (*on_event)(void);
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
