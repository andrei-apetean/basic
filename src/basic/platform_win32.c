#include <stdio.h>
#if defined(_WIN32) || defined(_WIN64)

#include <stdlib.h>

#include "basic/basic.h"

#define BASIC_WINDOW_CLASS "basic_window"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static struct {
    HINSTANCE     instance;
    HWND          hwnd;
    LARGE_INTEGER freq;
    LARGE_INTEGER start;

} platform_state;

double time_now_win32(void);
void sleep_win32(uint64_t ms);
void poll_events_win32(void);

static const struct platform platform_win32 = {
    .now = time_now_win32,
    .sleep = sleep_win32,
    .poll_events = poll_events_win32,
};

LRESULT CALLBACK win_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);

int WinMain(HINSTANCE h_instance, HINSTANCE prev_inst, char* p_cmd_line, int ncmdshow) {
    unused(prev_inst);
    unused(p_cmd_line);
    unused(ncmdshow);
    platform_state.instance = h_instance;
    QueryPerformanceFrequency(&platform_state.freq);
    QueryPerformanceCounter(&platform_state.start);

    struct game* game = create_game();
    if (!game) {
        printf("Error, failed to create game instance!\n");
        return -1;
    }

    WNDCLASSEX wc = {
        .lpfnWndProc = win_proc,
        .hInstance = h_instance,
        .lpszClassName = BASIC_WINDOW_CLASS,
        .cbSize = sizeof(WNDCLASSEX),
    };
    RegisterClassEx(&wc);

    platform_state.hwnd = CreateWindowEx(0,           // Optional window styles.
            BASIC_WINDOW_CLASS,   // Window class
            "basic window",       // Window text TODO
            WS_OVERLAPPEDWINDOW,  // Window style
            CW_USEDEFAULT,        // Size and position
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            NULL,                 // Parent window
            NULL,                 // Menu
            h_instance,           // Instance handle
            NULL                  // Additional application data
            );
    if (!(platform_state.hwnd)){
        printf("Error, failed to create game instance!\n");
        return -1;
    }

    ShowWindow(platform_state.hwnd, SW_SHOW);

    uint32_t err = 0;
    err = run_game(game, &platform_win32);

    if (platform_state.hwnd) DestroyWindow(platform_state.hwnd);
    return err;
}

LRESULT CALLBACK win_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    switch (msg) {
        case WM_DESTROY:
            exit(-1);
            break;
        default:
            return DefWindowProc(hwnd, msg, w_param, l_param);
    }
}

double time_now_win32(void) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - platform_state.start.QuadPart) 
        / (double)platform_state.freq.QuadPart;
}
void sleep_win32(uint64_t ms) {
    Sleep(ms);
}
void poll_events_win32(void) {
    MSG msg;
    while(PeekMessage(&msg, platform_state.hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

#endif /* _WIN32 || _WIN64 */
