#include "dock.h"
#include "fleck.h"
#include "utf8_text.h"
#include <cmath>
#include <cstring>

struct ui_draw_context {
    std::vector<FillRectCommand>* commands;
    std::vector<layout::Rect> clips;
    unsigned dpi;
    size_t clip_floor = 1;
    int text_rotation = 0;
};

namespace {
layout::Rect rect(ui_rect r) { return {r.x, r.y, r.x + r.width, r.y + r.height}; }
ui_rect public_rect(layout::Rect r) { return {r.left, r.top, r.right - r.left, r.bottom - r.top}; }
layout::Rect intersect(layout::Rect a, layout::Rect b)
{
    return {std::max(a.left, b.left), std::max(a.top, b.top), std::min(a.right, b.right), std::min(a.bottom, b.bottom)};
}
bool contains(layout::Rect r, int x, int y) { return x >= r.left && x < r.right && y >= r.top && y < r.bottom; }
bool valid(ui_rect r)
{
    return r.x >= -1000000 && r.y >= -1000000 && r.x <= 1000000 && r.y <= 1000000 &&
        r.width >= 0 && r.height >= 0 && r.width <= 1000000 && r.height <= 1000000;
}
void fill(ui_draw_context& draw, layout::Rect bounds, uint32_t rgb, unsigned alpha = 255)
{
    const auto clipped = intersect(bounds, draw.clips.back());
    if (clipped.right <= clipped.left || clipped.bottom <= clipped.top) { return; }
    auto color = theme_rgb(rgb); color[3] = static_cast<uint8_t>(alpha);
    draw.commands->push_back({clipped.left, clipped.top, clipped.right, clipped.bottom, color});
}
void rounded(ui_draw_context& draw, layout::Rect r, uint32_t rgb, unsigned alpha, unsigned radius)
{
    auto color = theme_rgb(rgb); color[3] = static_cast<uint8_t>(alpha);
    drawing::rounded(*draw.commands, r, draw.clips.back(), color, static_cast<int>(radius));
}

std::wstring wide(std::string_view utf8)
{
    const auto decoded = utf8_text::decode(utf8);
    if constexpr (sizeof(wchar_t) == 2) {
        // utf8_text::decode targets scalar wchar_t on Unix; decode UTF-8 explicitly for UTF-16.
        std::wstring result;
        for (size_t i = 0; i < utf8.size();) {
            unsigned first = static_cast<unsigned char>(utf8[i++]), scalar = first;
            if (first >= 0x80) {
                const unsigned tail = first < 0xe0 ? 1 : first < 0xf0 ? 2 : 3;
                scalar = first & (0x7f >> (tail + 1));
                for (unsigned j = 0; j < tail; ++j) { scalar = (scalar << 6) | (static_cast<unsigned char>(utf8[i++]) & 63); }
            }
            if (scalar > 0xffff) { scalar -= 0x10000; result += static_cast<wchar_t>(0xd800 + (scalar >> 10)); result += static_cast<wchar_t>(0xdc00 + (scalar & 1023)); }
            else { result += static_cast<wchar_t>(scalar); }
        }
        return result;
    }
    return decoded;
}
}

