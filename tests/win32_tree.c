#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <sunfleck/ui.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ui_result set_fixed_width(ui_node* node, double pixels) {
    ui_binding_value value = {UI_VALUE_LENGTH};
    value.integer = UI_LENGTH_PIXELS; value.number = pixels;
    return ui_node_set_property(node, UI_PROPERTY_WIDTH, &value);
}


struct state {
    ui_app* app;
    ui_node* label;
    unsigned calls;
    int reentry_ok;
};

static void clicked(ui_node* node, void* context)
{
    struct state* state = context;
    uint32_t value = 0;
    ++state->calls;
    state->reentry_ok = ui_app_poll(state->app, 0) == UI_ERROR_INVALID_STATE;
    if (ui_node_get_value(node, &value) == UI_OK) {
        ui_node_set_text(state->label, value ? "On / \xe5\xbc\x80" : "Off");
    }
}

static void follow_slider(ui_node* slider, void* context)
{
    uint32_t value = 0;
    if (ui_node_get_value(slider, &value) == UI_OK) {
        ui_node_set_value(context, value);
    }
}

static ui_node* make(ui_app* app, ui_node* parent, ui_node_kind kind)
{
    ui_node* node = NULL;
    if (ui_node_create(app, kind, &node) != UI_OK ||
        (parent && ui_node_append(parent, node) != UI_OK)) {
        return NULL;
    }
    return node;
}

static BOOL CALLBACK find_window(HWND window, LPARAM context)
{
    wchar_t name[32];
    if (GetClassNameW(window, name, 32) && lstrcmpW(name, L"SunfleckPreview") == 0) {
        *(HWND*)context = window;
        return FALSE;
    }
    return TRUE;
}

static int has_value(ui_node* node, unsigned expected)
{
    uint32_t value = 0;
    return ui_node_get_value(node, &value) == UI_OK && value == expected;
}

static COLORREF client_pixel(HWND window, int x, int y)
{
    DwmFlush();
    HDC dc = GetDC(window);
    if (!dc) {
        return CLR_INVALID;
    }
    const COLORREF color = GetPixel(dc, x, y);
    ReleaseDC(window, dc);
    return color;
}

static COLORREF window_pixel(HWND window, int x, int y)
{
    const unsigned dpi = GetDpiForWindow(window);
    return client_pixel(window, MulDiv(x, dpi, 96), MulDiv(y, dpi, 96));
}

static int click(ui_app* app, HWND window, int x, int y)
{
    const unsigned dpi = GetDpiForWindow(window);
    return PostMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(MulDiv(x, dpi, 96), MulDiv(y, dpi, 96))) &&
        ui_app_poll(app, 0) == UI_OK;
}

static char* debug_snapshot(ui_app* app)
{
    size_t size = 0;
    if (ui_app_get_debug_snapshot(app, NULL, &size) != UI_OK) return NULL;
    char* text = malloc(size);
    if (text && ui_app_get_debug_snapshot(app, text, &size) != UI_OK) {
        free(text); return NULL;
    }
    return text;
}

