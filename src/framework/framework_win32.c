#include <stdio.h>
#if defined(_WIN32) || defined(_WIN64)

/************************************************************
 *                       TODO
 * logger
 * get window config from game
 * app icon
 *
 ************************************************************/

#include "framework.h"

#include <Windows.h>

#define BASIC_WINDOW_CLASS "basic_window"

static LRESULT CALLBACK win_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);
static uint32_t         win32_open_window(void);
static double           win32_get_time(void);

static uint32_t           g_exit_requested = 0;
static HWND               g_hwnd = 0;
static HINSTANCE          g_hinstance = 0;
static LARGE_INTEGER      g_clock_frequency = {0};
static LARGE_INTEGER      g_clock_start = {0};
static struct loop_config g_loop_config = {0};

int APIENTRY WinMain(HINSTANCE hinstance, HINSTANCE hprev, LPSTR args, int cmdshow)
{
    g_hinstance = hinstance;
    (void)hprev;
    (void)args;
    (void)cmdshow;

    QueryPerformanceFrequency(&g_clock_frequency);
    QueryPerformanceCounter(&g_clock_start);
    uint32_t err = 0;
    err = win32_open_window();
    if (err) {
        printf("[err] window creation failed\n");
        return err;
    }
    g_loop_config.game = load_game();
    if (!g_loop_config.game) {
        printf("[err] game loading failed\n");
        return -1;
    };

    err = loop_init(&g_loop_config);
    if (err) {
        printf("[err] loop initialization failed\n");
        return err;
    }

    ShowWindow(g_hwnd, SW_SHOW);

    double last_time = win32_get_time();
    const double target_frame_time = 1.0 / 60.0;
    while (!g_exit_requested && !g_loop_config.game->exit_requested) {

        double frame_start = win32_get_time();

        MSG msg;
        while (PeekMessage(&msg, g_hwnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        double dt = frame_start = last_time;
        loop_update();
        double frame_end = win32_get_time();
        double elapsed = frame_end - frame_start;
        double remaining = target_frame_time - elapsed;

        while (remaining > 0) {
            if (remaining > target_frame_time * 0.25) {
                Sleep(1);
            }
            frame_end = win32_get_time();
            elapsed = frame_end - frame_start;
            remaining = target_frame_time - elapsed;

            (void)dt;
        }
    };
    loop_terminate();
    return err;
}

uint32_t win32_open_window(void)
{
    WNDCLASSEX wc = {
        .lpfnWndProc = win_proc,
        .hInstance = g_hinstance,
        .lpszClassName = BASIC_WINDOW_CLASS,
        .cbSize = sizeof(WNDCLASSEX),
    };
    RegisterClassEx(&wc);

    DWORD win_style = WS_THICKFRAME | WS_SYSMENU;
    g_hwnd = CreateWindowEx(0,           // Optional window styles.
            BASIC_WINDOW_CLASS,   // Window class
            "basic window",       // Window text
            win_style,            // Window style
            CW_USEDEFAULT,        // Size and position
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            NULL,                 // Parent window
            NULL,                 // Menu
            g_hinstance,          // Instance handle
            NULL                  // Additional application data
            );
    if (!(g_hwnd)){
        printf("Error, failed to create game instance!\n");
        return 1;
    }
    return 0;
}

double win32_get_time(void)
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - g_clock_start.QuadPart) 
        / (double)g_clock_frequency.QuadPart;
}

LRESULT CALLBACK win_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
{
    struct os_event ev = {0};
    switch (msg) {
        case WM_DESTROY:
        case WM_CLOSE:
            g_exit_requested = 1;
            break;
        case WM_KEYUP:
        case WM_KEYDOWN:
            ev.kind = msg == WM_KEYDOWN ? OS_EVENT_KEY_PRESS : OS_EVENT_KEY_RELEASE;
            ev.key.code = w_param;
            if (w_param == VK_ESCAPE) {
                ev.key.code = KEY_ESCAPE;
            }
            loop_on_event(&ev);

        default:
            break;
    }
    return DefWindowProc(hwnd, msg, w_param, l_param);
}

#endif /* defined(_WIN32) || defined(_WIN64) */


