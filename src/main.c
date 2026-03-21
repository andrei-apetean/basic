
#include "basic.h"
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#pragma comment(lib, "user32.lib")

#define BASIC_WINDOW_CLASS "basic_window"

struct win32_application
{
    struct application base;
    HWND hwnd;
    HINSTANCE hinstance;
};

LRESULT CALLBACK win_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
{
    switch (msg)
    {
    case WM_DESTROY:
        exit(-1);
        break;
    default:
        return DefWindowProc(hwnd, msg, w_param, l_param);
    }
}

static uint32_t create_window(HWND *hwnd, HINSTANCE hinstance)
{
    WNDCLASSEX wc = {
        .lpfnWndProc = win_proc,
        .hInstance = hinstance,
        .lpszClassName = BASIC_WINDOW_CLASS,
        .cbSize = sizeof(WNDCLASSEX)
    };

    RegisterClassEx(&wc);

    *hwnd = CreateWindowEx(
        0,                   // Optional window styles.
        BASIC_WINDOW_CLASS,  // Window class
        "basic window",      // Window text TODO
        WS_OVERLAPPEDWINDOW, // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

        NULL,      // Parent window
        NULL,      // Menu
        hinstance, // Instance handle
        NULL       // Additional application data
    );
    if (!(*hwnd))
        return ERR_FAIL;

    ShowWindow(*hwnd, SW_SHOW);
    return ERR_OK;
}

static void poll_events_win32(struct application *app)
{
    struct win32_application *self = container_of(app, struct win32_application, base);
    MSG msg;
    while (PeekMessageA(&msg, self->hwnd, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

int WinMain(HINSTANCE h_instance, HINSTANCE h_prev_instance, char *p_cmd_line, int ncmdshow)
{
    unused(h_prev_instance);
    unused(p_cmd_line);
    unused(ncmdshow);

    uint32_t err = ERR_OK;
    struct win32_application app = {
        .hinstance = h_instance,
    };
    err = create_window(&app.hwnd, h_instance);
    if (err)
    {
        // TODO display error message
        return ERR_FAIL;
    }
    app.base.is_running = 1,
    app.base.poll_events = poll_events_win32,
    run_application(&app.base);
    DestroyWindow(app.hwnd);
}

#endif

#include "core.c"