static int verify(ui_app* app)
{
    ui_node* root = make(app, NULL, UI_STACK_PANEL);
    if (!root) {
        return 1;
    }
    ui_node* grid = make(app, root, UI_FLEX);
    ui_node* radios = make(app, root, UI_FLEX);
    ui_node* slider = make(app, root, UI_SLIDER);
    ui_node* label = make(app, root, UI_TEXT_BLOCK);
    ui_node* check = make(app, root, UI_CHECK_BOX);
    ui_node* toggle = make(app, root, UI_SWITCH);
    if (!grid || !radios || !slider || !label || !check || !toggle) {
        return 2;
    }
    ui_node* a = make(app, grid, UI_BUTTON);
    ui_node* b = make(app, grid, UI_BUTTON);
    ui_node* r1 = make(app, radios, UI_RADIO_BUTTON);
    ui_node* r2 = make(app, radios, UI_RADIO_BUTTON);
    struct state state = {app, label, 0, 0};
    if (!a || !b || !r1 || !r2 || set_fixed_width(a, 100) != UI_OK ||
        ui_node_set_text(a, "A") != UI_OK || ui_node_set_text(b, "B") != UI_OK ||
        ui_node_set_text(label, "\xff") != UI_ERROR_INVALID_ARGUMENT ||
        ui_node_set_action(a, clicked, &state) != UI_OK ||
        ui_node_append(grid, root) != UI_ERROR_INVALID_ARGUMENT ||
        ui_node_append(radios, a) != UI_ERROR_INVALID_STATE ||
        ui_app_set_root(app, root) != UI_OK || ui_app_open_window(app) != UI_OK ||
        ui_app_poll(app, 0) != UI_OK) {
        return 3;
    }
    HWND window = NULL;
    EnumThreadWindows(GetCurrentThreadId(), find_window, (LPARAM)&window);
    uint64_t selected = 0;
    char expected[64];
    char* snapshot = NULL;
    if (!window || ui_node_get_debug_id(a, &selected) != UI_OK ||
        ui_app_set_debug_picking(app, 1) != UI_OK || ui_app_poll(app, 0) != UI_OK ||
        !click(app, window, 30, 30) || state.calls != 0 || !has_value(a, 0) ||
        !(snapshot = debug_snapshot(app))) {
        return 20;
    }
    snprintf(expected, sizeof(expected), "\"selection\":\"%llu\"", (unsigned long long)selected);
    const int snapshot_matches = strstr(snapshot, expected) && strstr(snapshot, "\"picking\":false");
    free(snapshot);
    if (!snapshot_matches) {
        return 21;
    }
    if (!window || !click(app, window, 30, 30) || !has_value(a, 1) || !has_value(b, 0) ||
        state.calls != 1 || !state.reentry_ok || !click(app, window, 170, 30) ||
        !has_value(a, 1) || !has_value(b, 1)) {
        return 4;
    }
    if (!click(app, window, 30, 75) || !has_value(r1, 1) || !has_value(r2, 0) ||
        !click(app, window, 300, 75) || !has_value(r1, 0) || !has_value(r2, 1)) {
        return 5;
    }
    ui_node* progress = make(app, NULL, UI_PROGRESS_BAR);
    const unsigned dpi = GetDpiForWindow(window);
    const int slider_y = MulDiv(120, dpi, 96);
    uint32_t dragged_value = 0;
    if (!progress || ui_node_set_action(slider, follow_slider, progress) != UI_OK ||
        !PostMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(MulDiv(26, dpi, 96), slider_y)) ||
        !PostMessageW(window, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(MulDiv(150, dpi, 96), slider_y)) ||
        ui_app_poll(app, 0) != UI_OK ||
        ui_node_get_value(slider, &dragged_value) != UI_OK || dragged_value == 0 || dragged_value == 100 ||
        !has_value(progress, dragged_value) ||
        !PostMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(MulDiv(150, dpi, 96), slider_y)) ||
        ui_app_poll(app, 0) != UI_OK || !has_value(slider, dragged_value)) {
        return 10;
    }
    if (!click(app, window, 26, 120) || !has_value(slider, 0) ||
        ui_app_set_builtin_theme(app, UI_BUILTIN_MATERIAL_3, 1, 1) != UI_OK || ui_app_poll(app, 0) != UI_OK) {
        return 6;
    }
    SetFocus(window);
    if (!PostMessageW(window, WM_KEYUP, VK_RIGHT, 0) || ui_app_poll(app, 0) != UI_OK ||
        !has_value(slider, 5) || !has_value(a, 1) || !has_value(r2, 1)) {
        return 7;
    }
    if (!PostMessageW(window, WM_KEYUP, VK_TAB, 0) || ui_app_poll(app, 0) != UI_OK ||
        !PostMessageW(window, WM_KEYUP, VK_SPACE, 0) || ui_app_poll(app, 0) != UI_OK ||
        !has_value(check, 1) || !PostMessageW(window, WM_KEYUP, VK_TAB, 0) ||
        ui_app_poll(app, 0) != UI_OK || !PostMessageW(window, WM_KEYUP, VK_SPACE, 0) ||
        ui_app_poll(app, 0) != UI_OK || !has_value(toggle, 1)) {
        return 8;
    }
    return ui_app_request_close(app) == UI_OK && ui_app_poll(app, 0) == UI_EXITED &&
        !IsWindow(window) ? 0 : 9;
}

