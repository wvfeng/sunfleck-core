#pragma once
#include <sunfleck/ui.h>
#include <string_view>
#include <map>
#include <algorithm>
#include <vector>
#include "property_value.h"

namespace properties {
// At most seven precedence levels exist. Contiguous storage avoids allocating
// a tree node for each level, especially the common single local value.
class LayerValues {
    using Entry = std::pair<ui_value_precedence, BindingValue>;
    std::vector<Entry> entries;
    auto lower(ui_value_precedence key) {
        return std::lower_bound(entries.begin(), entries.end(), key,
            [](const Entry& entry, ui_value_precedence value) { return entry.first < value; });
    }
public:
    bool empty() const noexcept { return entries.empty(); }
    size_t size() const noexcept { return entries.size(); }
    auto begin() noexcept { return entries.begin(); }
    auto end() noexcept { return entries.end(); }
    auto begin() const noexcept { return entries.begin(); }
    auto end() const noexcept { return entries.end(); }
    auto rbegin() const noexcept { return entries.rbegin(); }
    auto rend() const noexcept { return entries.rend(); }
    bool contains(ui_value_precedence key) const {
        return std::any_of(entries.begin(), entries.end(), [key](const Entry& entry) { return entry.first == key; });
    }
    BindingValue& operator[](ui_value_precedence key) {
        auto found = lower(key);
        if (found == entries.end() || found->first != key) found = entries.emplace(found, key, BindingValue{});
        return found->second;
    }
    void insert_or_assign(ui_value_precedence key, BindingValue value) {
        auto found = lower(key);
        if (found != entries.end() && found->first == key) found->second = std::move(value);
        else entries.emplace(found, key, std::move(value));
    }
    void erase(ui_value_precedence key) {
        auto found = lower(key);
        if (found != entries.end() && found->first == key) entries.erase(found);
    }
};
struct Slot {
    LayerValues values;
    const BindingValue* effective(bool base = false) const {
        for (auto it = values.rbegin(); it != values.rend(); ++it) {
            if (!base || it->first != UI_PRECEDENCE_ANIMATION) return &it->second;
        }
        return nullptr;
    }
};
inline constexpr unsigned count = UI_PROPERTY_SOURCE + 1;
inline bool metric(ui_property property) { return property >= UI_PROPERTY_SPACING && property <= UI_PROPERTY_WEIGHT; }
inline ui_property canonical(ui_property property) {
    return property == UI_PROPERTY_CONTENT ? UI_PROPERTY_TEXT : property;
}
bool valid(ui_property property);
ui_property_id allocate_id();
const char* name(ui_property property);
bool find(std::string_view name, ui_property& property);
void written(ui_node& node, ui_property property);
void capture(ui_node& node, ui_property property);
ui_result update_triggers(ui_node& node);
void initialize(ui_node& node);
ui_result validate(ui_node& node, ui_property property, const ui_binding_value& value);
ui_result set_metric_pair(ui_node& node, ui_property first, double first_value, ui_property second, double second_value);
const char* source_name(ui_property_source source);
}

// Immediately retire input state when visibility/enabled/hit testing changes.
void reconcile_input_state(ui_node& node);
