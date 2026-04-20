#include <basic/basic.h>
#include <stdatomic.h>
#include "app.h"

/* must be a power of two */
#define EVENT_QUEUE_CAPACITY 256
#define EVENT_QUEUE_MASK (EVENT_QUEUE_CAPACITY - 1)

static uint32_t pop_event(struct app_event* event);
static uint32_t no_op_on_init(void);
static void     no_op_on_void(void);
static void     no_op_on_event(struct app_event* event);

static struct {
    struct app_event buf[EVENT_QUEUE_CAPACITY];
    atomic_uint write_head;
    atomic_uint read_head;
} g_event_queue;

static uint32_t         g_shutdown;

void app_push_event(struct app_event* event)
{
    uint32_t head = atomic_load_explicit(
            &g_event_queue.write_head, memory_order_relaxed);
    uint32_t next =  (head + 1) & EVENT_QUEUE_MASK;
    uint32_t tail = atomic_load_explicit(
            &g_event_queue.read_head, memory_order_acquire);

    if (next == tail) return; /* full, drop event (resize storms, fine to drop)*/

    g_event_queue.buf[head] = *event;
    atomic_store_explicit(&g_event_queue.write_head, next, memory_order_release);
}

void app_thread_run(struct app_desc* app)
{
    const app_fn_init fn_init = app->fn_init != NULL
        ? app->fn_init : no_op_on_init;

    const app_fn_update fn_update = app->fn_update != NULL
        ? app->fn_update : no_op_on_void;

    const app_fn_terminate fn_terminate = app->fn_terminate != NULL
        ? app->fn_terminate : no_op_on_void;

    const app_fn_event fn_event = app->fn_event != NULL
        ? app->fn_event : no_op_on_event;

    const float target_ms = 1000.0f / (app->target_fps ? app->target_fps : 60);

    uint32_t err = fn_init();
    if (err) return;

    uint64_t last_time = os_get_time();

    while(!g_shutdown) {
        uint64_t frame_start = os_get_time();
        float dt             = (frame_start - last_time) * 0.0001f;
        last_time            = frame_start;
        struct app_event event;
        while(pop_event(&event)) {
            if (event.kind == APP_EVENT_CLOSE)
                g_shutdown = 1;
            else
                fn_event(&event);
        };

        if (g_shutdown) break;

        fn_update();
        (void) dt;
        uint64_t elapsed   = os_get_time() - frame_start;

        if (elapsed < target_ms) {
            os_sleep((uint32_t)(target_ms - elapsed));
        }
    };
    fn_terminate();
}

void quit_app(void)
{
    g_shutdown = 1;
}

static uint32_t pop_event(struct app_event* event)
{
    uint32_t tail = atomic_load_explicit(&g_event_queue.read_head,
                                         memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&g_event_queue.write_head,
                                         memory_order_acquire);
    if (tail == head) return 0;
 
    uint32_t next = (tail + 1) & EVENT_QUEUE_MASK;

    *event = g_event_queue.buf[tail];
    atomic_store_explicit(&g_event_queue.read_head, next, memory_order_release);
    return 1;
}

static uint32_t no_op_on_init(void) { return 0; /* no op */ }
static void     no_op_on_void(void) { /* no op */ }
static void     no_op_on_event(struct app_event* event) { (void)event; /* no op*/ }