static int verify_advanced(ui_app* app)
{
    ui_node* root = make(app, NULL, UI_STACK_PANEL);
    if (!root) {
        return 20;
    }
    ui_node* combo = make(app, root, UI_COMBO_BOX);
    ui_node* text = make(app, root, UI_TEXT_BOX);
    ui_node* tabs = make(app, root, UI_TAB_VIEW);
    ui_node* list = make(app, root, UI_LIST_VIEW);
    if (!combo || !text || !tabs || !list) {
        return 21;
    }
    ui_node* first = make(app, tabs, UI_TAB_ITEM);
    ui_node* second = make(app, tabs, UI_TAB_ITEM);
    const char* const choices[] = {"One", "Two", "Three"};
    const char* const invalid[] = {"Valid", "\xff"};
    ui_node* items = first ? make(app, first, UI_ITEMS_CONTROL) : NULL;
    if (!first || !second || !items || ui_node_set_items(items, choices, 1) != UI_OK ||
        !make(app, second, UI_TEXT_BLOCK) ||
        ui_node_set_items(combo, invalid, 2) != UI_ERROR_INVALID_ARGUMENT ||
        ui_node_set_items(combo, choices, 3) != UI_OK ||
        ui_node_set_items(list, choices, 3) != UI_OK ||
        ui_node_set_text(text, "initial") != UI_OK ||
        ui_app_set_root(app, root) != UI_OK || ui_app_open_window(app) != UI_OK ||
        ui_app_poll(app, 0) != UI_OK) {
        return 22;
    }
    HWND window = NULL;
    EnumThreadWindows(GetCurrentThreadId(), find_window, (LPARAM)&window);
    if (!window) {
        return 23;
    }
    const COLORREF combo_border = window_pixel(window, 100, 16);
    if (combo_border == CLR_INVALID || !click(app, window, 30, 25) ||
        window_pixel(window, 100, 16) != combo_border || !click(app, window, 30, 108) ||
        !has_value(combo, 1) || window_pixel(window, 100, 16) != combo_border) {
        return 23;
    }
    SetFocus(window);
    SendMessageW(window, WM_KEYUP, VK_SPACE, 0);
    if (ui_app_poll(app, 0) != UI_OK || window_pixel(window, 100, 16) != combo_border) {
        return 23;
    }
    SendMessageW(window, WM_KEYUP, VK_ESCAPE, 0);
    if (ui_app_poll(app, 0) != UI_OK || !has_value(combo, 1)) {
        return 23;
    }
    if (ui_node_set_text(text, "") != UI_OK || !click(app, window, 30, 72) ||
        window_pixel(window, 26, 76) != RGB(44, 36, 29)) {
        return 29;
    }
    POINT anchor = {-1, -1};
    const unsigned dpi = GetDpiForWindow(window);
    if (!GetCaretPos(&anchor) || anchor.x != MulDiv(26, dpi, 96) ||
        anchor.y < MulDiv(62, dpi, 96) || anchor.y > MulDiv(76, dpi, 96) ||
        window_pixel(window, 26, MulDiv(anchor.y, 96, dpi) + 1) != RGB(44, 36, 29)) {
        fprintf(stderr, "IME caret: %ld,%ld; dpi=%u\n", anchor.x, anchor.y, dpi);
        return 50;
    }
    const UINT blink = GetCaretBlinkTime();
    if (blink != 0 && blink != INFINITE) {
        if (ui_app_advance_animation_clock(app, blink) != UI_OK) return 44;
        if (ui_app_poll(app, 0) != UI_OK || window_pixel(window, 26, 76) != RGB(246, 239, 228)) {
            return 44;
        }
        if (ui_app_advance_animation_clock(app, blink) != UI_OK) return 44;
        if (ui_app_poll(app, 0) != UI_OK || window_pixel(window, 26, 76) != RGB(44, 36, 29)) {
            return 45;
        }
    }
    SendMessageW(window, WM_KILLFOCUS, 0, 0);
    if (ui_app_poll(app, 0) != UI_OK || window_pixel(window, 26, 76) != RGB(246, 239, 228)) {
        return 30;
    }
    SendMessageW(window, WM_SETFOCUS, 0, 0);
    if (ui_node_set_text(text, "WW") != UI_OK || ui_app_poll(app, 0) != UI_OK ||
        !click(app, window, 40, 76) || !PostMessageW(window, WM_CHAR, 'X', 0) ||
        ui_app_poll(app, 0) != UI_OK) {
        return 46;
    }
    char inserted[16];
    uint32_t inserted_size = sizeof(inserted);
    if (ui_node_get_text(text, inserted, &inserted_size) != UI_OK || strcmp(inserted, "WXW") != 0 ||
        !PostMessageW(window, WM_KEYDOWN, VK_LEFT, 0) ||
        !PostMessageW(window, WM_KEYDOWN, VK_DELETE, 0) || ui_app_poll(app, 0) != UI_OK) {
        return 47;
    }
    inserted_size = sizeof(inserted);
    if (ui_node_get_text(text, inserted, &inserted_size) != UI_OK || strcmp(inserted, "WW") != 0) {
        return 48;
    }
    ui_text_source* source = NULL;
    ui_node* mirror = NULL;
    if (ui_text_source_create(app, "WW", &source) != UI_OK ||
        ui_node_create(app, UI_TEXT_BLOCK, &mirror) != UI_OK ||
        ui_node_bind_text(mirror, source, UI_BIND_ONE_WAY) != UI_OK ||
        ui_node_bind_text(text, source, UI_BIND_TWO_WAY) != UI_OK) {
        return 49;
    }
    if (!click(app, window, 30, 72) || !PostMessageW(window, WM_CHAR, 1, 0) ||
        !PostMessageW(window, WM_CHAR, L'A', 0) || !PostMessageW(window, WM_IME_CHAR, 0x4e2d, 0) ||
        ui_app_poll(app, 0) != UI_OK) {
        return 24;
    }
    char buffer[16] = {0};
    uint32_t size = sizeof(buffer);
    if (ui_node_get_text(text, buffer, &size) != UI_OK ||
        strcmp(buffer, "A\xe4\xb8\xad") != 0) {
        fprintf(stderr, "IME text: size=%u bytes=%02x,%02x,%02x,%02x; focus=%d\n",
                size, (unsigned char)buffer[0], (unsigned char)buffer[1],
                (unsigned char)buffer[2], (unsigned char)buffer[3], GetFocus() == window);
        return 25;
    }
    size = sizeof(buffer);
    if (ui_node_get_text(mirror, buffer, &size) != UI_OK || strcmp(buffer, "A\xe4\xb8\xad") != 0 ||
        !PostMessageW(window, WM_CHAR, 8, 0) || ui_app_poll(app, 0) != UI_OK) {
        return 50;
    }
    size = sizeof(buffer);
    if (ui_node_get_text(mirror, buffer, &size) != UI_OK || strcmp(buffer, "A") != 0 ||
        ui_text_source_destroy(source) != UI_OK) {
        return 51;
    }
    if (!click(app, window, 300, 120) || !has_value(tabs, 1)) {
        return 26;
    }
    SetFocus(window);
    if (!click(app, window, 30, 225) || !has_value(list, 0) ||
        !PostMessageW(window, WM_KEYUP, VK_DOWN, 0) || ui_app_poll(app, 0) != UI_OK ||
        !has_value(list, 1)) {
        return 28;
    }
    ui_node* nav = make(app, NULL, UI_NAVIGATION_VIEW);
    ui_node* page = nav ? make(app, nav, UI_TAB_ITEM) : NULL;
    ui_node* button = page ? make(app, page, UI_BUTTON) : NULL;
    if (!button || ui_node_set_text(button, "Save") != UI_OK ||
        ui_app_set_root(app, nav) != UI_OK || ui_app_poll(app, 0) != UI_OK) {
        return 53;
    }
    const COLORREF nav_top = window_pixel(window, 40, 16);
    const COLORREF button_top = window_pixel(window, 170, 16);
    if (!click(app, window, 300, 30) || !has_value(button, 0) ||
        !click(app, window, 170, 30) || !has_value(button, 1) ||
        window_pixel(window, 170, 16) != button_top) {
        return 54;
    }
    SendMessageW(window, WM_KEYUP, VK_TAB, 0);
    if (ui_app_poll(app, 0) != UI_OK || window_pixel(window, 40, 16) == nav_top ||
        !click(app, window, 40, 30) || window_pixel(window, 40, 16) != nav_top) {
        return 55;
    }
    return ui_app_request_close(app) == UI_OK && ui_app_poll(app, 0) == UI_EXITED ? 0 : 27;
}

