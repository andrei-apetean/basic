#ifndef BASIC_FRAMEWORK_H
#define BASIC_FRAMEWORK_H

#include <basic/basic.h>

#include <stdint.h>

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

struct loop_config {
    const struct game* game;
};

uint32_t loop_init(const struct loop_config* config);
void     loop_update(void);
void     loop_terminate(void);
void     loop_on_event(struct os_event*);

#endif /* BASIC_FRAMEWORK_H */
