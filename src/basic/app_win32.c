#include <stdio.h>
#if defined(_WIN32) || defined(_WIN64)

#define WIN32_LEAN_AND_MEAN
#define BASIC_WINDOW_CLASS "basic_window"
#define BASIC_APPLICATION_TITLE "Basic Application"
#define TITLE_BAR_HEIGHT 30

#include <Windows.h>
#include <windowsx.h>

#include <basic/basic.h>
#include "app.h"

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static uint32_t         win32_create_window(struct window* window);
static DWORD WINAPI     win32_game_thread(void *arg);

static const struct app_desc* g_app;

int APIENTRY WinMain(HINSTANCE hinstance, HINSTANCE hprev, LPSTR args, int cmdshow)
{
    (void)hprev;
    (void)args;
    (void)cmdshow;

    g_app = load_app();
    if (!g_app) return -1;

    if (g_app->window) {
        g_app->window->backend = WINDOW_BACKEND_WIN32;
        g_app->window->win32.hinstance = hinstance;
        int err = win32_create_window(g_app->window);
        if (err) {
            printf("failed to create window!\n");
            return err;
        }
    }

    HANDLE thread = CreateThread(NULL, 0, win32_game_thread, (LPVOID)g_app, 0, NULL);
    if (!thread) {
        printf("render thread creation failed\n");
        return -1;
    }

    if (g_app->window) {
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);

    return 0;
}

uint64_t os_get_time(void)
{
    static LARGE_INTEGER freq = {0};
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (uint64_t)(now.QuadPart * 1000 / freq.QuadPart);
}

void os_sleep(uint64_t ms)
{
    if (ms > 0) Sleep(ms);
}

