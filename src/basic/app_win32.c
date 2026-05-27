#include <stdio.h>
#if defined(_WIN32) || defined(_WIN64)

#define WIN32_LEAN_AND_MEAN
#define BASIC_WINDOW_CLASS "basic_window"
#define BASIC_APPLICATION_TITLE "Basic Application"
#define TITLE_BAR_HEIGHT 30

#include <windowsx.h>
#include <Windows.h>

#include <basic/basic.h>
#include "app.h"

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static uint32_t         win32_create_window(struct window* window);
static uint32_t         win32_translate_keycode(uint32_t code);
static DWORD WINAPI     win32_game_thread(void *arg);

static const struct app_desc* g_app;
static uint32_t g_custom_decorations;
static uint32_t g_titlebar_height;

int APIENTRY WinMain(HINSTANCE hinstance, HINSTANCE hprev, LPSTR args, int cmdshow)
{
    (void)hprev;
    (void)args;
    (void)cmdshow;

    g_app = load_app();
    if (!g_app) return -1;

    g_custom_decorations = g_app->custom_decorations;
    g_titlebar_height = g_app->titlebar_height;

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

    uint32_t width = window->width > 0 ? window->width : 1080;
    uint32_t height = window->height > 0 ? window->height : 720;
    DWORD win_style = WS_OVERLAPPEDWINDOW;
    DWORD ex_style  = 0;
    int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
    int show = SW_SHOW;
    const char* title = window->title ? window->title : BASIC_APPLICATION_TITLE;

    switch (window->mode) {
        case WINDOW_MODE_INVALID:
        case WINDOW_MODE_WINDOWED: {
            show = SW_SHOW;
            break;
        }
        case WINDOW_MODE_MAXIMIZED: {
            HMONITOR mon  = MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
            MONITORINFO info = { .cbSize = sizeof(info) };
            GetMonitorInfoA(mon, &info);

            win_style = WS_OVERLAPPEDWINDOW;
            ex_style  = WS_EX_TOPMOST;
            x = info.rcMonitor.left;
            y = info.rcMonitor.top;
            width = info.rcMonitor.right  - info.rcMonitor.left;
            height = info.rcMonitor.bottom - info.rcMonitor.top;
            show = SW_MAXIMIZE;
            break;
        }
        case WINDOW_MODE_FULLSCREEN: {
            RECT rect = { 0, 0, (LONG)width, (LONG)height };
            AdjustWindowRectEx(&rect, win_style, FALSE, ex_style);
            width = rect.right  - rect.left;
            height = rect.bottom - rect.top;
            break;
        }
        default: 
            return -1;
    }

    window->win32.hwnd = CreateWindowExA(
            ex_style,
            BASIC_WINDOW_CLASS,
            title,
            win_style,
            x, y, width, height,
            NULL, NULL,
            window->win32.hinstance,
            NULL);
    if (!window->win32.hwnd) return -1;

    if (g_custom_decorations) {
        SetWindowPos(window->win32.hwnd, NULL, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }

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
                    .key  = { win32_translate_keycode(wp)},
                    });
            return 0;

        case WM_KEYUP: {
            app_push_event(&(struct app_event){
                    .kind = APP_EVENT_KEY_UP,
                    .key  = { win32_translate_keycode(wp)},
                    });
            return 0;
        }

        case WM_MOUSEMOVE:
            app_push_event(&(struct app_event){
                    .kind       = APP_EVENT_MOUSE_MOVE,
                    .mouse_move = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) },
                    });
            return 0;

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            app_push_event(&(struct app_event){
                    .kind         = APP_EVENT_MOUSE_DOWN,
                    .mouse_button = {
                    GET_X_LPARAM(lp), GET_Y_LPARAM(lp),
                    (msg == WM_LBUTTONDOWN) ? BUTTON_LEFT :
                    (msg == WM_RBUTTONDOWN) ? BUTTON_RIGHT : BUTTON_MIDDLE
                    }
                    });
            return 0;
        }

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: {
            app_push_event(&(struct app_event){
                    .kind         = APP_EVENT_MOUSE_UP,
                    .mouse_button = {
                    GET_X_LPARAM(lp), GET_Y_LPARAM(lp),
                    (msg == WM_LBUTTONUP) ? BUTTON_LEFT :
                    (msg == WM_RBUTTONUP) ? BUTTON_RIGHT : BUTTON_MIDDLE
                    },
                    });
            return 0;
        }
        case WM_CLOSE: {
            app_push_event(&(struct app_event){ .kind = APP_EVENT_CLOSE });
            PostQuitMessage(0);
            return 0;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }

        case WM_NCCALCSIZE: {
            if (!wp) break;
            if (g_custom_decorations) {
                return 0;
            } else {
                break;
            }
        }

        case WM_NCLBUTTONDOWN: {
            if (g_custom_decorations) {
                POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                ScreenToClient(hwnd, &pt);
                app_push_event(&(struct app_event){
                        .kind         = APP_EVENT_MOUSE_DOWN,
                        .mouse_button = { pt.x, pt.y, BUTTON_LEFT },
                        });
            }
            // fall through to DefWindowProc so dragging still works
            break;
        }

        case WM_NCLBUTTONUP: {
            if (g_custom_decorations) {
                POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                ScreenToClient(hwnd, &pt);
                app_push_event(&(struct app_event){
                        .kind         = APP_EVENT_MOUSE_UP,
                        .mouse_button = { pt.x, pt.y, BUTTON_LEFT },
                        });
            }
            break;
        }

        case WM_NCMOUSEMOVE: {
            if (g_custom_decorations) {
                POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                ScreenToClient(hwnd, &pt);
                app_push_event(&(struct app_event){
                        .kind       = APP_EVENT_MOUSE_MOVE,
                        .mouse_move = { pt.x, pt.y },
                        });
            }
            break;
        }

        case WM_NCHITTEST: {
            if (g_custom_decorations) {
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
                if (pt.y < border + (long)g_titlebar_height) return HTCAPTION;
                return HTCLIENT;
            }
            break;
        }

        case WM_ERASEBKGND: {
            return TRUE;
        }

        case WM_SETCURSOR: {
            if (g_custom_decorations) {
                WORD ht = LOWORD(lp);
                if (ht == HTCLIENT) {
                    SetCursor(LoadCursor(NULL, IDC_ARROW));
                    return TRUE;  /* TRUE prevents DefWindowProc from changing it */
                }
            }
            break;
        }
    }

    return DefWindowProcA(hwnd, msg, wp, lp);
}

