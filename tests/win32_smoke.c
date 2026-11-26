#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <sunfleck/ui.h>
#include <stdio.h>
#include <string.h>
#ifdef SUNFLECK_TEST_ANGLE
#include <GLES3/gl3.h>
#endif

static WNDPROC original_proc;
static unsigned paint_count;
static int capture_desktop;

static LRESULT CALLBACK count_paint(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_PAINT) {
        ++paint_count;
    }
    return CallWindowProcW(original_proc, window, message, wparam, lparam);
}

static BOOL CALLBACK capture_window(HWND window, LPARAM result)
{
    wchar_t name[32];
    if (GetClassNameW(window, name, 32) && lstrcmpW(name, L"SunfleckPreview") == 0) {
        *(HWND*)result = window;
        return FALSE;
    }
    return TRUE;
}

static COLORREF button_color(HWND window, int x, int y)
{
    DwmFlush();
    // Flip-model swapchains do not update the legacy GDI client bitmap.
    // Verify that the pixel belongs to this window before sampling the desktop.
    POINT point = {x,y};
    if (capture_desktop) {
        if (!ClientToScreen(window, &point)) return CLR_INVALID;
        if (GetAncestor(WindowFromPoint(point), GA_ROOT) != window) return CLR_INVALID;
    }
    HWND target = capture_desktop ? NULL : window;
    HDC dc = GetDC(target);
    if (!dc) return CLR_INVALID;
    COLORREF color = GetPixel(dc, point.x, point.y);
    ReleaseDC(target, dc);
    return color;
}

/* Flip-model presentation and the desktop window-open animation are asynchronous.
   Keep the exact pixel assertion, but wait for the compositor to expose the frame. */
static int wait_button_color(HWND window, int x, int y, COLORREF expected)
{
    const ULONGLONG deadline = GetTickCount64() + 2000;
    COLORREF actual;
    do {
        actual = button_color(window, x, y);
        if (actual == expected) return 1;
        Sleep(10);
    } while (GetTickCount64() < deadline);
    fprintf(stderr, "Window pixel (%d,%d): expected %08lx, got %08lx\n",
        x, y, (unsigned long)expected, (unsigned long)actual);
    return 0;
}

static int renderer_title_is(HWND window, const wchar_t* expected)
{
    wchar_t title[64];
    return GetWindowTextW(window, title, 64) && lstrcmpW(title, expected) == 0;
}

static int verify(ui_app* app, const wchar_t* expected_title)
{
    HWND window = NULL;
    EnumThreadWindows(GetCurrentThreadId(), capture_window, (LPARAM)&window);
    if (!window || ui_app_poll(app, 0) != UI_OK) {
        return 1;
    }
    if (capture_desktop && !SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)) return 1;
    const int point = MulDiv(32, GetDpiForWindow(window), 96);
    const POINT hit_point = {point, point};
    if (capture_desktop && ChildWindowFromPointEx(window, hit_point,
            CWP_SKIPDISABLED | CWP_SKIPINVISIBLE) != window) return 14;
    if (!wait_button_color(window, point, point, RGB(136, 73, 0)) ||
        !PostMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(point, point)) ||
        ui_app_poll(app, 0) != UI_OK ||
        !wait_button_color(window, point, point, RGB(243, 223, 190))) {
        return 2;
    }
    if (expected_title && !renderer_title_is(window, expected_title)) {
        return 13;
    }
    if (capture_desktop && expected_title && lstrcmpW(expected_title, L"Sunfleck [CPU]") == 0 &&
        FindWindowExW(window, NULL, L"SunfleckSkiaPresentation", NULL)) return 15;
    SetFocus(window);
    if (!PostMessageW(window, WM_KEYUP, VK_SPACE, 0) ||
        ui_app_poll(app, 0) != UI_OK ||
        !wait_button_color(window, point, point, RGB(136, 73, 0))) {
        return 5;
    }
    if (!SetWindowPos(window, NULL, 0, 0, 520, 360, SWP_NOMOVE | SWP_NOZORDER) ||
        !UpdateWindow(window)) {
        return 6;
    }
    RECT client;
    if (!GetClientRect(window, &client)) {
        return 6;
    }
    /* Sample the client background inside the compositor's rounded corners. */
    const int inset = MulDiv(16, GetDpiForWindow(window), 96);
    if (!wait_button_color(window, client.right - inset, client.bottom - inset, RGB(251, 248, 242))) {
        fprintf(stderr, "Resized client: %ldx%ld\n", client.right, client.bottom);
        return 6;
    }
    /* Let activation and exposure messages from opening the window settle. */
    Sleep(100);
    ui_app_poll(app, 0);
    original_proc = (WNDPROC)SetWindowLongPtrW(window, GWLP_WNDPROC, (LONG_PTR)count_paint);
    if (!original_proc) {
        return 3;
    }
    const ULONGLONG idle_deadline = GetTickCount64() + 4000;
    ULONGLONG quiet_start = GetTickCount64();
    while (GetTickCount64() - quiet_start < 1000) {
        const ui_result status = ui_app_poll(app, 0);
        if (status != UI_TIMEOUT && status != UI_OK) {
            return 3;
        }
        /* Late compositor exposure can request one legitimate paint. Require a
           full quiet second within four seconds; continuous invalidation fails. */
        if (paint_count) {
            quiet_start = GetTickCount64();
            paint_count = 0;
        }
        if (GetTickCount64() >= idle_deadline) {
            fprintf(stderr, "Window did not reach a quiet interval\n");
            return 3;
        }
        Sleep(10);
    }
    if (!PostMessageW(window, WM_CLOSE, 0, 0) ||
        ui_app_poll(app, 0) != UI_EXITED || IsWindow(window)) {
        return 4;
    }
    return 0;
}