static int text_is(ui_node* node, const char* expected)
{
    char buffer[64];
    uint32_t size = sizeof(buffer);
    return ui_node_get_text(node, buffer, &size) == UI_OK && strcmp(buffer, expected) == 0;
}

static int verify_extensions(ui_app* app)
{
    ui_node* root = make(app, NULL, UI_STACK_PANEL);
    if (!root) {
        return 30;
    }
    ui_node* split = make(app, root, UI_SPLIT_VIEW);
    ui_node* spin = make(app, root, UI_SPIN_BOX);
    ui_node* password = make(app, root, UI_PASSWORD_BOX);
    ui_node* area = make(app, root, UI_TEXT_AREA);
    ui_node* search = make(app, root, UI_SEARCH_BOX);
    if (!split || !spin || !password || !area || !search ||
        !make(app, split, UI_BUTTON) || !make(app, split, UI_BUTTON) ||
        ui_app_set_root(app, root) != UI_OK || ui_app_open_window(app) != UI_OK ||
        ui_app_poll(app, 0) != UI_OK) {
        return 31;
    }
    HWND window = NULL;
    EnumThreadWindows(GetCurrentThreadId(), find_window, (LPARAM)&window);
    RECT bounds;
    if (!window || !GetClientRect(window, &bounds)) {
        return 32;
    }
    const unsigned dpi = GetDpiForWindow(window);
    const int middle = bounds.right / 2;
    const int y = MulDiv(30, dpi, 96);
    if (!PostMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(middle, y)) ||
        !PostMessageW(window, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(middle + 40, y)) ||
        !PostMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(middle + 40, y)) ||
        ui_app_poll(app, 0) != UI_OK || has_value(split, 50) ||
        !click(app, window, 300, 75) || !has_value(spin, 1)) {
        return 33;
    }
    if (!click(app, window, 30, 120) || !PostMessageW(window, WM_CHAR, 'p', 0) ||
        ui_app_poll(app, 0) != UI_OK || !text_is(password, "p") ||
        !click(app, window, 30, 170) || !PostMessageW(window, WM_CHAR, 'a', 0) ||
        !PostMessageW(window, WM_CHAR, 13, 0) || !PostMessageW(window, WM_CHAR, 'b', 0) ||
        ui_app_poll(app, 0) != UI_OK || !text_is(area, "a\nb") ||
        !click(app, window, 30, 225) || !PostMessageW(window, WM_CHAR, 'q', 0) ||
        ui_app_poll(app, 0) != UI_OK || !text_is(search, "q")) {
        return 34;
    }
    ui_node* overlay_root = make(app, NULL, UI_STACK_PANEL);
    if (!overlay_root) {
        return 35;
    }
    ui_node* anchor = make(app, overlay_root, UI_BUTTON);
    ui_node* tooltip = make(app, overlay_root, UI_TOOLTIP);
    ui_node* context = make(app, overlay_root, UI_CONTEXT_MENU);
    ui_node* dialog = make(app, overlay_root, UI_DIALOG);
    ui_node* tree = make(app, overlay_root, UI_TREE_VIEW);
    ui_node* branch = tree ? make(app, tree, UI_TREE_ITEM) : NULL;
    if (!anchor || !tooltip || !context || !dialog || !branch ||
        !make(app, tooltip, UI_TEXT_BLOCK) || !make(app, context, UI_BUTTON) ||
        !make(app, dialog, UI_TEXT_BLOCK) || !make(app, branch, UI_TREE_ITEM) ||
        ui_node_set_anchor(tooltip, anchor) != UI_OK ||
        ui_node_set_anchor(context, anchor) != UI_OK ||
        ui_app_set_root(app, overlay_root) != UI_OK || ui_app_poll(app, 0) != UI_OK) {
        return 36;
    }
    const LPARAM point = MAKELPARAM(MulDiv(30, dpi, 96), y);
    SendMessageW(window, WM_MOUSEMOVE, 0, point);
    if (!has_value(tooltip, 1)) {
        return 37;
    }
    SendMessageW(window, WM_MOUSELEAVE, 0, 0);
    if (!has_value(tooltip, 0)) {
        return 40;
    }
    SendMessageW(window, WM_RBUTTONUP, 0, point);
    if (!has_value(context, 1) || ui_app_poll(app, 0) != UI_OK) {
        return 41;
    }
    SetFocus(window);
    if (!PostMessageW(window, WM_KEYUP, VK_ESCAPE, 0) || ui_app_poll(app, 0) != UI_OK ||
        !has_value(context, 0) || ui_node_set_value(dialog, 1) != UI_OK ||
        ui_app_poll(app, 0) != UI_OK || !click(app, window, 30, 30) || !has_value(anchor, 0) ||
        !PostMessageW(window, WM_KEYUP, VK_ESCAPE, 0) || ui_app_poll(app, 0) != UI_OK ||
        !has_value(dialog, 0) || !click(app, window, 30, 100) || !has_value(branch, 1)) {
        return 38;
    }
    ui_node* spinner = make(app, NULL, UI_SPINNER);
    if (!spinner || ui_app_set_root(app, spinner) != UI_OK || ui_app_poll(app, 0) != UI_OK) {
        return 42;
    }
    if (ui_app_set_reduced_motion(app,0) != UI_OK || ui_app_advance_animation_clock(app,0) != UI_OK) return 43;
    (void)ui_app_poll(app,0);
    uint32_t before = 0, after = 0;
    if (ui_node_get_value(spinner, &before) != UI_OK) {
        return 43;
    }
    if (ui_app_advance_animation_clock(app,100) != UI_OK || ui_app_poll(app,0) != UI_OK) return 43;
    if (ui_node_get_value(spinner, &after) != UI_OK || after != (before + 1) % 8) {
        return 43;
    }
    ui_node* table = make(app, NULL, UI_DATA_GRID);
    ui_node* row = table ? make(app, table, UI_GRID) : NULL;
    ui_node* identifier = row ? make(app, row, UI_BADGE) : NULL;
    ui_node* cell = row ? make(app, row, UI_TEXT_BOX) : NULL;
    const ui_grid_track columns[] = {{UI_GRID_PIXELS,85},{UI_GRID_STAR,1}};
    if (!identifier || !cell || ui_grid_set_tracks(row,NULL,0,columns,2) != UI_OK ||
        ui_node_set_grid_cell(cell,0,1,1,1) != UI_OK || set_fixed_width(identifier, 85) != UI_OK ||
        ui_node_set_text(cell, "value") != UI_OK || ui_app_set_root(app, table) != UI_OK ||
        ui_app_poll(app, 0) != UI_OK || !click(app, window, 150, 30) ||
        !PostMessageW(window, WM_CHAR, 1, 0) || !PostMessageW(window, WM_CHAR, 'X', 0) ||
        ui_app_poll(app, 0) != UI_OK || !text_is(cell, "X")) {
        return 49;
    }
    return ui_app_request_close(app) == UI_OK && ui_app_poll(app, 0) == UI_EXITED ? 0 : 39;
}

