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
static uint32_t         win32_translate_keycode(uint32_t code);

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
            ev.key.code = win32_translate_keycode(w_param);
            loop_on_event(&ev);

        default:
            break;
    }
    return DefWindowProc(hwnd, msg, w_param, l_param);
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
        default:   return KEY_UNKNOWN;
    }
}

#endif /* defined(_WIN32) || defined(_WIN64) */


