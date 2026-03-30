#if defined(_WIN32) || defined(_WIN64)
#include <stdio.h>
#include <stdlib.h>

#include "basic/basic.h"
#include "basic/basic_native.h"

#define BASIC_WINDOW_CLASS "basic_window"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static struct {
    struct win32_state state;
    LARGE_INTEGER freq;
    LARGE_INTEGER start;
} pwin32;

double time_now_win32(void);
void sleep_win32(uint32_t ms);
void poll_events_win32(void);

const struct platform_itf platform = {
    .now = time_now_win32,
    .sleep = sleep_win32,
    .poll_events = poll_events_win32,
    .window_api = WINDOW_API_WIN32,
};

LRESULT CALLBACK win_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);

int WinMain(HINSTANCE h_instance, HINSTANCE prev_inst, char* p_cmd_line, int ncmdshow) {
    unused(prev_inst);
    unused(p_cmd_line);
    unused(ncmdshow);
    pwin32.state.hinstance = h_instance;
    QueryPerformanceFrequency(&pwin32.freq);
    QueryPerformanceCounter(&pwin32.start);

    struct game_itf* game = create_game();
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

    pwin32.state.hwnd = CreateWindowEx(0,           // Optional window styles.
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
    if (!(pwin32.state.hwnd)){
        printf("Error, failed to create game instance!\n");
        return -1;
    }

    ShowWindow(pwin32.state.hwnd, SW_SHOW);

    uint32_t err = 0;
    err = run_game(game);

    if (pwin32.state.hwnd) DestroyWindow(pwin32.state.hwnd);
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
    return (double)(now.QuadPart - pwin32.start.QuadPart) 
        / (double)pwin32.freq.QuadPart;
}
void sleep_win32(uint32_t ms) {
    Sleep(ms);
}
void poll_events_win32(void) {
    MSG msg;
    while(PeekMessage(&msg, pwin32.state.hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void get_win32_state(struct win32_state* s) {
    *s = (struct win32_state){
        .hwnd = pwin32.state.hwnd,
        .hinstance = pwin32.state.hinstance,
    };
}

#else

#include <assert.h>

void get_win32_state(struct win32_state* s) {
    (void)s;
    assert(0 && "get_win32_state called on platform different from WIN32");
}

#endif /* _WIN32 || _WIN64 */