static int near_color(COLORREF actual, COLORREF expected)
{
    return actual != CLR_INVALID && abs((int)GetRValue(actual) - GetRValue(expected)) <= 1 &&
        abs((int)GetGValue(actual) - GetGValue(expected)) <= 1 &&
        abs((int)GetBValue(actual) - GetBValue(expected)) <= 1;
}

static int verify_glass(ui_app* app)
{
    ui_node* toolbar = make(app, NULL, UI_TOOL_BAR);
    if (!toolbar || ui_app_set_root(app, toolbar) != UI_OK || ui_app_open_window(app) != UI_OK) {
        return 50;
    }
    HWND window = NULL;
    EnumThreadWindows(GetCurrentThreadId(), find_window, (LPARAM)&window);
    if (!window) {
        return 50;
    }
    const COLORREF backgrounds[] = {RGB(245, 247, 252), RGB(22, 29, 44)};
    RECT rect;
    if (!GetWindowRect(window, &rect)) {
        return 50;
    }
    const unsigned native_dpi = GetDpiForWindow(window);
    const unsigned test_dpis[] = {native_dpi, native_dpi == 144 ? 192 : 144};
    for (unsigned i = 0; i < 2; ++i) {
        const unsigned dpi = test_dpis[i];
        if (i) {
            SendMessageW(window, WM_DPICHANGED, MAKEWPARAM(dpi, dpi), (LPARAM)&rect);
        }
        const int edge = MulDiv(16, dpi, 96);
        const int center = MulDiv(60, dpi, 96);
        for (uint32_t dark = 0; dark < 2; ++dark) {
            if (ui_app_set_builtin_theme(app, UI_BUILTIN_LIQUID_GLASS, dark, 0) != UI_OK ||
                ui_app_poll(app, 0) != UI_OK) {
                return 51;
            }
            // Let activation/exposure settle before sampling a presented GPU frame.
            Sleep(100);
            const ui_result settled = ui_app_poll(app, 0);
            if (settled != UI_OK && settled != UI_TIMEOUT) {
                return 51;
            }
            RECT client;
            if (!GetClientRect(window, &client)) {
                return 51;
            }
            const int middle = client.bottom / 2;
            const COLORREF corner = client_pixel(window, edge, edge);
            const COLORREF highlight = client_pixel(window, center, edge + 2);
            const COLORREF tint = client_pixel(window, center, middle);
            const COLORREF left = client_pixel(window, edge + 1, middle);
            const COLORREF right = client_pixel(window, client.right - edge - 2, middle);
            const COLORREF lower = client_pixel(window, center, client.bottom - edge - 3);
            const COLORREF above = client_pixel(window, center, edge + MulDiv(8, dpi, 96));
            const COLORREF below = client_pixel(window, center, client.bottom - edge - MulDiv(8, dpi, 96));
            // Antialiased rims blend with the backdrop. Check the complete border,
            // specular highlight and vertical sheen instead of a flat palette color.
            if (!near_color(corner, backgrounds[dark]) || highlight == CLR_INVALID || tint == CLR_INVALID ||
                GetRValue(highlight) <= GetRValue(tint) || !near_color(left, right) || near_color(left, tint) ||
                near_color(lower, below) || GetRValue(above) <= GetRValue(below)) {
                fprintf(stderr, "Glass DPI %u dark %u: corner=%06lx edge=%06lx tint=%06lx\n",
                        dpi, dark, corner, highlight, tint);
                return 51;
            }
        }
    }
    return ui_app_request_close(app) == UI_OK && ui_app_poll(app, 0) == UI_EXITED ? 0 : 52;
}