static DWORD WINAPI win32_game_thread(void* arg)
{
    struct app_desc* app = arg;
    app_thread_run(app);

    if (app->window)
        PostMessage(app->window->win32.hwnd, WM_CLOSE, 0, 0);

    return 0;
}

uint32_t win32_translate_keycode(uint32_t code)
{
    switch (code)
    {
        case  65:  return KEY_A;
        case  66:  return KEY_B;
        case  67:  return KEY_C;
        case  68:  return KEY_D;
        case  69:  return KEY_E;
        case  70:  return KEY_F;
        case  71:  return KEY_G;
        case  72:  return KEY_H;
        case  73:  return KEY_I;
        case  74:  return KEY_J;
        case  75:  return KEY_K;
        case  76:  return KEY_L;
        case  77:  return KEY_M;
        case  78:  return KEY_N;
        case  79:  return KEY_O;
        case  80:  return KEY_P;
        case  81:  return KEY_Q;
        case  82:  return KEY_R;
        case  83:  return KEY_S;
        case  84:  return KEY_T;
        case  85:  return KEY_U;
        case  86:  return KEY_V;
        case  87:  return KEY_W;
        case  88:  return KEY_X;
        case  89:  return KEY_Y;
        case  90:  return KEY_Z;
        case  49:  return KEY_1;
        case  50:  return KEY_2;
        case  51:  return KEY_3;
        case  52:  return KEY_4;
        case  53:  return KEY_5;
        case  54:  return KEY_6;
        case  55:  return KEY_7;
        case  56:  return KEY_8;
        case  57:  return KEY_9;
        case  48:  return KEY_0;
        case  13:  return KEY_ENTER;
        case  27:  return KEY_ESCAPE;
        case   8:  return KEY_BACKSPACE;
        case   9:  return KEY_TAB;
        case  32:  return KEY_SPACE;
        case 189:  return KEY_MINUS;
        case 187:  return KEY_EQUAL;
        case 219:  return KEY_LBRACE;
        case 221:  return KEY_RBRACE;
        case 220:  return KEY_BACKSLASH;
        case 186:  return KEY_SEMICOLON;
        case 222:  return KEY_APOSTROPHE;
        case 192:  return KEY_GRAVEACCENT;
        case 188:  return KEY_COMMA;
        case 190:  return KEY_PERIOD;
        case 191:  return KEY_SLASH;
        case 112:  return KEY_F1;
        case 113:  return KEY_F2;
        case 114:  return KEY_F3;
        case 115:  return KEY_F4;
        case 116:  return KEY_F5;
        case 117:  return KEY_F6;
        case 118:  return KEY_F7;
        case 119:  return KEY_F8;
        case 120:  return KEY_F9;
        case 121:  return KEY_F10;
        case 122:  return KEY_F11;
        case 123:  return KEY_F12;
        case  39:  return KEY_RIGHT;
        case  37:  return KEY_LEFT;
        case  40:  return KEY_DOWN;
        case  38:  return KEY_UP;
        case  20:  return KEY_CAPSLOCK;
        case  17:  return KEY_LCONTROL;
        case  18:  return KEY_LALT;
        case  42:  return KEY_LSHIFT;
        case  91:  return KEY_LMETA;
        case  16:  return KEY_RSHIFT;
        default:   return KEY_ANY;
    }
}

#endif /* defined(_WIN32) || defined(_WIN64) */