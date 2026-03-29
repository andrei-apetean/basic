#ifndef BASIC_H
#define BASIC_H

#include <stddef.h>
#include <stdint.h>

#define unused(x) (void)(x)
#define container_of(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))

struct platform_window;

struct game {
    uint32_t (*init)(void);
    void     (*update)(void);
    void     (*terminate)(void);
    uint32_t running;
};
struct platform {
    double (*now)(void);
    void   (*sleep)(uint64_t ms);
    void   (*poll_events)(void);
};

extern struct game* create_game(void);

uint32_t run_game(const struct game* g, const struct platform* p);

#endif
