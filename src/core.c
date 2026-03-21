#include "basic.h"

static const char* err_strs[ERR_COUNT] = {
    [ERR_OK] = "Success",
    [ERR_FAIL] = "Operation failed",
};

static_assert(ERR_COUNT == 2, "error codes have been modified");

const char* err_str(err_c err)
{
    if (err < 0 || err > ERR_COUNT) {
        return "Invalid error code";
    }
    return err_strs[err];
}

uint32_t run_application(struct application* app)
{
    while (app->is_running) {
        app->poll_events(app);
    }
    return 0;
}
