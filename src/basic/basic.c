#include "basic/basic.h"
#include <stdio.h>

uint32_t run_game(const struct game* g, const struct platform* p) {
    uint32_t err = 0;
    err = g->init();
    if (err) return err;
    const double target_frame_time = 1.0 / 60.0;   
    double last_time = p->now();

  while (g->running) {
        p->poll_events();
        double current_time = p->now();
        double dt = current_time - last_time;
        last_time = current_time;

        printf("dt: %lf\n", dt);
        g->update();

        // Frame pacing
        double frame_end = p->now();
        double frame_time = frame_end - current_time;
        double remaining = target_frame_time - frame_time;

        if (remaining > 0) {
            if (remaining > 0.003) {
                p->sleep((uint32_t)((remaining - 0.001) * 1000.0));
            }

            // spin wait for the last ~1ms
            while (p->now() - current_time < target_frame_time) {
                // busy wait
            }
        }
    }
    g->terminate();

    return err;
}

#include "platform_win32.c"
