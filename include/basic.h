#ifndef BASIC_H
#define BASIC_H

#include <stdint.h>
#include <stddef.h>

#define unused(x) (void)(x)
#define container_of(ptr, type, member) ((void*)((char *)(ptr) - offsetof(type, member)))


typedef enum err_c {
    ERR_OK = 0,
    ERR_FAIL,
    ERR_COUNT,
} err_c;

const char* err_str(err_c err);

struct window;

struct application {
    struct window* window;
    uint32_t is_running;
    void (*poll_events)(struct application*);
};

uint32_t run_application(struct application* app);

#endif
