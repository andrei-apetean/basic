#include <basic/basic.h>

void     app_push_event(struct app_event* event);
void     app_thread_run(struct app_desc* app);
uint64_t os_get_time(void);
void     os_sleep(uint64_t ms);
