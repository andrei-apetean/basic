#include <stdint.h>
#include <stdio.h>

enum input_event_kind {
    INPUT_KEY,
    INPUT_POINTER,
    INPUT_WINDOW_SIZE,
    INPUT_WINDOW_CLOSE,
};

union input_event {
    struct {
        uint32_t code;
        uint32_t pressed;
    } key;
    struct {
        uint32_t x, y;
        uint32_t dx, dy;
    } pointer;
    struct {
        uint32_t width;
        uint32_t height;
    } win_size;
};

struct render_device;

struct platform* platform;
struct render_device* render_device;
uint32_t g_exit_requested;

void init(void);
void terminate(void);
void update(void);

int main(void) {
    printf("hello world!\n");

    init();

    while (!g_exit_requested) {
        update();
    }

    terminate();
    printf("goodbye!\n");

    return 0;
}
