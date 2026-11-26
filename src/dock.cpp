#include "dock.h"
#include "dock_json.h"
#include "fleck.h"
#include <cstring>
#include <limits>

namespace docking {
ui_result check(ui_dock* dock, bool mutation)
{
    if (!dock) { return UI_ERROR_INVALID_ARGUMENT; }
    const auto status = dock_app_check_workspace(dock);
    if (status != UI_OK) { return status; }
    return (dock->painting || (mutation && dock->notifying)) ? UI_ERROR_INVALID_STATE : UI_OK;
}
ui_result check(ui_dock_item* item, bool mutation)
{
    return item ? check(item->dock, mutation) : UI_ERROR_INVALID_ARGUMENT;
}
ui_dock_item* find(ui_dock& dock, const std::string& id)
{
    for (const auto& item : dock.items) { if (item->id == id) { return item.get(); } }
    return nullptr;
}
unsigned group_of(const Layout& layout, const std::string& id)
{
    for (const auto& [key, region] : layout.regions) {
        if (!region.axis && std::find(region.tabs.begin(), region.tabs.end(), id) != region.tabs.end()) { return key; }
    }
    return 0;
}
unsigned window_of(const Layout& layout, unsigned group)
{
    const auto has = [&](const auto& self, unsigned root) -> bool {
        if (root == group) { return true; }
        const auto it = layout.regions.find(root);
        return it != layout.regions.end() && it->second.axis &&
            (self(self, it->second.first) || self(self, it->second.second));
    };
    for (const auto& floating : layout.floating) { if (has(has, floating.root)) { return floating.id; } }
    return 0;
}
void remove(Layout& layout, const std::string& id)
{
    const auto group = group_of(layout, id);
    if (!group) { return; }
    auto& region = layout.regions.at(group);
    const auto tab = std::find(region.tabs.begin(), region.tabs.end(), id);
    auto& placement = layout.placements.at(id);
    placement.group = group;
    placement.index = static_cast<unsigned>(tab - region.tabs.begin());
    placement.path.clear();
    placement.window = window_of(layout, group);
    for (const auto& window : layout.floating) {
        if (window.id == placement.window) { placement.bounds = window.bounds; }
    }
    unsigned child = group;
    for (unsigned depth = 0; depth < 64; ++depth) {
        const auto parent = std::find_if(layout.regions.begin(), layout.regions.end(), [&](const auto& pair) {
            return pair.second.axis && (pair.second.first == child || pair.second.second == child);
        });
        if (parent == layout.regions.end()) { break; }
        const auto& branch = parent->second;
        const bool before = branch.first == child;
        const auto sibling = before ? branch.second : branch.first;
        unsigned leaf = sibling;
        while (layout.regions.at(leaf).axis) { leaf = layout.regions.at(leaf).first; }
        const auto& tabs = layout.regions.at(leaf).tabs;
        placement.path.push_back({sibling, tabs.empty() ? "" : tabs.front(),
            branch.axis == 1 ? (before ? UI_DOCK_POSITION_LEFT : UI_DOCK_POSITION_RIGHT) :
                               (before ? UI_DOCK_POSITION_TOP : UI_DOCK_POSITION_BOTTOM), branch.ratio});
        child = branch.id;
    }
    region.tabs.erase(tab);
    if (region.active == id) { region.active = region.tabs.empty() ? "" : region.tabs[std::min<size_t>(placement.index, region.tabs.size() - 1)]; }
    if (layout.expanded == id) { layout.expanded.clear(); }
}
void normalize(Layout& layout)
{
    const auto clean = [&](const auto& self, unsigned id) -> unsigned {
        auto it = layout.regions.find(id);
        if (it == layout.regions.end()) { return 0; }
        auto& node = it->second;
        if (!node.axis) {
            if (node.tabs.empty()) { layout.regions.erase(it); return 0; }
            return id;
        }
        const auto first = self(self, node.first), second = self(self, node.second);
        if (!first || !second) { layout.regions.erase(id); return first ? first : second; }
        node.first = first; node.second = second;
        return id;
    };
    // A workspace with only floating/hidden items still owns an empty main root.
    // Keep its identity when normalizing an already normalized layout.
    const auto root = layout.regions.find(layout.root);
    if (root == layout.regions.end() || root->second.axis || !root->second.tabs.empty()) {
        layout.root = clean(clean, layout.root);
    }
    if (!layout.root) {
        layout.root = layout.next++;
        layout.regions.emplace(layout.root, Region{layout.root});
    }
    for (auto& window : layout.floating) { window.root = clean(clean, window.root); }
    std::erase_if(layout.floating, [](const Floating& window) { return !window.root; });
    for (const auto& [id, group] : layout.regions) {
        if (group.axis) { continue; }
        for (size_t i = 0; i < group.tabs.size(); ++i) {
            auto& p = layout.placements.at(group.tabs[i]);
            p.group = id; p.index = static_cast<unsigned>(i);
            p.state = window_of(layout, id) ? UI_DOCK_FLOATING : UI_DOCK_DOCKED;
        }
    }
}
static unsigned make_group(Layout& layout, ui_dock_kind kind)
{
    const auto id = layout.next++;
    Region region{id}; region.kind = kind;
    layout.regions.emplace(id, std::move(region));
    return id;
}
static void replace(Layout& layout, unsigned old, unsigned replacement)
{
    if (layout.root == old) { layout.root = replacement; return; }
    for (auto& floating : layout.floating) { if (floating.root == old) { floating.root = replacement; return; } }
    for (auto& [key, node] : layout.regions) {
        (void)key;
        if (node.axis && node.first == old) { node.first = replacement; return; }
        if (node.axis && node.second == old) { node.second = replacement; return; }
    }
}
void place(Layout& layout, const std::string& id, ui_dock_kind kind, const std::string& target,
           ui_dock_position position, unsigned index)
{
    unsigned target_group = group_of(layout, target);
    if (target == id && position != UI_DOCK_CENTER) { throw std::range_error("Cannot split an item against itself"); }
    if (!target.empty() && !target_group) { throw std::range_error("Dock target is not visible"); }
    const auto previous = layout.placements.at(id);
    remove(layout, id);
    if (!target_group && position == UI_DOCK_CENTER && layout.regions.contains(previous.group) &&
        !window_of(layout, previous.group) && !layout.regions.at(previous.group).axis && layout.regions.at(previous.group).kind == kind) { target_group = previous.group; }
    if (!target_group) {
        for (const auto& [key, node] : layout.regions) {
            if (!node.axis && (node.kind == kind || node.tabs.empty()) && !window_of(layout, key)) { target_group = key; break; }
        }
    }
    if (!target_group) { target_group = layout.root; }
    if (position == UI_DOCK_CENTER && layout.regions.at(target_group).kind != kind && !layout.regions.at(target_group).tabs.empty()) {
        if (!target.empty()) { throw std::range_error("Document and tool tabs cannot share a group"); }
        position = kind == UI_DOCK_TOOL_KIND ? UI_DOCK_POSITION_LEFT : UI_DOCK_POSITION_RIGHT;
    }
    if (position != UI_DOCK_CENTER || layout.regions.at(target_group).axis) {
        if (position == UI_DOCK_CENTER) { position = UI_DOCK_POSITION_RIGHT; }
        const auto fresh = make_group(layout, kind);
        const auto split = layout.next++;
        const bool before = position == UI_DOCK_POSITION_LEFT || position == UI_DOCK_POSITION_TOP;
        Region branch{split}; branch.axis = position == UI_DOCK_POSITION_TOP || position == UI_DOCK_POSITION_BOTTOM ? 2 : 1;
        branch.first = before ? fresh : target_group; branch.second = before ? target_group : fresh;
        replace(layout, target_group, split);
        layout.regions.emplace(split, std::move(branch));
        target_group = fresh;
    }
    auto& group = layout.regions.at(target_group);
    group.kind = kind;
    const auto offset = std::min<size_t>(index, group.tabs.size());
    group.tabs.insert(group.tabs.begin() + offset, id);
    group.active = id;
    auto& placement = layout.placements.at(id);
    placement.group = target_group; placement.index = static_cast<unsigned>(offset);
    placement.state = window_of(layout, target_group) ? UI_DOCK_FLOATING : UI_DOCK_DOCKED;
    normalize(layout);
}
void return_to(Layout& layout, const ui_dock_item& item)
{
    const auto saved = layout.placements.at(item.id);
    if (saved.resume == UI_DOCK_AUTO_HIDDEN && layout.placements.at(item.id).state == UI_DOCK_HIDDEN) {
        layout.placements.at(item.id).state = UI_DOCK_AUTO_HIDDEN;
        return;
    }
    const auto group = layout.regions.find(saved.group);
    if (group != layout.regions.end() && !group->second.axis && group->second.kind == item.kind) {
        if (window_of(layout, saved.group) && !(item.capabilities & UI_DOCK_CAN_FLOAT)) { throw std::range_error("Dock item cannot float"); }
        const auto target = group->second.tabs.empty() ? "" : group->second.tabs.front();
        place(layout, item.id, item.kind, target, UI_DOCK_CENTER, saved.index);
        return;
    }
    unsigned target = 0;
    const Placement::ReturnSplit* split = nullptr;
    for (const auto& step : saved.path) {
        const unsigned sibling = layout.regions.contains(step.sibling) ? step.sibling : group_of(layout, step.anchor);
        if (sibling && window_of(layout, sibling) == saved.window) { target = sibling; split = &step; break; }
    }
    const unsigned fresh = !layout.regions.contains(saved.group) ? saved.group : layout.next++;
    layout.next = std::max(layout.next, fresh + 1);
    Region node{fresh}; node.kind = item.kind; node.tabs = {item.id}; node.active = item.id;
    layout.regions.emplace(fresh, std::move(node));
    if (target && split) {
        Region branch{layout.next++}; branch.axis = split->position == UI_DOCK_POSITION_TOP || split->position == UI_DOCK_POSITION_BOTTOM ? 2 : 1;
        const bool before = split->position == UI_DOCK_POSITION_LEFT || split->position == UI_DOCK_POSITION_TOP;
        branch.first = before ? fresh : target; branch.second = before ? target : fresh; branch.ratio = split->ratio;
        replace(layout, target, branch.id); layout.regions.emplace(branch.id, std::move(branch));
    } else if (saved.window) {
        if (!(item.capabilities & UI_DOCK_CAN_FLOAT)) { throw std::range_error("Dock item cannot float"); }
        const auto window = std::find_if(layout.floating.begin(), layout.floating.end(), [&](const auto& w) { return w.id == saved.window; });
        if (window == layout.floating.end()) {
            layout.floating.push_back({saved.window, fresh, saved.bounds}); layout.next_window = std::max(layout.next_window, saved.window + 1);
        } else {
            Region branch{layout.next++}; branch.axis = 1; branch.first = window->root; branch.second = fresh;
            window->root = branch.id; layout.regions.emplace(branch.id, std::move(branch));
        }
    } else if (!layout.regions.at(layout.root).axis && layout.regions.at(layout.root).tabs.empty()) {
        layout.regions.erase(layout.root); layout.root = fresh;
    } else {
        Region branch{layout.next++}; branch.axis = 1; branch.first = fresh; branch.second = layout.root;
        layout.root = branch.id; layout.regions.emplace(branch.id, std::move(branch));
    }
    layout.expanded.clear(); normalize(layout);
}
void changed(ui_dock& dock, ui_dock_item* item, ui_dock_event_kind event)
{
    dock_app_dirty(dock.owner);
    if (dock.callback && !dock.notifying) {
        (void)dock_app_callback(dock, item, event);
    }
}

std::string save(const Layout& layout)
{
    const auto tree = [&](const auto& self, unsigned id) -> std::string {
        const auto& node = layout.regions.at(id);
        std::string out = "{\"id\":" + std::to_string(id) + ",\"axis\":" + std::to_string(node.axis);
        if (node.axis) {
            out += ",\"ratio\":" + std::to_string(node.ratio) + ",\"first\":" + self(self, node.first) + ",\"second\":" + self(self, node.second);
        } else {
            out += ",\"kind\":" + std::to_string(node.kind) + ",\"active\":" + json::quote(node.active) + ",\"tabs\":[";
            for (size_t i = 0; i < node.tabs.size(); ++i) { if (i) { out += ','; } out += json::quote(node.tabs[i]); }
            out += ']';
        }
        return out + '}';
    };
    std::string out = "{\"version\":1,\"root\":" + tree(tree, layout.root) + ",\"floating\":[";
    for (size_t i = 0; i < layout.floating.size(); ++i) {
        if (i) { out += ','; }
        const auto& w = layout.floating[i];
        out += "{\"id\":" + std::to_string(w.id) + ",\"x\":" + std::to_string(w.bounds.x) + ",\"y\":" + std::to_string(w.bounds.y) +
            ",\"width\":" + std::to_string(w.bounds.width) + ",\"height\":" + std::to_string(w.bounds.height) + ",\"root\":" + tree(tree, w.root) + '}';
    }
    out += "],\"items\":[";
    bool comma = false;
    for (const auto& [id, placement] : layout.placements) {
        if (comma) { out += ','; } comma = true;
        out += "{\"id\":" + json::quote(id) + ",\"state\":" + std::to_string(placement.state) + ",\"group\":" + std::to_string(placement.group) +
            ",\"index\":" + std::to_string(placement.index) + ",\"edge\":" + std::to_string(placement.edge) +
            ",\"returnWindow\":" + std::to_string(placement.window) + ",\"resume\":" + std::to_string(placement.resume) +
            ",\"returnBounds\":[" + std::to_string(placement.bounds.x) + ',' + std::to_string(placement.bounds.y) + ',' +
            std::to_string(placement.bounds.width) + ',' + std::to_string(placement.bounds.height) + "],\"returnPath\":[";
        for (size_t i = 0; i < placement.path.size(); ++i) {
            if (i) { out += ','; }
            const auto& step = placement.path[i];
            out += "{\"sibling\":" + std::to_string(step.sibling) + ",\"anchor\":" + json::quote(step.anchor) +
                ",\"position\":" + std::to_string(step.position) + ",\"ratio\":" + std::to_string(step.ratio) + '}';
        }
        out += "]}";
    }
    return out + "]}";
}
static void validate_limits(const Layout& layout)
{
    if (layout.regions.size() > 1024 || layout.floating.size() > 128 || layout.next > 1000001 || layout.next_window > 1000001) { throw std::range_error("Dock layout limit exceeded"); }
    const auto tree = [&](const auto& self, unsigned id, unsigned depth) -> void {
        if (depth > 64) { throw std::range_error("Dock tree limit exceeded"); }
        const auto& region = layout.regions.at(id);
        if (region.axis) { self(self, region.first, depth + 1); self(self, region.second, depth + 1); }
    };
    tree(tree, layout.root, 0);
    for (const auto& window : layout.floating) { tree(tree, window.root, 0); }
}
Layout restore(std::string_view input, const ui_dock& dock)
{
    const auto document = json::Reader(input).parse();
    if (document.at("version").number(1, 1) != 1) { throw std::range_error("Unsupported Dock layout version"); }
    Layout layout; layout.regions.clear(); layout.placements.clear();
    std::set<std::string> tabs;
    const auto tree = [&](const auto& self, const json::Value& value, unsigned depth) -> unsigned {
        if (depth > 64 || layout.regions.size() >= 1024) { throw std::range_error("Dock tree limit exceeded"); }
        const auto id = static_cast<unsigned>(value.at("id").number(1, 1000000));
        if (layout.regions.contains(id)) { throw std::range_error("Duplicate Dock region"); }
        Region node{id}; node.axis = value.at("axis").number(0, 2);
        layout.regions.emplace(id, node);
        layout.next = std::max(layout.next, id + 1);
        if (node.axis) {
            node.ratio = value.at("ratio").number(1, 9999);
            node.first = self(self, value.at("first"), depth + 1); node.second = self(self, value.at("second"), depth + 1);
        } else {
            node.kind = static_cast<ui_dock_kind>(value.at("kind").number(0, 1));
            node.active = value.at("active").string();
            for (const auto& tab : value.at("tabs").array()) {
                const auto& name = tab.string();
                if (name.empty() || name.size() > 128 || name.find('\0') != std::string::npos || !tabs.insert(name).second) { throw std::range_error("Duplicate or invalid Dock item"); }
                node.tabs.push_back(name);
            }
            if (!node.active.empty() && std::find(node.tabs.begin(), node.tabs.end(), node.active) == node.tabs.end()) { throw std::range_error("Invalid active Dock tab"); }
            if (!node.tabs.empty() && node.active.empty()) { throw std::range_error("Missing active Dock tab"); }
        }
        layout.regions.at(id) = std::move(node);
        return id;
    };
    layout.root = tree(tree, document.at("root"), 0);
    std::set<unsigned> windows;
    for (const auto& entry : document.at("floating").array()) {
        const auto id = static_cast<unsigned>(entry.at("id").number(1, 1000000));
        if (!windows.insert(id).second || windows.size() > 128) { throw std::range_error("Duplicate or excessive floating windows"); }
        Floating window{id, tree(tree, entry.at("root"), 0), {entry.at("x").number(-100000, 100000), entry.at("y").number(-100000, 100000),
            entry.at("width").number(64, 8192), entry.at("height").number(64, 8192)}};
        layout.next_window = std::max(layout.next_window, id + 1); layout.floating.push_back(window);
    }
    for (const auto& entry : document.at("items").array()) {
        const auto& id = entry.at("id").string();
        if (id.empty() || id.size() > 128 || id.find('\0') != std::string::npos || layout.placements.size() >= 512) { throw std::range_error("Invalid Dock item ID"); }
        Placement p{static_cast<ui_dock_state>(entry.at("state").number(0, 4)),
            static_cast<unsigned>(entry.at("group").number(1, 1000000)), static_cast<unsigned>(entry.at("index").number(0, 511)),
            static_cast<ui_dock_edge>(entry.at("edge").number(0, 3))};
        if (entry.has("returnWindow")) {
            p.window = entry.at("returnWindow").number(0, 1000000);
            p.resume = static_cast<ui_dock_state>(entry.at("resume").number(0, 4));
            const auto& b = entry.at("returnBounds").array();
            if (b.size() != 4) { throw std::range_error("Invalid Dock return bounds"); }
            p.bounds = {b[0].number(-100000, 100000), b[1].number(-100000, 100000), b[2].number(64, 8192), b[3].number(64, 8192)};
            for (const auto& step : entry.at("returnPath").array()) {
                if (p.path.size() >= 64 || step.at("anchor").string().size() > 128) { throw std::range_error("Invalid Dock return path"); }
                p.path.push_back({static_cast<unsigned>(step.at("sibling").number(1, 1000000)), step.at("anchor").string(),
                    static_cast<ui_dock_position>(step.at("position").number(1, 4)), static_cast<unsigned>(step.at("ratio").number(1, 9999))});
            }
            layout.next = std::max(layout.next, p.group + 1); layout.next_window = std::max(layout.next_window, p.window + 1);
        }
        if (!layout.placements.emplace(id, p).second) { throw std::range_error("Duplicate Dock item placement"); }
        const auto group = group_of(layout, id);
        if ((p.state == UI_DOCK_DOCKED || p.state == UI_DOCK_FLOATING) != (group != 0) || (group && (p.group != group ||
            (p.state == UI_DOCK_FLOATING) != (window_of(layout, group) != 0)))) { throw std::range_error("Inconsistent Dock placement"); }
        if (group && (p.index >= layout.regions.at(group).tabs.size() || layout.regions.at(group).tabs[p.index] != id)) { throw std::range_error("Inconsistent Dock tab index"); }
    }
    for (const auto& id : tabs) { if (!layout.placements.contains(id)) { throw std::range_error("Unregistered Dock tab"); } }
    for (const auto& [id, p] : layout.placements) {
        auto* item = find(const_cast<ui_dock&>(dock), id);
        if (!item) { continue; }
        if ((p.state == UI_DOCK_FLOATING && !(item->capabilities & UI_DOCK_CAN_FLOAT)) ||
            (p.state == UI_DOCK_AUTO_HIDDEN && (item->kind != UI_DOCK_TOOL_KIND || !(item->capabilities & UI_DOCK_CAN_AUTO_HIDE))) ||
            (p.state == UI_DOCK_CLOSED && !(item->capabilities & UI_DOCK_CAN_CLOSE))) { throw std::range_error("Dock capabilities disallow restored state"); }
        const auto group = group_of(layout, id);
        if (group && layout.regions.at(group).kind != item->kind) { throw std::range_error("Incompatible Dock tab kind"); }
    }
    for (auto& [id, node] : layout.regions) {
        (void)id;
        if (!node.axis) {
            std::erase_if(node.tabs, [&](const std::string& name) { return !find(const_cast<ui_dock&>(dock), name); });
            if (std::find(node.tabs.begin(), node.tabs.end(), node.active) == node.tabs.end()) { node.active = node.tabs.empty() ? "" : node.tabs.front(); }
        }
    }
    std::erase_if(layout.placements, [&](const auto& pair) { return !find(const_cast<ui_dock&>(dock), pair.first); });
    normalize(layout);
    for (const auto& item : dock.items) {
        if (!layout.placements.contains(item->id)) {
            const auto initial = dock.defaults.find(item->id);
            layout.placements[item->id] = initial == dock.defaults.end() ? Placement{} : initial->second;
            if (layout.placements.at(item->id).state == UI_DOCK_DOCKED || layout.placements.at(item->id).state == UI_DOCK_FLOATING) {
                auto& p = layout.placements.at(item->id);
                unsigned existing = 0;
                if (dock.initial) {
                    const auto group = dock.initial->regions.find(p.group);
                    if (group != dock.initial->regions.end() && !group->second.axis) {
                        for (const auto& peer : group->second.tabs) {
                            existing = group_of(layout, peer);
                            if (existing) { break; }
                        }
                    }
                }
                p.group = existing ? existing : layout.next++;
                for (auto& step : p.path) { step.sibling = group_of(layout, step.anchor); }
                std::erase_if(p.path, [](const auto& step) { return !step.sibling; });
                p.state = UI_DOCK_HIDDEN; p.resume = UI_DOCK_DOCKED;
                return_to(layout, *item);
            }
        }
    }
    validate_limits(layout); return layout;
}

static bool valid_text(const char* value, size_t maximum, bool nonempty = false)
{
    if (!value) { return false; }
    const std::string_view text(value);
    if (text.size() > maximum || (nonempty && text.empty()) || text.find('\0') != std::string_view::npos) { return false; }
    (void)utf8_text::decode(text);
    return true;
}
template<class F> static ui_result transaction(ui_dock* dock, F operation)
{
    if (const auto status = check(dock, true); status != UI_OK) { return status; }
    try {
        auto next = dock->layout;
        operation(next);
        validate_limits(next);
        dock->layout = std::move(next);
        changed(*dock);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (const std::exception&) { return UI_ERROR_INVALID_ARGUMENT; }
}
}

ui_result ui_dock_create(ui_node* host, ui_dock** out_dock)
{
    if (!out_dock) { return UI_ERROR_INVALID_ARGUMENT; } *out_dock = nullptr;
    if (!host || host->kind != UI_DOCK_HOST) { return UI_ERROR_INVALID_ARGUMENT; }
    if (host->workspace) {
        if (const auto status = docking::check(host->workspace); status != UI_OK) { return status; }
        *out_dock = host->workspace;
        return UI_OK;
    }
    if (const auto status = dock_app_check(host->owner); status != UI_OK) { return status; }
    ui_window* current = nullptr;
    if (ui_app_get_current_window(host->owner, &current) != UI_OK || current != host->window_owner) { return UI_ERROR_INVALID_STATE; }
    if (auto* dock = dock_app_workspace(host->owner)) {
        if (dock->host != host) { return UI_ERROR_INVALID_STATE; }
        *out_dock = dock; return UI_OK;
    }
    try {
        auto dock = std::make_unique<ui_dock>(); dock->owner = host->owner; dock->host = host;
        *out_dock = dock.get(); host->workspace = dock.get(); dock_app_set_workspace(host->owner, std::move(dock));
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_dock_register(ui_dock* dock, const ui_dock_item_options* options, ui_dock_item** out_item)
{
    if (!out_item) { return UI_ERROR_INVALID_ARGUMENT; } *out_item = nullptr;
    if (const auto status = docking::check(dock, true); status != UI_OK) { return status; }
    if (!options || options->struct_size < sizeof(*options) || options->version != 1 || !options->content ||
        options->content->owner != dock->owner || options->content->window_owner != dock->host->window_owner || options->kind < UI_DOCK_DOCUMENT_KIND || options->kind > UI_DOCK_TOOL_KIND ||
        options->capabilities > 7 || options->min_width > 4096 || options->min_height > 4096 || dock->items.size() >= 512) { return UI_ERROR_INVALID_ARGUMENT; }
    try {
        if (!docking::valid_text(options->id, 128, true) || !docking::valid_text(options->title, 4096)) { return UI_ERROR_INVALID_ARGUMENT; }
        if (docking::find(*dock, options->id)) { return UI_ERROR_INVALID_ARGUMENT; }
        for (auto* ancestor = dock->host; ancestor; ancestor = ancestor->parent) { if (ancestor == options->content) { return UI_ERROR_INVALID_ARGUMENT; } }
        if (options->content->parent || dock_app_root(dock) == options->content ||
            std::any_of(dock->items.begin(), dock->items.end(), [&](const auto& item) { return item->content == options->content; })) { return UI_ERROR_INVALID_STATE; }
        auto item = std::make_unique<ui_dock_item>(ui_dock_item{dock, options->id, options->title, options->content,
            options->kind, options->capabilities, double(std::max(32u, options->min_width)), double(std::max(32u, options->min_height))});
        auto layout = dock->layout;
        layout.placements.emplace(item->id, docking::Placement{});
        docking::place(layout, item->id, item->kind, "", UI_DOCK_CENTER, UINT32_MAX);
        dock->items.reserve(dock->items.size() + 1);
        auto defaults = dock->defaults; defaults[item->id] = layout.placements.at(item->id);
        dock->host->children.reserve(dock->host->children.size() + 1);
        item->content->parent = dock->host; dock->host->children.push_back(item->content);
        dock->layout = std::move(layout); dock->defaults = std::move(defaults);
        *out_item = item.get(); dock->items.push_back(std::move(item)); docking::changed(*dock, *out_item); return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (const std::exception&) { return UI_ERROR_INVALID_ARGUMENT; }
}
ui_result ui_dock_find(ui_dock* dock, const char* id, ui_dock_item** out_item)
{
    if (!out_item) { return UI_ERROR_INVALID_ARGUMENT; } *out_item = nullptr;
    if (const auto status = docking::check(dock); status != UI_OK) { return status; }
    if (!id) { return UI_ERROR_INVALID_ARGUMENT; }
    *out_item = docking::find(*dock, id); return *out_item ? UI_OK : UI_ERROR_INVALID_ARGUMENT;
}
ui_result ui_dock_set_event(ui_dock* dock, ui_dock_event callback, void* context)
{
    if (const auto status = docking::check(dock, true); status != UI_OK) { return status; }
    dock->callback = callback; dock->callback_context = context; return UI_OK;
}
ui_result ui_dock_move(ui_dock_item* item, ui_dock_item* target, ui_dock_position position, uint32_t index)
{
    if (const auto status = docking::check(item, true); status != UI_OK) { return status; }
    if ((target && target->dock != item->dock) || position < UI_DOCK_CENTER || position > UI_DOCK_POSITION_BOTTOM) { return UI_ERROR_INVALID_ARGUMENT; }
    const auto& current = item->dock->layout;
    const auto destination = target ? docking::group_of(current, target->id) : current.placements.at(item->id).group;
    if (docking::window_of(current, destination) && !(item->capabilities & UI_DOCK_CAN_FLOAT)) { return UI_ERROR_INVALID_STATE; }
    return docking::transaction(item->dock, [&](auto& layout) { docking::place(layout, item->id, item->kind, target ? target->id : "", position, index); });
}
ui_result ui_dock_move_group(ui_dock_item* item, ui_dock_item* target, ui_dock_position position)
{
    if (const auto status = docking::check(item, true); status != UI_OK) { return status; }
    if ((target && target->dock != item->dock) || position < UI_DOCK_CENTER || position > UI_DOCK_POSITION_BOTTOM) { return UI_ERROR_INVALID_ARGUMENT; }
    const auto& current = item->dock->layout;
    const auto source = docking::group_of(current, item->id);
    if (!source) { return UI_ERROR_INVALID_STATE; }
    if (target && docking::group_of(current, target->id) == source) { return position == UI_DOCK_CENTER ? UI_OK : UI_ERROR_INVALID_ARGUMENT; }
    const auto destination = target ? docking::group_of(current, target->id) : 0;
    const auto ids = current.regions.at(source).tabs;
    if (docking::window_of(current, destination)) {
        for (const auto& id : ids) { if (!(docking::find(*item->dock, id)->capabilities & UI_DOCK_CAN_FLOAT)) { return UI_ERROR_INVALID_STATE; } }
    }
    return docking::transaction(item->dock, [&](auto& layout) {
        std::string anchor = target ? target->id : "";
        for (size_t i = 0; i < ids.size(); ++i) {
            docking::place(layout, ids[i], item->kind, anchor, i ? UI_DOCK_CENTER : position, UINT32_MAX); anchor = ids[i];
        }
        layout.regions.at(docking::group_of(layout, item->id)).active = item->id;
    });
}
ui_result ui_dock_get_state(ui_dock_item* item, ui_dock_state* state)
{
    if (const auto status = docking::check(item); status != UI_OK) { return status; }
    if (!state) { return UI_ERROR_INVALID_ARGUMENT; } *state = item->dock->layout.placements.at(item->id).state; return UI_OK;
}
ui_result ui_dock_activate(ui_dock_item* item)
{
    if (const auto status = docking::check(item, true); status != UI_OK) { return status; }
    auto& dock = *item->dock;
    const auto state = dock.layout.placements.at(item->id).state;
    if (state == UI_DOCK_HIDDEN || state == UI_DOCK_CLOSED) { return UI_ERROR_INVALID_STATE; }
    if (state == UI_DOCK_AUTO_HIDDEN) { dock.layout.expanded = item->id; }
    else { dock.layout.regions.at(docking::group_of(dock.layout, item->id)).active = item->id; }
    docking::changed(dock, item, UI_DOCK_ACTIVATED); return UI_OK;
}
ui_result ui_dock_hide(ui_dock_item* item)
{
    if (const auto status = docking::check(item, true); status != UI_OK) { return status; }
    return docking::transaction(item->dock, [&](auto& layout) {
        auto& p = layout.placements.at(item->id);
        if (p.state == UI_DOCK_HIDDEN || p.state == UI_DOCK_CLOSED) { return; }
        p.resume = p.state; docking::remove(layout, item->id); p.state = UI_DOCK_HIDDEN; docking::normalize(layout);
    });
}
ui_result ui_dock_show(ui_dock_item* item)
{
    if (const auto status = docking::check(item, true); status != UI_OK) { return status; }
    const auto state = item->dock->layout.placements.at(item->id).state;
    if (state != UI_DOCK_HIDDEN && state != UI_DOCK_CLOSED) { return ui_dock_activate(item); }
    return docking::transaction(item->dock, [&](auto& layout) { docking::return_to(layout, *item); });
}
ui_result ui_dock_close(ui_dock_item* item)
{
    if (const auto status = docking::check(item, true); status != UI_OK) { return status; }
    if (!(item->capabilities & UI_DOCK_CAN_CLOSE)) { return UI_ERROR_INVALID_STATE; }
    auto& dock = *item->dock;
    if (dock.callback) {
        const uint32_t cancel = dock_app_callback(dock, item, UI_DOCK_CLOSING);
        if (cancel) { return UI_ERROR_INVALID_STATE; }
    }
    return docking::transaction(&dock, [&](auto& layout) {
        auto& p = layout.placements.at(item->id);
        if (p.state == UI_DOCK_HIDDEN || p.state == UI_DOCK_CLOSED) { return; }
        p.resume = p.state; docking::remove(layout, item->id); p.state = item->kind == UI_DOCK_TOOL_KIND ? UI_DOCK_HIDDEN : UI_DOCK_CLOSED; docking::normalize(layout);
    });
}
ui_result ui_dock_float(ui_dock_item* item, const ui_rect* bounds, uint32_t whole_group)
{
    if (const auto status = docking::check(item, true); status != UI_OK) { return status; }
    if (!bounds || bounds->width < 64 || bounds->height < 64 || bounds->width > 8192 || bounds->height > 8192 ||
        bounds->x < -100000 || bounds->x > 100000 || bounds->y < -100000 || bounds->y > 100000 || whole_group > 1) { return UI_ERROR_INVALID_ARGUMENT; }
    return docking::transaction(item->dock, [&](auto& layout) {
        const auto group = docking::group_of(layout, item->id);
        std::vector<std::string> ids = whole_group && group ? layout.regions.at(group).tabs : std::vector<std::string>{item->id};
        for (const auto& id : ids) { if (!(docking::find(*item->dock, id)->capabilities & UI_DOCK_CAN_FLOAT)) { throw std::range_error("Dock item cannot float"); } }
        for (const auto& id : ids) { docking::remove(layout, id); }
        const auto fresh = docking::make_group(layout, item->kind);
        auto& node = layout.regions.at(fresh); node.tabs = ids; node.active = item->id;
        auto rectangle = *bounds;
        const auto& theme = dock_app_theme(item->dock->owner);
        const auto height = [&](const char* key) { const auto found = theme.metrics.find(key); return found == theme.metrics.end() ? theme.palette.control_height : found->second; };
        const unsigned chrome = std::max(height("dock.title_height"), height("dock.tab_height"));
        for (const auto& id : ids) {
            const auto* tab = docking::find(*item->dock, id);
            rectangle.width = std::max(rectangle.width, static_cast<int>(std::ceil(tab->min_width)));
            rectangle.height = std::max(rectangle.height, static_cast<int>(std::ceil(tab->min_height + chrome)));
            auto& p = layout.placements.at(id); p.state = UI_DOCK_FLOATING; p.group = fresh;
        }
        layout.floating.push_back({layout.next_window++, fresh, rectangle}); docking::normalize(layout);
    });
}
ui_result ui_dock_auto_hide(ui_dock_item* item, ui_dock_edge edge)
{
    if (const auto status = docking::check(item, true); status != UI_OK) { return status; }
    if (edge < UI_DOCK_LEFT || edge > UI_DOCK_BOTTOM) { return UI_ERROR_INVALID_ARGUMENT; }
    if (item->kind != UI_DOCK_TOOL_KIND || !(item->capabilities & UI_DOCK_CAN_AUTO_HIDE)) { return UI_ERROR_INVALID_STATE; }
    return docking::transaction(item->dock, [&](auto& layout) { docking::remove(layout, item->id); auto& p = layout.placements.at(item->id); p.state = UI_DOCK_AUTO_HIDDEN; p.edge = edge; docking::normalize(layout); });
}
ui_result ui_dock_pin(ui_dock_item* item)
{
    if (const auto status = docking::check(item, true); status != UI_OK) { return status; }
    if (item->dock->layout.placements.at(item->id).state != UI_DOCK_AUTO_HIDDEN) { return UI_ERROR_INVALID_STATE; }
    return docking::transaction(item->dock, [&](auto& layout) {
        docking::return_to(layout, *item); layout.expanded.clear();
    });
}
static ui_result copy_dock_string(const std::string& text, char* output, uint32_t* size)
{
    if (!size) { return UI_ERROR_INVALID_ARGUMENT; }
    const auto required = static_cast<uint32_t>(text.size() + 1);
    if (!output) { *size = required; return UI_OK; }
    if (*size < required) { *size = required; return UI_ERROR_INVALID_ARGUMENT; }
    std::memcpy(output, text.c_str(), required); *size = required; return UI_OK;
}
ui_result ui_dock_get_id(ui_dock_item* item, char* text, uint32_t* size)
{
    if (const auto status = docking::check(item); status != UI_OK) { return status; }
    return copy_dock_string(item->id, text, size);
}
ui_result ui_dock_set_title(ui_dock_item* item, const char* title)
{
    if (const auto status = docking::check(item, true); status != UI_OK) { return status; }
    try {
        if (!docking::valid_text(title, 4096)) { return UI_ERROR_INVALID_ARGUMENT; }
        item->title = title; docking::changed(*item->dock, item); return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; } catch (...) { return UI_ERROR_INVALID_ARGUMENT; }
}
ui_result ui_dock_set_icon(ui_dock_item* item, const char* icon)
{
    if (const auto status = docking::check(item, true); status != UI_OK) { return status; }
    try {
        if (!docking::valid_text(icon, 64)) { return UI_ERROR_INVALID_ARGUMENT; }
        item->icon = icon; docking::changed(*item->dock, item); return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; } catch (...) { return UI_ERROR_INVALID_ARGUMENT; }
}
ui_result ui_dock_capture_default(ui_dock* dock)
{
    if (const auto status = docking::check(dock, true); status != UI_OK) { return status; }
    try {
        auto defaults = dock->layout.placements;
        for (const auto& item : dock->items) {
            auto snapshot = dock->layout; docking::remove(snapshot, item->id); defaults[item->id] = snapshot.placements.at(item->id);
        }
        auto layout = dock->layout; dock->initial = std::move(layout); dock->defaults = std::move(defaults); return UI_OK;
    }
    catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_dock_reset(ui_dock* dock)
{
    if (const auto status = docking::check(dock, true); status != UI_OK) { return status; }
    if (!dock->initial) { return UI_ERROR_INVALID_STATE; }
    return docking::transaction(dock, [&](auto& layout) { layout = docking::restore(docking::save(*dock->initial), *dock); });
}
ui_result ui_dock_save(ui_dock* dock, char* output, uint32_t* size)
{
    if (const auto status = docking::check(dock); status != UI_OK) { return status; }
    try { return copy_dock_string(docking::save(dock->layout), output, size); } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_dock_restore(ui_dock* dock, const char* text, size_t size, ui_fleck_diagnostic* diagnostic)
{
    if (diagnostic) { *diagnostic = {}; }
    if (const auto status = docking::check(dock, true); status != UI_OK) { return status; }
    if (!text) { return UI_ERROR_INVALID_ARGUMENT; }
    try { auto layout = docking::restore(std::string_view(text, size), *dock); docking::cancel(*dock); dock->layout = std::move(layout); docking::changed(*dock); return UI_OK; }
    catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
    catch (const std::exception& error) { if (diagnostic) { fleck::report({}, 0, UI_FLECK_VALUE, error.what(), *diagnostic); } return UI_ERROR_INVALID_ARGUMENT; }
}
ui_result ui_dock_get_windows(ui_dock* dock, uint32_t* ids, uint32_t* count)
{
    if (const auto status = docking::check(dock); status != UI_OK) { return status; }
    if (!count) { return UI_ERROR_INVALID_ARGUMENT; }
    const auto required = static_cast<uint32_t>(dock->layout.floating.size());
    if (!ids) { *count = required; return UI_OK; }
    if (*count < required) { *count = required; return UI_ERROR_INVALID_ARGUMENT; }
    for (size_t i = 0; i < required; ++i) { ids[i] = dock->layout.floating[i].id; } *count = required; return UI_OK;
}
ui_result ui_dock_pointer(ui_dock* dock, uint32_t window, ui_pointer_action action, int32_t x, int32_t y)
{
    if (const auto status = docking::check(dock, true); status != UI_OK) { return status; }
    if (action < UI_POINTER_DOWN || action > UI_POINTER_CANCEL) { return UI_ERROR_INVALID_ARGUMENT; }
    try { return dock_app_pointer(dock, window, action, x, y); }
    catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; } catch (...) { return UI_ERROR_INVALID_STATE; }
}
const uint8_t* ui_dock_pixels(ui_dock* dock, uint32_t window, uint32_t* width, uint32_t* height)
{
    if (docking::check(dock) != UI_OK) { return nullptr; }
    return dock_workspace_pixels(dock, window, width, height);
}

namespace docking {
bool close_window(ui_dock& dock, unsigned window)
{
    if (dock.drag.prepared && (dock.drag.window == window || dock.drag.target_window == window)) { cancel(dock); }
    std::vector<ui_dock_item*> items;
    for (const auto& item : dock.items) {
        const auto group = group_of(dock.layout, item->id);
        if (group && window_of(dock.layout, group) == window) { items.push_back(item.get()); }
    }
    for (auto* item : items) { (void)ui_dock_close(item); }
    return std::none_of(dock.layout.floating.begin(), dock.layout.floating.end(), [&](const auto& w) { return w.id == window; });
}
std::unique_ptr<ui_dock> from_fleck(ui_app* app, ui_node& root)
{
    std::vector<ui_node*> hosts;
    const auto scan = [&](const auto& self, ui_node& node) -> void {
        if (node.kind == UI_DOCK_HOST) { hosts.push_back(&node); }
        for (auto* child : node.children) { self(self, *child); }
    };
    scan(scan, root);
    if (hosts.empty()) { return {}; }
    if (hosts.size() != 1) { throw std::range_error("Only one DockHost is allowed per App"); }
    auto dock = std::make_unique<ui_dock>(); dock->owner = app; dock->host = hosts.front();
    dock->layout.regions.clear(); dock->layout.next = 1;
    const auto build = [&](const auto& self, ui_node& node, unsigned depth) -> unsigned {
        if (depth > 64) { throw std::range_error("Dock layout is too deep"); }
        if (node.kind == UI_DOCK_HOST) {
            if (node.children.size() != 1) { throw std::range_error("DockHost requires one DockSplit or DockGroup"); }
            return self(self, *node.children.front(), depth + 1);
        }
        const auto id = dock->layout.next++;
        Region region{id};
        if (node.kind == UI_DOCK_SPLIT) {
            if (node.children.size() != 2) { throw std::range_error("DockSplit requires two regions"); }
            region.axis = node.dock == UI_DOCK_TOP ? 2 : 1; region.ratio = std::clamp(node.value, 1u, 99u) * 100;
            region.first = self(self, *node.children[0], depth + 1); region.second = self(self, *node.children[1], depth + 1);
        } else if (node.kind == UI_DOCK_GROUP) {
            for (auto* tab : node.children) {
                if (tab->fleck_id.empty() || find(*dock, tab->fleck_id)) { throw std::range_error("Dock documents and tools require unique Ids"); }
                const auto kind = tab->kind == UI_DOCK_TOOL ? UI_DOCK_TOOL_KIND : UI_DOCK_DOCUMENT_KIND;
                if (!region.tabs.empty() && region.kind != kind) { throw std::range_error("Document and tool tabs cannot share a group"); }
                region.kind = kind; region.tabs.push_back(tab->fleck_id);
                dock->items.push_back(std::make_unique<ui_dock_item>(ui_dock_item{dock.get(), tab->fleck_id,
                    tab->dock_title.empty() ? tab->fleck_id : tab->dock_title, tab, kind, tab->dock_capabilities, tab->dock_min_width, tab->dock_min_height, tab->dock_icon}));
                dock->layout.placements[tab->fleck_id] = {UI_DOCK_DOCKED, id, static_cast<unsigned>(region.tabs.size() - 1), UI_DOCK_LEFT};
            }
            if (node.value >= region.tabs.size() && !region.tabs.empty()) { throw std::range_error("Invalid active Dock tab"); }
            region.active = region.tabs.empty() ? "" : region.tabs[node.value];
        } else { throw std::range_error("Invalid Dock layout child"); }
        dock->layout.regions.emplace(id, std::move(region)); return id;
    };
    dock->layout.root = build(build, *dock->host, 0); dock->initial = dock->layout; dock->defaults = dock->layout.placements;
    for (const auto& item : dock->items) {
        auto snapshot = dock->layout; remove(snapshot, item->id); dock->defaults[item->id] = snapshot.placements.at(item->id);
    }
    dock->host->workspace = dock.get(); return dock;
}
}
