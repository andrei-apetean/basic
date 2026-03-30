#include "basic/basic.h"
#include <stdio.h>

uint32_t run_game(const struct game_itf* g) {
    uint32_t err = 0;

    err = g->init();
    if (err) return err;

    const double target_frame_time = 1.0 / 60.0;   
    double last_time = platform.now();

    while (g->running) {
        double frame_start = platform.now();

        platform.poll_events();

        double dt = frame_start - last_time;
        last_time = frame_start;

        printf("dt: %lf\n", dt);
        g->update();

        double frame_end = platform.now();
        double elapsed = frame_end - frame_start;

        double remaining = target_frame_time - elapsed;

        while (remaining > 0) {
            if (remaining > target_frame_time * 0.25) {
                platform.sleep(1);
            }

            frame_end = platform.now();
            elapsed = frame_end - frame_start;
            remaining = target_frame_time - elapsed;
        }
    }

    g->terminate();

    return err;
}

#include "platform_win32.c"