static int verify_border(ui_app* app)
{
    const char* xml = "<Window xmlns='urn:sunfleck:v1'><Border Background='#123456' BorderBrush='#abcdef' BorderThickness='6,8,10,12' CornerRadius='16'/></Window>";
    ui_fleck_diagnostic diagnostic;
    if (ui_app_load_fleck(app, xml, strlen(xml), &diagnostic) != UI_OK || ui_app_open_window(app) != UI_OK) return 60;
    HWND window = NULL;
    EnumThreadWindows(GetCurrentThreadId(), find_window, (LPARAM)&window);
    RECT rect;
    if (!window || !GetWindowRect(window, &rect)) return 61;
    const unsigned dpis[] = {96,144,192};
    for (unsigned i = 0; i < 3; ++i) {
        unsigned dpi = dpis[i];
        SendMessageW(window, WM_DPICHANGED, MAKEWPARAM(dpi,dpi), (LPARAM)&rect);
        ui_result status = ui_app_poll(app,0);
        if (status != UI_OK && status != UI_TIMEOUT) return 62;
        Sleep(100);
        ui_app_poll(app,0);
        RECT client; GetClientRect(window,&client);
        int edge = MulDiv(16,dpi,96), x = client.right/2, y = client.bottom/2;
        if (!near_color(client_pixel(window,x,y),RGB(18,52,86)) ||
            !near_color(client_pixel(window,edge+MulDiv(2,dpi,96),y),RGB(171,205,239)) ||
            !near_color(client_pixel(window,x,edge+MulDiv(2,dpi,96)),RGB(171,205,239)) ||
            near_color(client_pixel(window,edge,edge),RGB(171,205,239))) {
            fprintf(stderr,"Border pixels DPI %u: center=%06lx left=%06lx top=%06lx corner=%06lx client=%ldx%ld\n",dpi,
                client_pixel(window,x,y),client_pixel(window,edge+MulDiv(2,dpi,96),y),
                client_pixel(window,x,edge+MulDiv(2,dpi,96)),client_pixel(window,edge,edge),client.right,client.bottom);
            return 63;
        }
    }
    char fractional[8192]="<Window xmlns='urn:sunfleck:v1'><StackPanel Spacing='0'>";
    for (unsigned i=0;i<100;++i) strcat(fractional,i%2 ? "<Border Height='1' Background='#00cc44'/>" : "<Border Height='1' Background='#cc2244'/>");
    strcat(fractional,"</StackPanel></Window>");
    if(ui_app_refresh_fleck(app,fractional,strlen(fractional),NULL,&diagnostic)!=UI_OK) return 65;
    SetWindowPos(window,NULL,0,0,800,600,SWP_NOMOVE|SWP_NOZORDER);
    if(!GetWindowRect(window,&rect)) return 65;
    for (unsigned i=0;i<3;++i) {
        unsigned dpi=dpis[i];
        SendMessageW(window,WM_DPICHANGED,MAKEWPARAM(dpi,dpi),(LPARAM)&rect);
        ui_app_poll(app,0); Sleep(100); ui_app_poll(app,0);
        RECT client; GetClientRect(window,&client);
        for (unsigned item=96;item<100;++item) {
            int first=(item*dpi+48)/96,last=((item+1)*dpi+48)/96;
            int y=MulDiv(16,dpi,96)+(first+last-1)/2;
            COLORREF expected=item%2 ? RGB(0,204,68) : RGB(204,34,68);
            if(!near_color(client_pixel(window,client.right/2,y),expected)) {
                fprintf(stderr,"Fractional stack boundary mismatch: DPI %u item %u\n",dpi,item);return 66;
            }
        }
    }
    return ui_app_request_close(app) == UI_OK && ui_app_poll(app,0) == UI_EXITED ? 0 : 64;
}