ui_result ui_draw_fill(ui_draw_context* draw, ui_rect bounds, uint32_t rgb, uint32_t opacity, uint32_t radius)
{
    if (!draw || !valid(bounds) || rgb > 0xffffff || opacity > 255 || radius > 4096) { return UI_ERROR_INVALID_ARGUMENT; }
    try { rounded(*draw, rect(bounds), rgb, opacity, radius); return UI_OK; } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_draw_stroke(ui_draw_context* draw, ui_rect bounds, uint32_t rgb, uint32_t width, uint32_t radius)
{
    if (!draw || !valid(bounds) || rgb > 0xffffff || width > 4096 || radius > 4096) { return UI_ERROR_INVALID_ARGUMENT; }
    if (!width || bounds.width < 2 || bounds.height < 2) { return UI_OK; }
    try {
        drawing::rounded(*draw->commands, rect(bounds), draw->clips.back(), theme_rgb(rgb),
            static_cast<int>(radius), static_cast<int>(width));
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_draw_text(ui_draw_context* draw, ui_rect bounds, const char* text, uint32_t rgb)
{
    if (!draw || !valid(bounds) || !text || rgb > 0xffffff || std::strlen(text) > 16384) { return UI_ERROR_INVALID_ARGUMENT; }
    try {
        const auto decoded = wide(text);
        if (draw->text_rotation) {
            append_rotated_text(*draw->commands, rect(bounds), draw->clips.back(), theme_rgb(rgb), draw->dpi, decoded.c_str(), draw->text_rotation > 0);
        } else { append_centered_text(*draw->commands, rect(bounds), draw->clips.back(), theme_rgb(rgb), draw->dpi, decoded.c_str()); }
        return UI_OK;
    }
    catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; } catch (...) { return UI_ERROR_INVALID_ARGUMENT; }
}
ui_result ui_draw_image(ui_draw_context* draw, ui_rect bounds, const uint8_t* rgba, uint32_t width, uint32_t height)
{
    if (!draw || !valid(bounds) || !rgba || !width || !height || width > 4096 || height > 4096 || !bounds.width || !bounds.height) { return UI_ERROR_INVALID_ARGUMENT; }
    try {
        const auto r = intersect(rect(bounds), draw->clips.back());
        for (int y = r.top; y < r.bottom; ++y) {
            for (int x = r.left; x < r.right; ++x) {
                const auto* pixel = rgba + ((static_cast<uint64_t>(y - bounds.y) * height / bounds.height) * width + static_cast<uint64_t>(x - bounds.x) * width / bounds.width) * 4;
                const uint32_t color = (pixel[0] << 16) | (pixel[1] << 8) | pixel[2];
                fill(*draw, {x, y, x + 1, y + 1}, color, pixel[3]);
            }
        }
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_draw_push_clip(ui_draw_context* draw, ui_rect bounds)
{
    if (!draw || !valid(bounds) || draw->clips.size() >= 64) { return UI_ERROR_INVALID_ARGUMENT; }
    try { draw->clips.push_back(intersect(draw->clips.back(), rect(bounds))); return UI_OK; } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_draw_pop_clip(ui_draw_context* draw)
{
    if (!draw || draw->clips.size() <= draw->clip_floor) { return UI_ERROR_INVALID_STATE; } draw->clips.pop_back(); return UI_OK;
}

ui_result ui_app_register_dock_skin(ui_app* app, const char* name, ui_dock_painter painter, void* context)
{
    if (const auto status = dock_app_check(app); status != UI_OK) { return status; }
    if (!name || !fleck::valid_binding_name(name)) { return UI_ERROR_INVALID_ARGUMENT; }
    try {
        auto& skins = dock_app_skins(app);
        if (painter) { skins[name] = {painter, context}; } else { skins.erase(name); }
        dock_app_dirty(app); return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_app_load_theme(ui_app* app, const char* text, size_t size, ui_fleck_diagnostic* diagnostic)
{
    if (diagnostic) { *diagnostic = {}; }
    if (const auto status = dock_app_check(app); status != UI_OK) { return status; }
    if (!text) { return UI_ERROR_INVALID_ARGUMENT; }
    ui_fleck_diagnostic local{}; auto& result = diagnostic ? *diagnostic : local;
    try {
        std::string_view input(text, size);
        if (input.starts_with(std::string_view("FLKTH\x01", 6))) { input.remove_prefix(6); }
        auto theme = docking::parse_theme(input, result);
        if (!theme.skin.empty() && !dock_app_skins(app).contains(theme.skin)) { fleck::report({}, 0, UI_FLECK_VALUE, "Theme references an unregistered skin", result); return UI_ERROR_INVALID_ARGUMENT; }
        dock_app_apply_theme(app, std::move(theme)); return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (const std::exception& error) { if (result.code == UI_FLECK_NONE) { fleck::report({}, 0, UI_FLECK_VALUE, error.what(), result); } return UI_ERROR_INVALID_ARGUMENT; }
}
ui_result ui_app_apply_theme(ui_app* app, const ui_theme_descriptor* theme, ui_fleck_diagnostic* diagnostic)
{
    if (diagnostic) { *diagnostic = {}; }
    if (const auto status = dock_app_check(app); status != UI_OK) { return status; }
    if (!theme || theme->struct_size < sizeof(*theme) || theme->version != 1 || theme->base < UI_BUILTIN_FLUENT_2 ||
        theme->base >= UI_BUILTIN_THEME_COUNT || theme->dark > 1 || theme->compact > 1 || theme->resources_size > 1024 * 1024 ||
        (!theme->resources && theme->resources_size) || (theme->skin && !fleck::valid_binding_name(theme->skin))) { return UI_ERROR_INVALID_ARGUMENT; }
    try {
        constexpr const char* bases[] = {"Fluent2", "Material3", "Radix", "LiquidGlass", "SunfleckLite"};
        std::string xml = std::string("<Theme Version=\"1\" Base=\"") + bases[theme->base] + "\" Dark=\"" +
            (theme->dark ? "true" : "false") + "\" Compact=\"" + (theme->compact ? "true" : "false") + '"';
        if (theme->skin) { xml += std::string(" Skin=\"") + theme->skin + '"'; }
        xml += '>';
        if (theme->resources_size) { xml.append(theme->resources, theme->resources_size); }
        xml += "</Theme>";
        return ui_app_load_theme(app, xml.data(), xml.size(), diagnostic);
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

namespace docking {
namespace {
std::vector<DropTarget> drop_targets(layout::Rect area, unsigned dpi)
{
    const int size = 32 * static_cast<int>(dpi) / 96, step = size + 4 * static_cast<int>(dpi) / 96;
    const int cx = (area.left + area.right) / 2, cy = (area.top + area.bottom) / 2;
    std::vector<DropTarget> result;
    for (const auto entry : {std::pair{UI_DOCK_CENTER, std::pair{0, 0}},
         {UI_DOCK_POSITION_LEFT, {-1, 0}}, {UI_DOCK_POSITION_RIGHT, {1, 0}},
         {UI_DOCK_POSITION_TOP, {0, -1}}, {UI_DOCK_POSITION_BOTTOM, {0, 1}}}) {
        const int x = cx + entry.second.first * step - size / 2, y = cy + entry.second.second * step - size / 2;
        result.push_back({{x, y, x + size, y + size}, entry.first});
    }
    return result;
}
struct MenuRow { unsigned action; const char* label; bool enabled; };
std::vector<MenuRow> menu_rows(ui_dock& dock, ui_dock_item& item)
{
    const auto state = dock.layout.placements.at(item.id).state;
    bool group_can_float = (item.capabilities & UI_DOCK_CAN_FLOAT) != 0;
    const auto group = group_of(dock.layout, item.id);
    if (group) {
        for (const auto& id : dock.layout.regions.at(group).tabs) { group_can_float &= (find(dock, id)->capabilities & UI_DOCK_CAN_FLOAT) != 0; }
    }
    return {{1, "Float", (item.capabilities & UI_DOCK_CAN_FLOAT) != 0},
            {5, "Float group", group_can_float},
            {2, "Dock", state == UI_DOCK_FLOATING},
            {3, state == UI_DOCK_AUTO_HIDDEN ? "Pin" : "Auto hide", item.kind == UI_DOCK_TOOL_KIND && (item.capabilities & UI_DOCK_CAN_AUTO_HIDE) != 0},
            {4, item.kind == UI_DOCK_TOOL_KIND ? "Hide" : "Close", (item.capabilities & UI_DOCK_CAN_CLOSE) != 0}};
}
struct Painter {
    ui_dock& dock;
    View& view;
    GalleryStyle style;
    ThemeShape shape;
    Theme& theme;
    ui_draw_context draw;
    std::function<void(ui_node&, layout::Rect, layout::Rect)> content;
    ui_dock_position indicator_direction = UI_DOCK_CENTER;
    const char* menu_label = nullptr;
    ui_dock_part chrome_hover = UI_DOCK_PART_COUNT, chrome_pressed = UI_DOCK_PART_COUNT;
    bool window_maximized = false;
    double scaled(double value) const { return value * view.dpi / 96.0; }
    int px(double value) const { return int(std::round(scaled(value))); }
    int metric(const char* key, unsigned fallback) const
    {
        const auto found = theme.metrics.find(key); return px(found == theme.metrics.end() ? fallback : found->second);
    }
    uint32_t color(const std::string& key, uint32_t fallback) const
    {
        if (key.empty()) { return fallback; }
        if (key.front() == '#' && key.size() == 7) { return static_cast<uint32_t>(std::stoul(key.substr(1), nullptr, 16)); }
        const auto custom = theme.colors.find(key);
        if (custom != theme.colors.end()) { return custom->second; }
        constexpr const char* keys[] = {"background", "panel", "card", "text", "primary", "on_primary", "selected", "control", "focus", "divider", "scrim"};
        for (unsigned i = 0; i < UI_THEME_COLOR_COUNT; ++i) { if (key == keys[i]) { return style.colors[i]; } }
        return fallback;
    }
    void hit(ui_dock_part part, layout::Rect bounds, unsigned group, const std::string& item)
    {
        bounds = intersect(bounds, draw.clips.back());
        if (bounds.right > bounds.left && bounds.bottom > bounds.top) { view.hits.push_back({part, bounds, group, item}); }
    }
    bool same(const std::optional<Hit>& hit, ui_dock_part part, const std::string& id) const { return hit && hit->part == part && hit->item == id; }
    static bool actions(const TemplateElement& node)
    {
        return node.kind == TemplateElement::Button || std::any_of(node.children.begin(), node.children.end(), [](const auto& child) { return actions(child); });
    }
    bool visible(const TemplateElement& node, const ui_dock_paint& state) const
    {
        if (node.kind != TemplateElement::Button) { return true; }
        if (node.role == "Menu") { return true; }
        const auto flag = node.role == "Close" ? UI_DOCK_CAN_CLOSE : node.role == "Pin" ? UI_DOCK_CAN_AUTO_HIDE : UI_DOCK_CAN_FLOAT;
        const auto* item = find(dock, state.item_id);
        return (state.capabilities & flag) && (node.role != "Pin" || (item && item->kind == UI_DOCK_TOOL_KIND));
    }
    void element(const TemplateElement& node, layout::Rect r, const ui_dock_paint& state, unsigned group)
    {
        if (!visible(node, state)) { return; }
        const auto resolve = [&](const std::string& key, uint32_t fallback) {
            return key == "{Background}" ? state.background : key == "{Foreground}" ? state.foreground : key == "{Border}" ? state.border : color(key, fallback);
        };
        const auto bg = resolve(node.background, state.background), fg = resolve(node.foreground, state.foreground);
        if (!node.background.empty() || node.kind == TemplateElement::Fill) { rounded(draw, r, bg, 255, px(node.radius)); }
        if (node.kind == TemplateElement::Border) { (void)ui_draw_stroke(&draw, public_rect(r), fg, px(node.thickness), px(node.radius)); }
        if (node.kind == TemplateElement::Text || node.kind == TemplateElement::Button) {
            const auto text = node.text == "{Title}" ? state.title : node.text == "{Id}" ? state.item_id : node.text == "{Icon}" ? state.icon : node.text.c_str();
            (void)ui_draw_text(&draw, public_rect(r), text, fg);
        }
        if (node.kind == TemplateElement::Button) {
            ui_dock_part part = node.role == "Menu" ? UI_DOCK_PART_MENU : node.role == "Close" ? UI_DOCK_PART_CLOSE : node.role == "Pin" ? UI_DOCK_PART_PIN : UI_DOCK_PART_FLOAT;
            const auto flag = part == UI_DOCK_PART_CLOSE ? UI_DOCK_CAN_CLOSE : part == UI_DOCK_PART_PIN ? UI_DOCK_CAN_AUTO_HIDE : UI_DOCK_CAN_FLOAT;
            auto* item = find(dock, state.item_id);
            if ((part == UI_DOCK_PART_MENU || (state.capabilities & flag)) && (part != UI_DOCK_PART_PIN || (item && item->kind == UI_DOCK_TOOL_KIND))) { hit(part, r, group, state.item_id); }
        } else if (node.role == "Drag") { hit(state.part, r, group, state.item_id); }
        const int padding = px(node.padding);
        r = {r.left + padding, r.top + padding, r.right - padding, r.bottom - padding};
        if (node.kind == TemplateElement::Stack && !node.children.empty()) {
            int fixed = 0, flexible = 0;
            for (const auto& child : node.children) {
                if (!visible(child, state)) { continue; }
                const auto size = node.horizontal ? child.width : child.height;
                if (size) { fixed += px(size); } else { ++flexible; }
            }
            const int extent = node.horizontal ? r.right - r.left : r.bottom - r.top;
            int position = node.horizontal ? r.left : r.top;
            int remaining = std::max(0, extent - fixed);
            for (const auto& child : node.children) {
                if (!visible(child, state)) { continue; }
                const auto preferred = node.horizontal ? child.width : child.height;
                const int size = preferred ? px(preferred) : flexible ? remaining / flexible : 0;
                if (!preferred && flexible) { remaining -= size; --flexible; }
                auto area = r;
                if (node.horizontal) { area.left = position; area.right = std::min(r.right, position + size); }
                else { area.top = position; area.bottom = std::min(r.bottom, position + size); }
                element(child, area, state, group); position += size;
            }
        } else { for (const auto& child : node.children) { element(child, r, state, group); } }
    }
    bool part(ui_dock_part part, layout::Rect r, ui_dock_item* item, unsigned group, bool active = false, bool interactive = true, bool disabled = false)
    {
        if (r.right <= r.left || r.bottom <= r.top) { return false; }
        // Every painter, including the built-in glyphs, stays inside its part.
        draw.clips.push_back(intersect(draw.clips.back(), r));
        struct PartClip {
            ui_draw_context& draw;
            int rotation;
            ~PartClip() { draw.clips.pop_back(); draw.text_rotation = rotation; }
        } clip{draw, draw.text_rotation};
        draw.text_rotation = 0;
        if (part == UI_DOCK_PART_AUTO_HIDE && item) {
            const auto edge = dock.layout.placements.at(item->id).edge;
            draw.text_rotation = edge == UI_DOCK_LEFT || edge == UI_DOCK_RIGHT ? 1 : 0;
        }
        const std::string id = item ? item->id : "";
        unsigned state = active ? UI_PART_ACTIVE : 0;
        if (disabled) { state |= UI_PART_DISABLED; }
        if (part == chrome_hover || (same(dock.hovered, part, id) && (part != UI_DOCK_PART_MENU_ITEM || dock.hovered->bounds.top == r.top))) { state |= UI_PART_HOVER; }
        if (part == chrome_pressed || (same(dock.pressed, part, id) && (part != UI_DOCK_PART_MENU_ITEM || dock.pressed->bounds.top == r.top))) { state |= UI_PART_PRESSED; }
        const auto bg_token = part == UI_DOCK_PART_PREVIEW || part == UI_DOCK_PART_INDICATOR ? UI_THEME_PRIMARY :
            part == UI_DOCK_PART_SPLITTER || part == UI_DOCK_PART_TITLE || part == UI_DOCK_PART_WINDOW_TITLE ? UI_THEME_PANEL :
            active ? UI_THEME_SELECTED : part == UI_DOCK_PART_TAB ? UI_THEME_CONTROL : UI_THEME_PANEL;
        uint32_t bg = style.colors[bg_token], fg = style.colors[UI_THEME_TEXT], border = style.colors[UI_THEME_DIVIDER];
        if (part == UI_DOCK_PART_INDICATOR) { bg = style.colors[UI_THEME_CARD]; fg = style.colors[UI_THEME_PRIMARY]; }
        constexpr const char* names[] = {"title", "tab", "close", "pin", "float", "splitter", "indicator", "preview", "auto_hide", "menu", "menu_item", "window_title", "window_minimize", "window_maximize", "window_close"};
        const std::string prefix = std::string("dock.") + names[part];
        bg = color(prefix + ".background", bg); fg = color(prefix + ".foreground", fg); border = color(prefix + ".border", border);
        if (state & (UI_PART_HOVER | UI_PART_PRESSED)) { bg = color(prefix + ".hover", style.colors[UI_THEME_SELECTED]); }
        if (active) { bg = color(prefix + ".active", bg); }
        if (disabled) { bg = color(prefix + ".disabled", style.colors[part == UI_DOCK_PART_MENU_ITEM ? UI_THEME_PANEL : UI_THEME_DIVIDER]); }
        if (part == UI_DOCK_PART_WINDOW_CLOSE && (state & (UI_PART_HOVER | UI_PART_PRESSED))) {
            bg = color(prefix + ".hover", 0xc42b1c); fg = color(prefix + ".hover_foreground", 0xffffff);
        }
        ui_dock_paint data{sizeof(data), 1, part, state, item ? item->capabilities : 0u, view.dpi, public_rect(r),
            menu_label ? menu_label : item ? item->title.c_str() : "", item ? item->id.c_str() : "", bg, fg, border, item ? item->icon.c_str() : ""};
        if (interactive && !disabled) { hit(part, r, group, id); }
        const auto command_start = draw.commands->size(), hit_start = view.hits.size();
        const auto skin = dock_app_skins(dock.owner).find(theme.skin);
        if (skin != dock_app_skins(dock.owner).end()) {
            const auto clip_depth = draw.clips.size();
            draw.clips.push_back(intersect(draw.clips.back(), r));
            draw.clip_floor = draw.clips.size();
            uint32_t handled = 0;
            dock.painting = true;
            try { handled = skin->second.painter(&draw, &data, skin->second.context); } catch (...) { }
            dock.painting = false; draw.clips.resize(clip_depth); draw.clip_floor = 1;
            if (handled) { return false; }
            draw.commands->resize(command_start); view.hits.resize(hit_start);
        }
        auto templ = theme.templates.find({part, 0});
        unsigned specificity = 0;
        for (auto it = theme.templates.begin(); it != theme.templates.end(); ++it) {
            if (it->first.first != part || !it->first.second || (state & it->first.second) != it->first.second) { continue; }
            const auto mask = it->first.second;
            unsigned bits = 0; for (auto value = mask; value; value >>= 1) { bits += value & 1; }
            if (bits >= specificity) { specificity = bits; templ = it; }
        }
        if (templ != theme.templates.end()) {
            draw.clips.push_back(intersect(draw.clips.back(), r));
            element(templ->second, r, data, group); draw.clips.pop_back(); return actions(templ->second);
        }
        if (part >= UI_DOCK_PART_WINDOW_TITLE) {
            fill(draw, r, bg);
            const int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2, h = px(5), line = px(1);
            if (part == UI_DOCK_PART_WINDOW_TITLE) {
                const int x = r.left + px(12), y = cy - px(6);
                (void)ui_draw_stroke(&draw, {x, y, px(13), px(12)}, style.colors[UI_THEME_PRIMARY], line, 0);
                fill(draw, {x + px(1), y + px(1), x + px(12), y + px(4)}, style.colors[UI_THEME_PRIMARY]);
                auto label = r; label.left += px(34);
                (void)ui_draw_text(&draw, public_rect(label), menu_label ? menu_label : "", fg);
            } else if (part == UI_DOCK_PART_WINDOW_MINIMIZE) {
                fill(draw, {cx - h, cy, cx + h, cy + line}, fg);
            } else if (part == UI_DOCK_PART_WINDOW_MAXIMIZE) {
                if (window_maximized) { (void)ui_draw_stroke(&draw, {cx - h + px(3), cy - h, 2 * h - px(3), 2 * h - px(3)}, fg, line, 0); }
                (void)ui_draw_stroke(&draw, {cx - h, cy - h + (window_maximized ? px(3) : 0), 2 * h - (window_maximized ? px(3) : 0), 2 * h - (window_maximized ? px(3) : 0)}, fg, line, 0);
            } else {
                for (int i = -h; i <= h; ++i) {
                    fill(draw, {cx + i, cy + i, cx + i + line, cy + i + line}, fg);
                    fill(draw, {cx + i, cy - i, cx + i + line, cy - i + line}, fg);
                }
            }
            return false;
        }
        if (part == UI_DOCK_PART_SPLITTER) {
            fill(draw, r, bg);
            auto line = r;
            if (r.bottom - r.top > r.right - r.left) { line.left = (r.left + r.right) / 2; line.right = line.left + px(1); }
            else { line.top = (r.top + r.bottom) / 2; line.bottom = line.top + px(1); }
            fill(draw, line, (state & (UI_PART_HOVER | UI_PART_PRESSED)) ? style.colors[UI_THEME_PRIMARY] : border); return false;
        }
        if (part == UI_DOCK_PART_PREVIEW) { rounded(draw, r, bg, 52, px(2)); (void)ui_draw_stroke(&draw, public_rect(r), bg, px(2), px(2)); return false; }
        if (part == UI_DOCK_PART_INDICATOR) {
            rounded(draw, r, active ? style.colors[UI_THEME_SELECTED] : bg, 255, px(2));
            (void)ui_draw_stroke(&draw, public_rect(r), active ? fg : border, px(1), px(2));
            auto icon = r; icon.left += px(7); icon.right -= px(7); icon.top += px(7); icon.bottom -= px(7);
            auto pane = icon;
            if (indicator_direction == UI_DOCK_POSITION_LEFT) { pane.right = (icon.left + icon.right) / 2; }
            if (indicator_direction == UI_DOCK_POSITION_RIGHT) { pane.left = (icon.left + icon.right) / 2; }
            if (indicator_direction == UI_DOCK_POSITION_TOP) { pane.bottom = (icon.top + icon.bottom) / 2; }
            if (indicator_direction == UI_DOCK_POSITION_BOTTOM) { pane.top = (icon.top + icon.bottom) / 2; }
            fill(draw, pane, fg, disabled ? 64 : 110);
            (void)ui_draw_stroke(&draw, public_rect(icon), disabled ? border : fg, px(1), 0);
            if (indicator_direction == UI_DOCK_CENTER) {
                fill(draw, {icon.left, icon.top, icon.right, icon.top + px(3)}, fg);
            } else {
                const bool vertical = indicator_direction == UI_DOCK_POSITION_LEFT || indicator_direction == UI_DOCK_POSITION_RIGHT;
                const int middle = vertical ? (icon.left + icon.right) / 2 : (icon.top + icon.bottom) / 2;
                for (int offset = vertical ? icon.top : icon.left; offset < (vertical ? icon.bottom : icon.right); offset += px(3)) {
                    fill(draw, vertical ? layout::Rect{middle, offset, middle + px(1), offset + px(1)} : layout::Rect{offset, middle, offset + px(1), middle + px(1)}, fg);
                }
            }
            return false;
        }
        const bool header_button = part == UI_DOCK_PART_CLOSE || part == UI_DOCK_PART_MENU || part == UI_DOCK_PART_PIN || part == UI_DOCK_PART_FLOAT;
        if (header_button) {
            // Resting buttons inherit the header/tab underneath; hover is inset so
            // rounded button corners cannot erase or break the header outline.
            if ((state & (UI_PART_HOVER | UI_PART_PRESSED)) || theme.colors.contains(prefix + ".background")) {
                const int margin = px(3);
                rounded(draw, {r.left + margin, r.top + margin, r.right - margin, r.bottom - margin}, bg, 255,
                    metric("dock.radius", static_cast<unsigned>(std::min(shape.input_radius, 6))));
            }
        } else if (part == UI_DOCK_PART_TAB) {
            const int radius = std::min({metric("dock.radius", static_cast<unsigned>(std::min(shape.input_radius, 8))),
                (r.right - r.left) / 2, (r.bottom - r.top) / 2});
            drawing::rounded(*draw.commands, r, draw.clips.back(), theme_rgb(bg), radius, 0, true);
            drawing::rounded(*draw.commands, r, draw.clips.back(), theme_rgb(border), radius, px(1), true);
        } else {
            rounded(draw, r, bg, 255, part == UI_DOCK_PART_TITLE ? 0 : metric("dock.radius", static_cast<unsigned>(std::min(shape.input_radius, 8))));
        }
        if (part == UI_DOCK_PART_TITLE) {
            fill(draw, {r.left, r.bottom - px(1), r.right, r.bottom}, border);
            return false;
        }
        if (part == UI_DOCK_PART_TAB) {
            if (active) { fill(draw, {r.left + px(1), r.bottom - px(2), r.right - px(1), r.bottom}, color("dock.tab.accent", style.colors[UI_THEME_PRIMARY])); }
        }
        if (part == UI_DOCK_PART_MENU) {
            const int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
            for (int row = 0; row < 4; ++row) { fill(draw, {cx - px(4 - row), cy + px(row - 1), cx + px(4 - row), cy + px(row)}, fg); }
        } else if (part == UI_DOCK_PART_MENU_ITEM && menu_label) {
            auto label = r; label.left += px(12); (void)ui_draw_text(&draw, public_rect(label), menu_label, disabled ? border : fg);
        } else if (part == UI_DOCK_PART_CLOSE || part == UI_DOCK_PART_PIN || part == UI_DOCK_PART_FLOAT) {
            const int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2, h = px(4);
            if (part == UI_DOCK_PART_CLOSE) {
                for (int i = -h; i <= h; ++i) { fill(draw, {cx + i, cy + i, cx + i + px(1), cy + i + px(1)}, fg); fill(draw, {cx + i, cy - i, cx + i + px(1), cy - i + px(1)}, fg); }
            } else {
                (void)ui_draw_stroke(&draw, {cx - h, cy - h, 2 * h, 2 * h}, fg, px(1), 0);
                if (part == UI_DOCK_PART_PIN) { fill(draw, {cx, cy + h, cx + px(1), cy + h + px(3)}, fg); }
                else { fill(draw, {cx - h - px(2), cy - h - px(2), cx + h - px(2), cy - h - px(1)}, fg); }
            }
        } else if (item) {
            auto label = r; label.left += px(7); label.right -= px(4);
            if (draw.text_rotation) {
                label = {r.left + px(2), r.top + px(7), r.right - px(2), r.bottom - px(7)};
            }
            if (part == UI_DOCK_PART_TAB && (item->capabilities & UI_DOCK_CAN_CLOSE) && r.right - r.left >= px(48)) {
                label.right -= std::min(r.bottom - r.top, px(24));
            }
            draw.clips.push_back(intersect(draw.clips.back(), label));
            if (!item->icon.empty() && (part == UI_DOCK_PART_TAB || part == UI_DOCK_PART_TITLE)) {
                auto icon = label; icon.right = std::min(icon.right, icon.left + px(20));
                (void)ui_draw_text(&draw, public_rect(icon), item->icon.c_str(), fg); label.left = icon.right;
            }
            (void)ui_draw_text(&draw, public_rect(label), item->title.c_str(), fg);
            draw.clips.pop_back();
        }
        return false;
    }
    std::pair<double, double> minimum(unsigned id)
    {
        const auto& node = dock.layout.regions.at(id);
        if (node.axis) {
            const auto a = minimum(node.first), b = minimum(node.second); const int divider = metric("dock.splitter_width", 6);
            return node.axis == 1 ? std::pair{a.first + b.first + divider, std::max(a.second, b.second)} :
                std::pair{std::max(a.first, b.first), a.second + b.second + divider};
        }
        double width = scaled(64), height = scaled(64);
        for (const auto& id : node.tabs) { if (auto* item = find(dock, id)) { width = std::max(width, scaled(item->min_width)); height = std::max(height, scaled(item->min_height)); } }
        return {width, height + std::max(metric("dock.title_height", style.control_height), metric("dock.tab_height", style.control_height))};
    }
    void group(const Region& node, layout::Rect r)
    {
        view.groups[node.id] = r;
        const auto first = draw.commands->size();
        const int radius = metric("dock.radius", static_cast<unsigned>(std::min(shape.input_radius, 8)));
        fill(draw, r, style.colors[UI_THEME_CARD]);
        auto* selected = find(dock, node.active);
        if (!selected) { return; }
        const int title = std::max(metric("dock.title_height", style.control_height), metric("dock.tab_height", style.control_height));
        layout::Rect header{r.left, r.top, r.right, std::min(r.bottom, r.top + title)};
        const bool custom_buttons = part(UI_DOCK_PART_TITLE, header, selected, node.id, true);
        int tab_right = header.right;
        if (!custom_buttons) {
            int right = header.right - px(2);
            const auto button = [&](ui_dock_part kind, unsigned capability) {
                if (kind != UI_DOCK_PART_MENU && !(selected->capabilities & capability)) { return; }
                const int extent = std::min(title, px(28));
                layout::Rect bounds{std::max(header.left, right - extent), header.top, right, header.bottom};
                part(kind, bounds, selected, node.id); right = bounds.left;
            };
            button(UI_DOCK_PART_MENU, UI_DOCK_DEFAULT_CAPABILITIES);
            tab_right = right;
        } else {
            for (const auto& hit : view.hits) {
                if (hit.group == node.id && (hit.part == UI_DOCK_PART_CLOSE || hit.part == UI_DOCK_PART_PIN || hit.part == UI_DOCK_PART_FLOAT || hit.part == UI_DOCK_PART_MENU)) { tab_right = std::min(tab_right, hit.bounds.left); }
            }
        }
        int left = r.left;
        const int available = std::max(0, tab_right - r.left);
        const int tab_extent = std::min(available, static_cast<int>(node.tabs.size()) * px(160));
        for (size_t i = 0; i < node.tabs.size(); ++i) {
            auto* item = find(dock, node.tabs[i]);
            const int right = r.left + static_cast<int>((static_cast<int64_t>(tab_extent) * (i + 1)) / node.tabs.size());
            layout::Rect tab{left, header.top, right, header.bottom};
            if (item) {
                const bool tab_buttons = part(UI_DOCK_PART_TAB, tab, item, node.id, item == selected);
                if (!custom_buttons && !tab_buttons && (item->capabilities & UI_DOCK_CAN_CLOSE) && right - left >= px(48)) {
                    const int extent = std::min(title, px(24));
                    part(UI_DOCK_PART_CLOSE, {right - extent - px(2), header.top, right - px(2), header.bottom}, item, node.id, item == selected);
                }
            }
            left = right;
        }
        const layout::Rect body{r.left, header.bottom, r.right, r.bottom};
        content(*selected->content, body, intersect(body, draw.clips.back()));
        (void)ui_draw_stroke(&draw, public_rect(r), color("dock.group.border", style.colors[UI_THEME_DIVIDER]), px(1), radius);
        drawing::clip_rounded(*draw.commands, first, r, radius);
    }
    void tree(unsigned id, layout::Rect r)
    {
        const auto& node = dock.layout.regions.at(id);
        if (!node.axis) { group(node, r); return; }
        view.splits[id] = r;
        const int gap = metric("dock.splitter_width", 6), extent = std::max(0, (node.axis == 1 ? r.right - r.left : r.bottom - r.top) - gap);
        const auto a = minimum(node.first), b = minimum(node.second);
        const double min_a = node.axis == 1 ? a.first : a.second, min_b = node.axis == 1 ? b.first : b.second;
        const int lower=int(std::ceil(min_a)), upper=extent-int(std::ceil(min_b));
        const int split = lower <= upper ? std::clamp(static_cast<int>(static_cast<int64_t>(extent) * node.ratio / 10000), lower, upper) :
            (min_a + min_b ? int(std::round(extent * min_a / (min_a + min_b))) : extent / 2);
        auto first = r, second = r, divider = r;
        if (node.axis == 1) { first.right = r.left + split; divider.left = first.right; divider.right = std::min(r.right, divider.left + gap); second.left = divider.right; }
        else { first.bottom = r.top + split; divider.top = first.bottom; divider.bottom = std::min(r.bottom, divider.top + gap); second.top = divider.bottom; }
        tree(node.first, first); tree(node.second, second); part(UI_DOCK_PART_SPLITTER, divider, nullptr, id);
    }
};
}

WindowFrame window_frame(const Theme& theme, int width, unsigned dpi)
{
    const auto px = [dpi](int value) { return static_cast<int>((value * dpi + 48) / 96); };
    const auto metric = theme.metrics.find("dock.window_title_height");
    const int height = px(metric == theme.metrics.end() ? std::max(32u, theme.palette.control_height) : std::max(28u, metric->second));
    const int button = std::min(px(44), std::max(1, (width - 2) / 4));
    const int right = std::max(1, width - 1);
    return {{1, 1, right - 3 * button, height}, {right - 3 * button, 1, right - 2 * button, height},
        {right - 2 * button, 1, right - button, height}, {right - button, 1, right, height}, height, px(6)};
}
void paint_window_frame(ui_dock& dock, int width, int height, unsigned dpi, const char* title,
                        bool active, bool maximized, ui_dock_part hovered, ui_dock_part pressed,
                        std::vector<FillRectCommand>& commands)
{
    const auto& theme = dock_app_theme(dock.owner);
    const auto frame = window_frame(theme, width, dpi);
    View view; view.dpi = dpi; view.area = {0, 0, width, height};
    Painter painter{dock, view, theme.palette, theme.shape, dock_app_theme(dock.owner), {&commands, {view.area}, dpi}, {}};
    painter.menu_label = title; painter.chrome_hover = hovered; painter.chrome_pressed = pressed; painter.window_maximized = maximized;
    painter.part(UI_DOCK_PART_WINDOW_TITLE, frame.title, nullptr, 0, active, false);
    painter.part(UI_DOCK_PART_WINDOW_MINIMIZE, frame.minimize, nullptr, 0, false, false);
    painter.part(UI_DOCK_PART_WINDOW_MAXIMIZE, frame.maximize, nullptr, 0, false, false);
    painter.part(UI_DOCK_PART_WINDOW_CLOSE, frame.close, nullptr, 0, false, false);
    const auto border = painter.color("dock.window_title.border", theme.palette.colors[active ? UI_THEME_PRIMARY : UI_THEME_DIVIDER]);
    (void)ui_draw_stroke(&painter.draw, {0, 0, width, height}, border, painter.px(1), maximized ? 0 : painter.px(8));
    fill(painter.draw, {1, frame.height - painter.px(1), width - 1, frame.height}, theme.palette.colors[UI_THEME_DIVIDER]);
}

void paint(ui_dock& dock, unsigned window, layout::Rect area, layout::Rect clip, GalleryStyle style,
           ThemeShape shape, unsigned dpi, std::vector<FillRectCommand>& commands,
           const std::function<void(ui_node&, layout::Rect, layout::Rect)>& content)
{
    auto& view = dock.views[window]; view = {}; view.area = area; view.dpi = dpi;
    Painter painter{dock, view, style, shape, dock_app_theme(dock.owner), {&commands, {clip}, dpi}, content};
    // Clear only content that belongs to this surface; another window keeps its geometry.
    for (const auto& item : dock.items) {
        const auto group = group_of(dock.layout, item->id);
        if (window_of(dock.layout, group) != window) { continue; }
        const auto clear = [&](const auto& self, ui_node& node) -> void { node.visible = {}; for (auto* child : node.children) { self(self, *child); } };
        clear(clear, *item->content);
    }
    unsigned root = dock.layout.root;
    if (window) {
        const auto found = std::find_if(dock.layout.floating.begin(), dock.layout.floating.end(), [&](const auto& w) { return w.id == window; });
        if (found == dock.layout.floating.end()) { return; } root = found->root;
    } else {
        const int rail = painter.metric("dock.rail_width", 28);
        for (int edge = UI_DOCK_LEFT; edge <= UI_DOCK_BOTTOM; ++edge) {
            std::vector<ui_dock_item*> items;
            for (const auto& item : dock.items) { const auto& p = dock.layout.placements.at(item->id); if (p.state == UI_DOCK_AUTO_HIDDEN && p.edge == edge) { items.push_back(item.get()); } }
            if (items.empty()) { continue; }
            const auto side = static_cast<ui_dock_edge>(edge);
            layout::Rect strip = area;
            if (side == UI_DOCK_LEFT) { strip.right = std::min(area.right, area.left + rail); area.left = strip.right; }
            if (side == UI_DOCK_RIGHT) { strip.left = std::max(area.left, area.right - rail); area.right = strip.left; }
            if (side == UI_DOCK_TOP) { strip.bottom = std::min(area.bottom, area.top + rail); area.top = strip.bottom; }
            if (side == UI_DOCK_BOTTOM) { strip.top = std::max(area.top, area.bottom - rail); area.bottom = strip.top; }
            const bool vertical = side == UI_DOCK_LEFT || side == UI_DOCK_RIGHT;
            const int extent = std::max(0, vertical ? strip.bottom - strip.top : strip.right - strip.left);
            const int gap = std::min(painter.px(4), extent / static_cast<int>(items.size()));
            const int available = std::max(0, extent - gap * static_cast<int>(items.size() - 1));
            std::vector<int> lengths;
            for (const auto* item : items) {
                lengths.push_back(std::max(rail, text_width(wide(item->title).c_str(), dpi) + painter.px(16)));
            }
            // Cap long labels first, preserving full short labels whenever space allows.
            int low = 0, high = available;
            while (low < high) {
                const int cap = low + (high - low + 1) / 2;
                int64_t total = 0; for (const int length : lengths) { total += std::min(length, cap); }
                if (total <= available) { low = cap; } else { high = cap - 1; }
            }
            int position = vertical ? strip.top : strip.left;
            for (size_t i = 0; i < items.size(); ++i) {
                auto tab = strip;
                const int length = std::min(lengths[i], low);
                if (vertical) { tab.top = position; tab.bottom = position + length; }
                else { tab.left = position; tab.right = position + length; }
                painter.part(UI_DOCK_PART_AUTO_HIDE, tab, items[i], 0, items[i]->id == dock.layout.expanded);
                position += length + gap;
            }
        }
    }
    if (dock.layout.regions.contains(root)) { painter.tree(root, area); }
    if (!window && !dock.layout.expanded.empty()) {
        auto* item = find(dock, dock.layout.expanded);
        if (item && dock.layout.placements.at(item->id).state == UI_DOCK_AUTO_HIDDEN) {
            const auto edge = dock.layout.placements.at(item->id).edge;
            auto popup = area;
            const int width = std::min(area.right - area.left, std::max(int(std::ceil(painter.scaled(item->min_width))), painter.metric("dock.auto_hide_width", 320)));
            const int height = std::min(area.bottom - area.top, std::max(int(std::ceil(painter.scaled(item->min_height + 72))), painter.metric("dock.auto_hide_height", 240)));
            if (edge == UI_DOCK_LEFT) { popup.right = popup.left + width; }
            if (edge == UI_DOCK_RIGHT) { popup.left = popup.right - width; }
            if (edge == UI_DOCK_TOP) { popup.bottom = popup.top + height; }
            if (edge == UI_DOCK_BOTTOM) { popup.top = popup.bottom - height; }
            view.popup = popup;
            Region temporary; temporary.kind = item->kind; temporary.tabs = {item->id}; temporary.active = item->id;
            painter.group(temporary, popup);
        }
    }
    if (!dock.external_overlay) { paint_drag_overlay(dock, window, commands); }
    if (dock.menu && dock.menu->window == window) {
        auto* item = find(dock, dock.menu->item);
        if (item) {
            const auto rows = menu_rows(dock, *item);
            const int width = painter.px(180), row_height = painter.px(30);
            const int left = std::max(view.area.left, std::min(dock.menu->anchor.right - width, view.area.right - width));
            const int top = std::max(view.area.top, std::min(dock.menu->anchor.bottom, view.area.bottom - static_cast<int>(rows.size()) * row_height));
            view.menu = {left, top, left + width, top + static_cast<int>(rows.size()) * row_height};
            fill(painter.draw, view.menu, style.colors[UI_THEME_PANEL]);
            for (size_t i = 0; i < rows.size(); ++i) {
                const layout::Rect bounds{left, top + static_cast<int>(i) * row_height, left + width, top + static_cast<int>(i + 1) * row_height};
                painter.menu_label = rows[i].label;
                painter.part(UI_DOCK_PART_MENU_ITEM, bounds, item, dock.menu->group, false, false, !rows[i].enabled);
                if (rows[i].enabled) { painter.hit(UI_DOCK_PART_MENU_ITEM, bounds, dock.menu->group, item->id); view.hits.back().action = rows[i].action; }
            }
            (void)ui_draw_stroke(&painter.draw, public_rect(view.menu), style.colors[UI_THEME_DIVIDER], painter.px(1), 0);
        }
    }
}

void paint_drag_overlay(ui_dock& dock, unsigned window, std::vector<FillRectCommand>& commands)
{
    const auto found = dock.views.find(window);
    if (found == dock.views.end()) { return; }
    auto& view = found->second; view.targets.clear();
    if (!dock.drag.active || dock.drag.target_window != window || dock.drag.target_area.right <= dock.drag.target_area.left) { return; }
    auto& theme = dock_app_theme(dock.owner);
    Painter painter{dock, view, theme.palette, theme.shape, theme, {&commands, {view.area}, view.dpi}, {}};
    if (dock.drag.preview.right > dock.drag.preview.left) { painter.part(UI_DOCK_PART_PREVIEW, dock.drag.preview, nullptr, 0, false, false, !dock.drag.allowed); }
    view.targets = drop_targets(dock.drag.target_area, view.dpi);
    const int gap = painter.px(4);
    const auto a = view.targets[1].bounds, b = view.targets[2].bounds, c = view.targets[3].bounds, d = view.targets[4].bounds;
    const auto plate = [&](layout::Rect r) {
        rounded(painter.draw, r, theme.palette.colors[UI_THEME_CARD], 245, painter.px(3));
        (void)ui_draw_stroke(&painter.draw, public_rect(r), theme.palette.colors[UI_THEME_DIVIDER], painter.px(1), painter.px(3));
    };
    plate({a.left - gap, a.top - gap, b.right + gap, b.bottom + gap});
    plate({c.left - gap, c.top - gap, d.right + gap, d.bottom + gap});
    const auto* item = find(dock, dock.drag.source.item);
    const auto target = dock.layout.regions.find(dock.drag.group);
    for (const auto& indicator : view.targets) {
        painter.indicator_direction = indicator.position;
        const bool disabled = indicator.position == UI_DOCK_CENTER && item && target != dock.layout.regions.end() && !target->second.tabs.empty() && target->second.kind != item->kind;
        painter.part(UI_DOCK_PART_INDICATOR, indicator.bounds, nullptr, 0,
            indicator.position == dock.drag.position && dock.drag.allowed, false, disabled);
    }
}

bool consumes(const ui_dock& dock, unsigned window, int x, int y)
{
    const auto view = dock.views.find(window);
    return view != dock.views.end() && ((dock.menu && dock.menu->window == window) || std::any_of(view->second.hits.rbegin(), view->second.hits.rend(), [&](const auto& hit) { return contains(hit.bounds, x, y); }));
}
void cancel(ui_dock& dock)
{
    if (dock.drag.prepared && dock.drag.source.part == UI_DOCK_PART_SPLITTER && dock.layout.regions.contains(dock.drag.source.group)) {
        dock.layout.regions.at(dock.drag.source.group).ratio = dock.drag.original_ratio;
    }
    if (dock.drag.prepared && !dock.drag.original_active.empty() && dock.layout.regions.contains(dock.drag.source.group)) {
        auto& group = dock.layout.regions.at(dock.drag.source.group);
        if (std::find(group.tabs.begin(), group.tabs.end(), dock.drag.original_active) != group.tabs.end()) { group.active = dock.drag.original_active; }
    }
    dock.drag = {}; dock.pressed.reset(); dock.menu.reset(); dock_app_dirty(dock.owner);
}
bool begin_window_drag(ui_dock& dock, unsigned window, int x, int y)
{
    const auto floating = std::find_if(dock.layout.floating.begin(), dock.layout.floating.end(), [&](const auto& entry) { return entry.id == window; });
    if (floating == dock.layout.floating.end()) { return false; }
    unsigned group = floating->root;
    while (dock.layout.regions.at(group).axis) { group = dock.layout.regions.at(group).first; }
    const auto& node = dock.layout.regions.at(group);
    if (node.tabs.empty()) { return false; }
    cancel(dock);
    dock.drag.prepared = true; dock.drag.window_move = true; dock.drag.whole_group = true;
    dock.drag.window = window; dock.drag.start_x = x; dock.drag.start_y = y;
    dock.drag.source = {UI_DOCK_PART_TITLE, {}, group, node.active};
    dock.drag.original_active = node.active;
    return true;
}
bool pointer(ui_dock& dock, unsigned window, ui_pointer_action action, int x, int y)
{
    const auto view_it = dock.views.find(window);
    if (view_it == dock.views.end()) { return false; }
    auto& view = view_it->second;
    const auto hit_it = std::find_if(view.hits.rbegin(), view.hits.rend(), [&](const auto& hit) { return contains(hit.bounds, x, y); });
    std::optional<Hit> hit = hit_it == view.hits.rend() ? std::nullopt : std::optional<Hit>(*hit_it);
    if (dock.menu && dock.menu->window == window && contains(view.menu, x, y) && hit && hit->part != UI_DOCK_PART_MENU_ITEM) { hit.reset(); }
    if (action == UI_POINTER_CANCEL) { cancel(dock); return true; }
    if (action == UI_POINTER_DOWN) {
        const auto menu = dock.menu;
        cancel(dock);
        if (hit && (hit->part == UI_DOCK_PART_MENU_ITEM || hit->part == UI_DOCK_PART_MENU)) { dock.menu = menu; }
        dock.pressed = hit; dock.hovered = hit;
        if (!hit) { if (!contains(view.popup, x, y)) { dock.layout.expanded.clear(); } return false; }
        if (hit->part == UI_DOCK_PART_TITLE || hit->part == UI_DOCK_PART_TAB || hit->part == UI_DOCK_PART_SPLITTER) {
            dock.drag.prepared = true; dock.drag.window = window; dock.drag.source = *hit;
            dock.drag.start_x = x; dock.drag.start_y = y; dock.drag.whole_group = hit->part == UI_DOCK_PART_TITLE;
            if (hit->part == UI_DOCK_PART_SPLITTER) { dock.drag.original_ratio = dock.layout.regions.at(hit->group).ratio; }
            else if (auto* item = find(dock, hit->item)) {
                if (dock.layout.regions.contains(hit->group)) { dock.drag.original_active = dock.layout.regions.at(hit->group).active; }
                (void)ui_dock_activate(item);
            }
        }
        dock_app_dirty(dock.owner); return true;
    }
    if (action == UI_POINTER_MOVE) {
        if (dock.drag.prepared) {
            auto& drag = dock.drag;
            if (drag.source.part == UI_DOCK_PART_SPLITTER) {
                auto& split = dock.layout.regions.at(drag.source.group); const auto r = view.splits.at(split.id);
                const int offset = split.axis == 1 ? x - r.left : y - r.top;
                const auto& metrics = dock_app_theme(dock.owner).metrics;
                const auto metric = metrics.find("dock.splitter_width");
                const int divider = (metric == metrics.end() ? 6 : metric->second) * static_cast<int>(view.dpi) / 96;
                const int extent = std::max(1, (split.axis == 1 ? r.right - r.left : r.bottom - r.top) - divider);
                split.ratio = std::clamp(static_cast<int>(static_cast<int64_t>(offset) * 10000 / extent), 1, 9999);
                drag.active = true; dock_app_dirty(dock.owner); return true;
            }
            if (!drag.active && std::abs(x - drag.start_x) + std::abs(y - drag.start_y) < static_cast<int>(6 * view.dpi / 96)) { return true; }
            drag.active = true; drag.x = x; drag.y = y; drag.preview = {}; drag.target_area = {}; drag.target.clear(); drag.target_window = UINT32_MAX; drag.target_index = UINT32_MAX;
            drag.allowed = true;
            unsigned target_window = window; int tx = x, ty = y;
            if (dock_app_locate(dock.owner, window, x, y, target_window, tx, ty)) {
                auto& target_view = dock.views.at(target_window);
                for (const auto& [group, bounds] : target_view.groups) {
                    if (!group || !contains(bounds, tx, ty)) { continue; }
                    const auto& node = dock.layout.regions.at(group);
                    drag.group = group; drag.target_window = target_window; drag.preview = bounds; drag.target_area = bounds; drag.position = UI_DOCK_CENTER;
                    const int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
                    const auto indicators = drop_targets(bounds, target_view.dpi);
                    const auto indicator = std::find_if(indicators.begin(), indicators.end(), [&](const auto& entry) { return contains(entry.bounds, tx, ty); });
                    const auto tab_hit = std::find_if(target_view.hits.rbegin(), target_view.hits.rend(), [&](const auto& h) {
                        return h.group == group && h.part == UI_DOCK_PART_TAB && contains(h.bounds, tx, ty);
                    });
                    if (indicator != indicators.end()) { drag.position = indicator->position; }
                    else if (tab_hit != target_view.hits.rend()) {
                        drag.target = tab_hit->item;
                        drag.target_index = static_cast<unsigned>(std::find(node.tabs.begin(), node.tabs.end(), drag.target) - node.tabs.begin());
                    }
                    else if (tx < bounds.left + width / 4) { drag.position = UI_DOCK_POSITION_LEFT; }
                    else if (tx >= bounds.right - width / 4) { drag.position = UI_DOCK_POSITION_RIGHT; }
                    else if (ty < bounds.top + height / 4) { drag.position = UI_DOCK_POSITION_TOP; }
                    else if (ty >= bounds.bottom - height / 4) { drag.position = UI_DOCK_POSITION_BOTTOM; }
                    if (drag.position == UI_DOCK_POSITION_LEFT) { drag.preview.right = bounds.left + width / 2; }
                    if (drag.position == UI_DOCK_POSITION_RIGHT) { drag.preview.left = bounds.left + width / 2; }
                    if (drag.position == UI_DOCK_POSITION_TOP) { drag.preview.bottom = bounds.top + height / 2; }
                    if (drag.position == UI_DOCK_POSITION_BOTTOM) { drag.preview.top = bounds.top + height / 2; }
                    if (drag.target.empty() && !node.tabs.empty()) {
                        drag.target = node.tabs.front();
                        for (const auto& id : node.tabs) { if (id != drag.source.item) { drag.target = id; break; } }
                    }
                    auto* item = find(dock, drag.source.item);
                    if (item) {
                        if (!node.tabs.empty() && node.kind != item->kind && drag.position == UI_DOCK_CENTER) { drag.allowed = false; }
                        if (target_window) {
                            const auto source_group = group_of(dock.layout, item->id);
                            const auto ids = drag.whole_group && source_group ? dock.layout.regions.at(source_group).tabs : std::vector<std::string>{item->id};
                            for (const auto& id : ids) { if (!(find(dock, id)->capabilities & UI_DOCK_CAN_FLOAT)) { drag.allowed = false; } }
                        }
                        if (!drag.whole_group && drag.target == item->id && drag.position != UI_DOCK_CENTER) { drag.allowed = false; }
                    }
                    break;
                }
            }
            dock_app_dirty(dock.owner); return true;
        }
        const bool different = dock.hovered.has_value() != hit.has_value() || (hit && dock.hovered && (hit->part != dock.hovered->part || hit->item != dock.hovered->item || hit->action != dock.hovered->action));
        dock.hovered = hit;
        if (hit && hit->part == UI_DOCK_PART_AUTO_HIDE && dock.layout.expanded != hit->item) { dock.layout.expanded = hit->item; dock_app_dirty(dock.owner); }
        else if (!window && !dock.layout.expanded.empty() && !contains(view.popup, x, y) && (!hit || hit->part != UI_DOCK_PART_AUTO_HIDE)) { dock.layout.expanded.clear(); dock_app_dirty(dock.owner); }
        if (different) { dock_app_dirty(dock.owner); }
        return hit.has_value();
    }
    if (action == UI_POINTER_UP) {
        if (dock.drag.active && !dock.drag.allowed) { cancel(dock); return true; }
        const auto drag = dock.drag; const auto pressed = dock.pressed;
        dock.drag = {}; dock.pressed.reset();
        if (drag.active && drag.source.part == UI_DOCK_PART_SPLITTER) { changed(dock); return true; }
        if (drag.active) {
            auto* item = find(dock, drag.source.item);
            if (!item) { return true; }
            if (drag.target_window != UINT32_MAX && drag.preview.right > drag.preview.left) {
                auto* target = find(dock, drag.target);
                const auto source_group = group_of(dock.layout, item->id);
                if (drag.whole_group && source_group == drag.group) { dock_app_dirty(dock.owner); return true; }
                if (drag.whole_group) { (void)ui_dock_move_group(item, target, drag.position); }
                else { (void)ui_dock_move(item, target, drag.position, drag.target_index); }
            } else if (!drag.window_move) {
                const auto bounds = dock_app_screen(dock.owner, window, x, y); (void)ui_dock_float(item, &bounds, drag.whole_group);
            }
            dock_app_dirty(dock.owner); return true;
        }
        if (pressed && hit && pressed->part == hit->part && pressed->item == hit->item && pressed->action == hit->action) {
            if (auto* item = find(dock, hit->item)) {
                switch (hit->part) {
                case UI_DOCK_PART_MENU:
                    if (dock.menu) { dock.menu.reset(); }
                    else { dock.menu = ui_dock::Menu{window, hit->group, item->id, hit->bounds}; }
                    break;
                case UI_DOCK_PART_MENU_ITEM: {
                    dock.menu.reset();
                    if (hit->action == 1 || hit->action == 5) {
                        const auto bounds = dock_app_screen(dock.owner, window, x, y); (void)ui_dock_float(item, &bounds, hit->action == 5);
                    } else if (hit->action == 2) {
                        ui_dock_item* target = nullptr;
                        for (const auto& candidate : dock.items) {
                            const auto group = group_of(dock.layout, candidate->id);
                            if (group && !window_of(dock.layout, group) && candidate->kind == item->kind) { target = candidate.get(); break; }
                        }
                        (void)ui_dock_move_group(item, target, UI_DOCK_CENTER);
                    } else if (hit->action == 3) {
                        if (dock.layout.placements.at(item->id).state == UI_DOCK_AUTO_HIDDEN) { (void)ui_dock_pin(item); }
                        else { (void)ui_dock_auto_hide(item, UI_DOCK_LEFT); }
                    } else if (hit->action == 4) { (void)ui_dock_close(item); }
                    break;
                }
                case UI_DOCK_PART_CLOSE: (void)ui_dock_close(item); break;
                case UI_DOCK_PART_PIN:
                    if (dock.layout.placements.at(item->id).state == UI_DOCK_AUTO_HIDDEN) { (void)ui_dock_pin(item); }
                    else { (void)ui_dock_auto_hide(item, UI_DOCK_LEFT); } break;
                case UI_DOCK_PART_FLOAT: { const auto bounds = dock_app_screen(dock.owner, window, x, y); (void)ui_dock_float(item, &bounds, 0); break; }
                default: (void)ui_dock_activate(item); break;
                }
            }
        }
        dock_app_dirty(dock.owner); return pressed.has_value() || drag.prepared;
    }
    return false;
}
}