static int verify_gallery(ui_app* app, HWND window)
{
    const unsigned dpi = GetDpiForWindow(window);
    RECT target = {0, 0, MulDiv(320, dpi, 96), MulDiv(200, dpi, 96)};
    if (!AdjustWindowRectExForDpi(&target, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi) ||
        !SetWindowPos(window, NULL, 0, 0, target.right - target.left, target.bottom - target.top,
                      SWP_NOMOVE | SWP_NOZORDER) ||
        ui_app_poll(app, 0) != UI_OK) {
        return 10;
    }
    const int x = MulDiv(56, dpi, 96);
    const int y = MulDiv(158, dpi, 96);
    if (!wait_button_color(window, x, y, RGB(136, 73, 0)) ||
        !PostMessageW(window, WM_MOUSEWHEEL, MAKEWPARAM(0, -WHEEL_DELTA), 0) ||
        ui_app_poll(app, 0) != UI_OK ||
        !wait_button_color(window, x, y, RGB(255, 253, 248))) {
        return 11;
    }
    return ui_app_request_close(app) == UI_OK && ui_app_poll(app, 0) == UI_EXITED &&
        !IsWindow(window) ? 0 : 12;
}

int main(int argc, char** argv)
{
    ui_app* app = NULL;
    int result = 1;
    const char* mode = argc > 1 ? argv[1] : "software";
    const int auto_mode = strcmp(mode, "auto") == 0 || strcmp(mode, "auto-present") == 0;
    const int angle_mode = strcmp(mode, "angle") == 0 || strcmp(mode, "angle-present") == 0;
    const int skia_mode = strcmp(mode, "skia") == 0;
    const int d3d_mode = strcmp(mode, "d3d11") == 0;
    const int gallery_mode = strcmp(mode, "gallery") == 0 || strcmp(mode, "gallery-gpu") == 0;
    const int gpu_mode = strcmp(mode, "software") != 0 && strcmp(mode, "gallery") != 0 && !auto_mode && !angle_mode && !d3d_mode && !skia_mode;
#ifdef SUNFLECK_TEST_SKIA_D3D12
    capture_desktop = skia_mode || auto_mode;
#endif
    ui_app_options options = {sizeof(options), UI_API_VERSION,
        gpu_mode ? UI_GLES3 : angle_mode ? UI_ANGLE : d3d_mode ? UI_D3D11 :
        (auto_mode || skia_mode) ? UI_AUTO : UI_SOFTWARE, UI_SHOW_RENDERER};
    if (ui_app_create(&options, &app) == UI_OK) {
        if (gallery_mode && ui_app_set_gallery(app, 0, 0) != UI_OK) {
            ui_app_destroy(app);
            return 10;
        }
        const ui_result opened = ui_app_open_window(app);
        HWND window = NULL;
        EnumThreadWindows(GetCurrentThreadId(), capture_window, (LPARAM)&window);
        if (strcmp(mode, "failure") == 0) {
            result = opened == UI_ERROR_PLATFORM && !window ? 0 : 7;
        } else if (opened == UI_OK) {
#ifdef SUNFLECK_TEST_ANGLE
            if (gpu_mode) {
                HMODULE gles = GetModuleHandleW(L"libGLESv2.dll");
                typedef const GLubyte* (GL_APIENTRY *GetString)(GLenum);
                const GetString get_string = gles ? (GetString)GetProcAddress(gles, "glGetString") : NULL;
                const char* renderer = get_string ? (const char*)get_string(GL_RENDERER) : NULL;
                printf("GL_RENDERER: %s\n", renderer ? renderer : "(none)");
                if (!renderer || !strstr(renderer, "ANGLE")) {
                    ui_app_destroy(app);
                    return 8;
                }
            }
#endif
            if (gallery_mode) {
                result = verify_gallery(app, window);
            } else if (strcmp(mode, "swap") == 0) {
                result = window && ui_app_poll(app, 0) == UI_OK &&
                    PostMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(
                        MulDiv(32, GetDpiForWindow(window), 96), MulDiv(32, GetDpiForWindow(window), 96))) &&
                    ui_app_poll(app, 0) == UI_ERROR_PLATFORM &&
                    !IsWindow(window) && ui_app_poll(app, 0) == UI_EXITED ? 0 : 9;
            } else {
                const int fallback = strcmp(mode, "auto-present") == 0 || strcmp(mode, "angle-present") == 0;
                if ((fallback || d3d_mode || skia_mode) && !renderer_title_is(window, L"Sunfleck [GPU]")) {
                    result = 13;
                } else {
                    const wchar_t* title = !fallback && (d3d_mode || skia_mode || gpu_mode)
                        ? L"Sunfleck [GPU]" : L"Sunfleck [CPU]";
                    result = verify(app, title);
                }
            }
        }
    }
    ui_app_destroy(app);
    if (result) {
        fprintf(stderr, "Win32 %s check failed: %d\n", mode, result);
    }
    return result;
}