static int verify_fonts(ui_app* app)
{
    const char* xml = "<Window xmlns='urn:sunfleck:v1'><StackPanel FontFamily='Segoe UI' FontSize='30' FontWeight='Bold' Foreground='#cc224466' FlowDirection='RightToLeft'><TextBox Id='edit' Height='80' TextAlignment='Center' TextWrapping='NoWrap' TextTrimming='WordEllipsis' LineHeight='48' TextDecorations='Underline'/><TextBlock><Run Text='Inline ' Foreground='#ff2244cc'/><Span FontStyle='Italic'><Run Text='paragraph' FontSize='24'/></Span></TextBlock></StackPanel></Window>";
    ui_fleck_diagnostic diagnostic;
    if (ui_app_load_fleck(app,xml,strlen(xml),&diagnostic)!=UI_OK || ui_app_open_window(app)!=UI_OK) return 70;
    ui_node* editor = NULL;
    HWND window = NULL;
    EnumThreadWindows(GetCurrentThreadId(),find_window,(LPARAM)&window);
    RECT rect;
    if (!window || !GetWindowRect(window,&rect) || ui_app_find_fleck_node(app,"edit",&editor)!=UI_OK) return 71;
    const unsigned dpis[] = {96,144,192};
    const char* sequences[] = {"e\xcc\x81", "\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb", "\xf0\x9f\x87\xa8\xf0\x9f\x87\xb3"};
    for (unsigned i=0;i<3;++i) {
        const unsigned dpi=dpis[i];
        SendMessageW(window,WM_DPICHANGED,MAKEWPARAM(dpi,dpi),(LPARAM)&rect);
        if(ui_app_poll(app,0)!=UI_OK) return 72; // Establish hit geometry before input.
        SetFocus(window);
        PostMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(MulDiv(35,dpi,96),MulDiv(35,dpi,96)));
        if(ui_app_poll(app,0)!=UI_OK) return 72;
        for(unsigned j=0;j<3;++j) {
            if(ui_node_set_text(editor,sequences[j])!=UI_OK || ui_app_poll(app,0)!=UI_OK) return 73;
            PostMessageW(window,WM_KEYDOWN,VK_END,0);
            PostMessageW(window,WM_CHAR,8,0);
            if(ui_app_poll(app,0)!=UI_OK || !text_is(editor,"")) {
                char actual[64]={0}; uint32_t count=sizeof(actual); ui_node_get_text(editor,actual,&count);
                fprintf(stderr,"Font backspace DPI %u sequence %u: '%s' focus=%d\n",dpi,j,actual,GetFocus()==window);
                return 74;
            }
            if(ui_node_set_text(editor,sequences[j])!=UI_OK || ui_app_poll(app,0)!=UI_OK) return 75;
            PostMessageW(window,WM_KEYDOWN,VK_HOME,0);
            PostMessageW(window,WM_KEYDOWN,VK_DELETE,0);
            if(ui_app_poll(app,0)!=UI_OK || !text_is(editor,"")) return 76;
        }
        ui_binding_value size={0}; size.kind=UI_VALUE_DOUBLE; size.number=42;
        if(ui_node_set_property(editor,UI_PROPERTY_FONT_SIZE,&size)!=UI_OK || ui_node_set_text(editor,"Text")!=UI_OK || ui_app_poll(app,0)!=UI_OK) return 77;
        POINT caret;
        if(!GetCaretPos(&caret) || caret.x<0 || caret.y<0) return 78;
        if(ui_node_set_text(editor,"\xd7\x90\xd7\x91\xd7\x92")!=UI_OK || ui_app_poll(app,0)!=UI_OK) return 80;
        PostMessageW(window,WM_KEYDOWN,VK_HOME,0);
        if(ui_app_poll(app,0)!=UI_OK || !GetCaretPos(&caret)) return 81;
        const LONG start_x=caret.x;
        PostMessageW(window,WM_KEYDOWN,VK_LEFT,0);
        if(ui_app_poll(app,0)!=UI_OK || !GetCaretPos(&caret) || caret.x>=start_x) return 82;
        PostMessageW(window,WM_KEYDOWN,VK_RIGHT,0);
        if(ui_app_poll(app,0)!=UI_OK || !GetCaretPos(&caret) || caret.x!=start_x) return 83;
    }
    return ui_app_request_close(app)==UI_OK && ui_app_poll(app,0)==UI_EXITED ? 0 : 79;
}

int main(int argc, char** argv)
{
    const char* mode = argc > 1 ? argv[1] : "";
    const ui_app_options options = {sizeof(options), UI_API_VERSION,
        strstr(mode, "gpu") ? UI_GLES3 : strstr(mode, "d3d11") ? UI_D3D11 : UI_SOFTWARE, 0};
    ui_app* app = NULL;
    int status = 1;
    if (ui_app_create(&options, &app) == UI_OK) {
        if (strstr(mode, "fonts")) {
            status = verify_fonts(app);
        } else if (strstr(mode, "border")) {
            status = verify_border(app);
        } else if (strstr(mode, "glass")) {
            status = verify_glass(app);
        } else if (strstr(mode, "extensions")) {
            status = verify_extensions(app);
        } else if (strstr(mode, "advanced")) {
            status = verify_advanced(app);
        } else {
            status = verify(app);
        }
    }
    ui_app_destroy(app);
    if (status) {
        fprintf(stderr, "Component tree check failed: %d\n", status);
    }
    return status;
}