static uint32_t win32_create_window(struct window* window)
{
    WNDCLASSEX wc = {
        .cbSize        = sizeof(WNDCLASSEX),
        .lpfnWndProc   = wnd_proc,
        .hInstance     = window->win32.hinstance,
        .lpszClassName = BASIC_WINDOW_CLASS,
        .hbrBackground = NULL, /* removes ugly white window borders */
    };
    if (!RegisterClassEx(&wc)) {
        printf("failed to register window class (%lu)\n", GetLastError());
        return -1;
    }
    const uint32_t width = window->width > 0 ? window->width : 1080;
    const uint32_t height = window->height > 0 ? window->height : 720;
    DWORD win_style = WS_OVERLAPPEDWINDOW;
    DWORD ex_style  = 0;
    int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
    int w = (int)width,    h = (int)height;
    int show = SW_SHOW;
    const char* title = window->title ? window->title : BASIC_APPLICATION_TITLE;
    switch (window->mode) {
        case WINDOW_MODE_INVALID:
        case WINDOW_MODE_WINDOWED:
            show = SW_SHOW;
            break;
        case WINDOW_MODE_MAXIMIZED:
            HMONITOR mon  = MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
            MONITORINFO info = { .cbSize = sizeof(info) };
            GetMonitorInfoA(mon, &info);

            win_style = WS_OVERLAPPEDWINDOW;
            ex_style  = WS_EX_TOPMOST;
            x = info.rcMonitor.left;
            y = info.rcMonitor.top;
            w = info.rcMonitor.right  - info.rcMonitor.left;
            h = info.rcMonitor.bottom - info.rcMonitor.top;
            show = SW_MAXIMIZE;
            break;
        case WINDOW_MODE_FULLSCREEN:
            RECT rect = { 0, 0, (LONG)width, (LONG)height };
            AdjustWindowRectEx(&rect, win_style, FALSE, ex_style);
            w = rect.right  - rect.left;
            h = rect.bottom - rect.top;
            break;
        default: 
            return -1;
    }

    window->win32.hwnd = CreateWindowExA(
        ex_style,
        BASIC_WINDOW_CLASS,
        title,
        win_style,
        x, y, w, h,
        NULL, NULL,
        window->win32.hinstance,
        NULL);
    if (!window->win32.hwnd) return -1;

    SetWindowPos(window->win32.hwnd, NULL, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    RECT client;
    GetClientRect(window->win32.hwnd, &client);
    window->width  = (uint32_t)(client.right  - client.left);
    window->height = (uint32_t)(client.bottom - client.top);

    ShowWindow(window->win32.hwnd, show);
    UpdateWindow(window->win32.hwnd);
    return 0;
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_SIZE: {
        uint32_t w = LOWORD(lp);
        uint32_t h = HIWORD(lp);
        if (w > 0 && h > 0)
            app_push_event(&(struct app_event){
                .kind   = APP_EVENT_WINDOW_SIZE,
                .resize = { w, h },
            });
        return 0;
    }
    case WM_KEYDOWN:
        app_push_event(&(struct app_event){
            .kind = APP_EVENT_KEY_DOWN,
            .key  = { (uint32_t)wp },
        });
        return 0;
    case WM_KEYUP:
        app_push_event(&(struct app_event){
            .kind = APP_EVENT_KEY_UP,
            .key  = { (uint32_t)wp },
        });
        return 0;
    case WM_MOUSEMOVE:
        app_push_event(&(struct app_event){
            .kind       = APP_EVENT_MOUSE_MOVE,
            .mouse_move = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) },
        });
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        app_push_event(&(struct app_event){
            .kind         = APP_EVENT_MOUSE_DOWN,
            .mouse_button = {
                GET_X_LPARAM(lp), GET_Y_LPARAM(lp),
                (msg == WM_LBUTTONDOWN) ? BUTTON_LEFT :
                (msg == WM_RBUTTONDOWN) ? BUTTON_RIGHT : BUTTON_MIDDLE
            },
        });
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
        app_push_event(&(struct app_event){
            .kind         = APP_EVENT_MOUSE_UP,
            .mouse_button = {
                GET_X_LPARAM(lp), GET_Y_LPARAM(lp),
                (msg == WM_LBUTTONUP) ? BUTTON_LEFT :
                (msg == WM_RBUTTONUP) ? BUTTON_RIGHT : BUTTON_MIDDLE
            },
        });
        return 0;
    case WM_CLOSE:
        app_push_event(&(struct app_event){ .kind = APP_EVENT_CLOSE });
        PostQuitMessage(0);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
/*    case WM_NCCALCSIZE:
        return 0;
    case WM_NCHITTEST:
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        RECT wr;
        GetWindowRect(hwnd, &wr);
        pt.x -= wr.left;
        pt.y -= wr.top;
        RECT rc;
        GetClientRect(hwnd, &rc);
        int border = GetSystemMetrics(SM_CXSIZEFRAME)
            + GetSystemMetrics(SM_CXPADDEDBORDER);

        if (pt.y < border) {
            if (pt.x < border)      return HTTOPLEFT;
            if (pt.x > rc.right - border) return HTTOPRIGHT;
            return HTTOP;
        }
        if (pt.y > rc.bottom - border) {
            if (pt.x < border)      return HTBOTTOMLEFT;
            if (pt.x > rc.right - border) return HTBOTTOMRIGHT;
            return HTBOTTOM;
        }
        if (pt.x < border)          return HTLEFT;
        if (pt.x > rc.right - border) return HTRIGHT;
        if (pt.y < border + TITLE_BAR_HEIGHT) return HTCAPTION;
        return HTCLIENT;
    case WM_ERASEBKGND:
        return TRUE;
    case WM_SETCURSOR: 
        WORD ht = LOWORD(lp);
        if (ht == HTCLIENT) {
            SetCursor(LoadCursor(NULL, IDC_ARROW));
        }

        */
        // return TRUE;  /* returning TRUE prevents DefWindowProc from changing it */
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static DWORD WINAPI win32_game_thread(void* arg)
{
    app_thread_run(arg);
    PostQuitMessage(0);
    return 0;
}

#endif /* defined(_WIN32) || defined(_WIN64) */

