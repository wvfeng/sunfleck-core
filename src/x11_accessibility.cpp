#include "x11_accessibility.h"

#include <atk/atk.h>
#include <atk-bridge.h>
#include <algorithm>
#include <map>
#include <mutex>
#include <thread>
#include <utility>

namespace x11_accessibility {
namespace {
struct Node;
}
struct Window {
    void* context;
    Perform perform;
    std::map<uint64_t, Node*> nodes;
};

namespace {
struct NodeData {
    Element value;
    Window* window = nullptr;
    Node* parent = nullptr;
    std::vector<Node*> children;
};
struct Node {
    AtkObject parent;
    NodeData* data;
};
struct NodeClass { AtkObjectClass parent; };
struct EditorNode { Node parent; };
struct EditorNodeClass { NodeClass parent; };
struct FrameNode { Node parent; };
struct FrameNodeClass { NodeClass parent; };

void action_interface(AtkActionIface* iface);
void component_interface(AtkComponentIface* iface);
void text_interface(AtkTextIface* iface);
G_DEFINE_TYPE_WITH_CODE(Node, sunfleck_accessible, ATK_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(ATK_TYPE_ACTION, action_interface)
    G_IMPLEMENT_INTERFACE(ATK_TYPE_COMPONENT, component_interface))
G_DEFINE_TYPE_WITH_CODE(EditorNode, sunfleck_accessible_editor, sunfleck_accessible_get_type(),
    G_IMPLEMENT_INTERFACE(ATK_TYPE_TEXT, text_interface))
G_DEFINE_TYPE_WITH_CODE(FrameNode, sunfleck_accessible_frame, sunfleck_accessible_get_type(),
    G_IMPLEMENT_INTERFACE(ATK_TYPE_WINDOW, nullptr))

std::mutex init_mutex;
std::thread::id owner;
Node* root = nullptr;
thread_local GMainContext* events = nullptr;

AtkObject* object(Node* node) { return node ? ATK_OBJECT(node) : nullptr; }
Node* node_cast(void* self) { return static_cast<Node*>(self); }

AtkRole role(ui_node_kind kind)
{
    switch (kind) {
    case UI_BUTTON: return ATK_ROLE_PUSH_BUTTON;
    case UI_TOGGLE_BUTTON: return ATK_ROLE_TOGGLE_BUTTON;
    case UI_CHECK_BOX: case UI_SWITCH: return ATK_ROLE_CHECK_BOX;
    case UI_RADIO_BUTTON: return ATK_ROLE_RADIO_BUTTON;
    case UI_SLIDER: return ATK_ROLE_SLIDER;
    case UI_PROGRESS_BAR: return ATK_ROLE_PROGRESS_BAR;
    case UI_COMBO_BOX: return ATK_ROLE_COMBO_BOX;
    case UI_TEXT_BOX: case UI_TEXT_AREA: case UI_SEARCH_BOX: return ATK_ROLE_ENTRY;
    case UI_PASSWORD_BOX: return ATK_ROLE_PASSWORD_TEXT;
    case UI_LIST_VIEW: return ATK_ROLE_LIST;
    case UI_LIST_ITEM: return ATK_ROLE_LIST_ITEM;
    case UI_TREE_VIEW: return ATK_ROLE_TREE;
    case UI_TREE_ITEM: return ATK_ROLE_TREE_ITEM;
    case UI_TAB_VIEW: return ATK_ROLE_PAGE_TAB_LIST;
    case UI_TAB_ITEM: return ATK_ROLE_PAGE_TAB;
    case UI_DOCUMENT_VIEW: return ATK_ROLE_DOCUMENT_FRAME;
    case UI_LABEL: case UI_TEXT_BLOCK: return ATK_ROLE_LABEL;
    default: return ATK_ROLE_PANEL;
    }
}

void sunfleck_accessible_init(Node* node) { node->data = new NodeData; }
void sunfleck_accessible_editor_init(EditorNode*) {}
void sunfleck_accessible_editor_class_init(EditorNodeClass*) {}
void sunfleck_accessible_frame_init(FrameNode*) {}
void sunfleck_accessible_frame_class_init(FrameNodeClass*) {}
void sunfleck_accessible_class_init(NodeClass* type)
{
    G_OBJECT_CLASS(type)->finalize = [](GObject* instance) {
        delete node_cast(instance)->data;
        G_OBJECT_CLASS(sunfleck_accessible_parent_class)->finalize(instance);
    };
    auto* klass = ATK_OBJECT_CLASS(type);
    klass->get_parent = [](AtkObject* self) { return object(node_cast(self)->data->parent); };
    klass->get_n_children = [](AtkObject* self) { return static_cast<gint>(node_cast(self)->data->children.size()); };
    klass->ref_child = [](AtkObject* self, gint index) -> AtkObject* {
        const auto& children = node_cast(self)->data->children;
        if (index < 0 || static_cast<size_t>(index) >= children.size()) { return nullptr; }
        return ATK_OBJECT(g_object_ref(children[index]));
    };
    klass->get_index_in_parent = [](AtkObject* self) -> gint {
        const auto* node = node_cast(self);
        if (!node->data->parent) { return -1; }
        const auto& siblings = node->data->parent->data->children;
        const auto found = std::find(siblings.begin(), siblings.end(), node);
        return found == siblings.end() ? -1 : static_cast<gint>(found - siblings.begin());
    };
    klass->ref_state_set = [](AtkObject* self) {
        auto* states = atk_state_set_new();
        const auto* data = node_cast(self)->data;
        if (data->parent || atk_object_get_role(self) == ATK_ROLE_APPLICATION) {
            atk_state_set_add_state(states, ATK_STATE_SHOWING);
            atk_state_set_add_state(states, ATK_STATE_VISIBLE);
        } else {
            atk_state_set_add_state(states, ATK_STATE_DEFUNCT);
        }
        const auto& value = data->value;
        if (value.enabled) {
            atk_state_set_add_state(states, ATK_STATE_ENABLED);
            atk_state_set_add_state(states, ATK_STATE_SENSITIVE);
        }
        if (value.focusable) { atk_state_set_add_state(states, ATK_STATE_FOCUSABLE); }
        if (value.focused) { atk_state_set_add_state(states, ATK_STATE_FOCUSED); }
        switch (value.kind) {
        case UI_CHECK_BOX: case UI_SWITCH: case UI_RADIO_BUTTON: case UI_TOGGLE_BUTTON:
            atk_state_set_add_state(states, ATK_STATE_CHECKABLE);
            break;
        default:
            break;
        }
        if (value.checked) { atk_state_set_add_state(states, ATK_STATE_CHECKED); }
        if (value.active) { atk_state_set_add_state(states, ATK_STATE_ACTIVE); }
        if (ATK_IS_TEXT(self)) { atk_state_set_add_state(states, ATK_STATE_EDITABLE); }
        return states;
    };
}

bool activatable(ui_node_kind kind)
{
    switch (kind) {
    case UI_BUTTON: case UI_CHECK_BOX: case UI_SWITCH: case UI_RADIO_BUTTON:
    case UI_TOGGLE_BUTTON: case UI_TREE_ITEM: case UI_LIST_ITEM: case UI_TAB_ITEM:
        return true;
    default:
        return false;
    }
}

gboolean perform(void* self, Action action)
{
    const auto* data = node_cast(self)->data;
    if (!data->window || !data->value.enabled || !data->value.focusable) { return FALSE; }
    try {
        return data->window->perform(data->window->context, data->value.id, action);
    } catch (...) {
        return FALSE;
    }
}

void action_interface(AtkActionIface* iface)
{
    iface->get_n_actions = [](AtkAction* self) { return activatable(node_cast(self)->data->value.kind) ? 1 : 0; };
    iface->get_name = [](AtkAction* self, gint index) -> const gchar* {
        return !index && activatable(node_cast(self)->data->value.kind) ? "activate" : nullptr;
    };
    iface->do_action = [](AtkAction* self, gint index) -> gboolean {
        return !index && activatable(node_cast(self)->data->value.kind) && perform(self, Action::activate);
    };
}

void component_interface(AtkComponentIface* iface)
{
    iface->get_extents = [](AtkComponent* self, gint* x, gint* y, gint* width, gint* height, AtkCoordType coords) {
        const auto* data = node_cast(self)->data;
        const auto& value = data->value;
        const Node* relative = nullptr;
        if (coords == ATK_XY_WINDOW && data->window && value.id) { relative = data->window->nodes.at(0); }
        if (coords == ATK_XY_PARENT) { relative = data->parent; }
        *x = value.x - (relative ? relative->data->value.x : 0);
        *y = value.y - (relative ? relative->data->value.y : 0);
        *width = value.width;
        *height = value.height;
    };
    iface->grab_focus = [](AtkComponent* self) { return perform(self, Action::focus); };
}

void text_interface(AtkTextIface* iface)
{
    iface->get_caret_offset = [](AtkText* self) { return node_cast(self)->data->value.caret; };
    iface->get_character_count = [](AtkText* self) {
        return static_cast<gint>(g_utf8_strlen(node_cast(self)->data->value.text.c_str(), -1));
    };
    iface->get_text = [](AtkText* self, gint first, gint end) {
        const auto& text = node_cast(self)->data->value.text;
        const auto count = static_cast<gint>(g_utf8_strlen(text.c_str(), -1));
        first = std::clamp(first, 0, count);
        end = end < 0 ? count : std::clamp(end, first, count);
        const auto* start = g_utf8_offset_to_pointer(text.c_str(), first);
        const auto* stop = g_utf8_offset_to_pointer(start, end - first);
        return g_strndup(start, stop - start);
    };
    iface->get_string_at_offset = [](AtkText* self, gint offset, AtkTextGranularity granularity,
                                     gint* first, gint* end) -> gchar* {
        const auto& text = node_cast(self)->data->value.text;
        *first = *end = -1;
        if (offset < 0 || offset >= g_utf8_strlen(text.c_str(), -1)) { return nullptr; }
        const auto* begin = text.c_str();
        const auto* start = g_utf8_offset_to_pointer(begin, offset);
        const auto* stop = g_utf8_next_char(start);
        if (granularity == ATK_TEXT_GRANULARITY_LINE || granularity == ATK_TEXT_GRANULARITY_PARAGRAPH) {
            while (start > begin && start[-1] != '\n') {
                --start;
            }
            while (*stop && stop[-1] != '\n') {
                ++stop;
            }
        } else if (granularity != ATK_TEXT_GRANULARITY_CHAR) {
            return nullptr;
        }
        *first = static_cast<gint>(g_utf8_pointer_to_offset(begin, start));
        *end = static_cast<gint>(g_utf8_pointer_to_offset(begin, stop));
        return g_strndup(start, stop - start);
    };
}

Node* make_node(bool editor)
{
    const auto type = editor ? sunfleck_accessible_editor_get_type() : sunfleck_accessible_get_type();
    return node_cast(g_object_new(type, nullptr));
}

void detach(Node* node)
{
    auto* data = node->data;
    data->window = nullptr;
    data->parent = nullptr;
    data->children.clear();
    data->value = {};
    atk_object_set_name(object(node), "");
    atk_object_notify_state_change(object(node), ATK_STATE_DEFUNCT, TRUE);
}
}

Window* open(void* context, Perform perform)
{
    std::lock_guard lock(init_mutex);
    if (owner != std::thread::id{} && owner != std::this_thread::get_id()) { return nullptr; }
    if (!root) {
        if (atk_get_root() || !g_getenv("DBUS_SESSION_BUS_ADDRESS")) { return nullptr; }
        auto* klass = ATK_UTIL_CLASS(g_type_class_ref(ATK_TYPE_UTIL));
        const auto previous_root = klass->get_root;
        const auto previous_name = klass->get_toolkit_name;
        const auto previous_version = klass->get_toolkit_version;
        root = make_node(false);
        atk_object_set_name(object(root), "Sunfleck");
        atk_object_set_role(object(root), ATK_ROLE_APPLICATION);
        klass->get_root = [] { return object(root); };
        klass->get_toolkit_name = [] { return "Sunfleck"; };
        klass->get_toolkit_version = [] { return "0.2"; };
        if (atk_bridge_adaptor_init(nullptr, nullptr) != 0) {
            klass->get_root = previous_root;
            klass->get_toolkit_name = previous_name;
            klass->get_toolkit_version = previous_version;
            g_object_unref(root);
            root = nullptr;
            return nullptr;
        }
        owner = std::this_thread::get_id();
        events = g_main_context_new();
        atk_bridge_set_event_context(events);
    }
    auto* window = new Window{context, perform, {}};
    return window;
}

void update(Window* window, std::vector<Element> elements)
{
    if (!window || elements.empty()) { return; }
    std::map<uint64_t, Node*> active;
    for (auto& entry : window->nodes) { entry.second->data->children.clear(); }
    for (const auto& value : elements) {
        Node* node = nullptr;
        const auto previous = window->nodes.find(value.id);
        if (previous != window->nodes.end()) {
            node = previous->second;
        } else {
            const bool editor = value.kind == UI_TEXT_BOX || value.kind == UI_TEXT_AREA ||
                value.kind == UI_SEARCH_BOX || value.kind == UI_PASSWORD_BOX;
            node = value.id ? make_node(editor) : node_cast(g_object_new(sunfleck_accessible_frame_get_type(), nullptr));
            node->data->window = window;
            if (value.id) {
                atk_object_set_role(object(node), role(value.kind));
            } else {
                atk_object_set_role(object(node), ATK_ROLE_FRAME);
                root->data->children.push_back(node);
            }
        }
        active.emplace(value.id, node);
        node->data->parent = value.id ? active.at(value.parent) : root;
        if (value.id) { node->data->parent->data->children.push_back(node); }
    }
    auto previous = std::exchange(window->nodes, std::move(active));
    for (auto& value : elements) {
        auto* node = window->nodes.at(value.id);
        const auto old = std::exchange(node->data->value, std::move(value));
        const auto& current = node->data->value;
        if (old.name != current.name) { atk_object_set_name(object(node), current.name.c_str()); }
        if (old.active != current.active) {
            atk_object_notify_state_change(object(node), ATK_STATE_ACTIVE, current.active);
            g_signal_emit_by_name(node, current.active ? "activate" : "deactivate");
        }
        if (old.focused != current.focused) {
            atk_object_notify_state_change(object(node), ATK_STATE_FOCUSED, current.focused);
        }
        if (old.checked != current.checked) {
            atk_object_notify_state_change(object(node), ATK_STATE_CHECKED, current.checked);
        }
        if (old.enabled != current.enabled) {
            atk_object_notify_state_change(object(node), ATK_STATE_ENABLED, current.enabled);
        }
        if (ATK_IS_TEXT(node) && old.text != current.text) {
            const auto* old_text = old.text.c_str();
            const auto* new_text = current.text.c_str();
            int start = 0;
            while (*old_text && *new_text && g_utf8_get_char(old_text) == g_utf8_get_char(new_text)) {
                old_text = g_utf8_next_char(old_text);
                new_text = g_utf8_next_char(new_text);
                ++start;
            }
            const auto removed = static_cast<gint>(g_utf8_strlen(old_text, -1));
            const auto added = static_cast<gint>(g_utf8_strlen(new_text, -1));
            if (removed) { g_signal_emit_by_name(node, "text-changed::delete", start, removed); }
            if (added) { g_signal_emit_by_name(node, "text-changed::insert", start, added); }
        }
        if (ATK_IS_TEXT(node) && old.caret != current.caret) {
            g_signal_emit_by_name(node, "text-caret-moved", current.caret);
        }
    }
    for (auto& [id, node] : previous) {
        if (!window->nodes.contains(id)) {
            detach(node);
            g_object_unref(node);
        }
    }
}

void close(Window* window)
{
    if (!window) { return; }
    if (const auto frame = window->nodes.find(0); frame != window->nodes.end()) {
        std::erase(root->data->children, frame->second);
    }
    for (auto& entry : window->nodes) {
        detach(entry.second);
        g_object_unref(entry.second);
    }
    delete window;
}

void poll()
{
    if (!events) { return; }
    for (unsigned count = 0; count < 16 && g_main_context_iteration(events, FALSE); ++count) {}
}
}
