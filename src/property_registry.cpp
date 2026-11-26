#include "property.h"
#include "components.h"
#include "ui_tree.h"
#include <atomic>
#include <limits>

// Tokens never contain addresses and are never recycled. Failed registrations
// may consume tokens, so a stale token cannot accidentally identify a new type.
ui_property_id properties::allocate_id() {
    static std::atomic<ui_property_id> next{ui_property_id{1} << 32};
    auto id = next.load(std::memory_order_relaxed);
    while (id != std::numeric_limits<ui_property_id>::max()) {
        if (next.compare_exchange_weak(id, id + 1, std::memory_order_relaxed)) return id;
    }
    return 0;
}

namespace {
struct Target {
    ui_property builtin{};
    const components::Property* custom = nullptr;
};
ui_result resolve(ui_node* node, ui_property_id id, Target& target) {
    if (!node || !id) return UI_ERROR_INVALID_ARGUMENT;
    if (auto result = binding_app_check(node->owner); result != UI_OK) return result;
    if (node->component && node->component->destroyed) return UI_ERROR_INVALID_STATE;
    if (id <= properties::count) {
        target.builtin = ui_property(id - 1);
        ui_property_descriptor descriptor{sizeof(descriptor), 1};
        return ui_property_describe(node->kind, target.builtin, &descriptor);
    }
    if (node->component) for (const auto& property : node->component->type->properties) {
        if (property.id == id) { target.custom = &property; return UI_OK; }
    }
    return UI_ERROR_INVALID_ARGUMENT;
}
}

ui_result ui_node_resolve_property(ui_node* node, const char* name, ui_property_id* property) {
    if (!node || !name || !property) return UI_ERROR_INVALID_ARGUMENT;
    if (auto result = binding_app_check(node->owner); result != UI_OK) return result;
    if (node->component) {
        if (node->component->destroyed) return UI_ERROR_INVALID_STATE;
        for (const auto& candidate : node->component->type->properties) {
            if (candidate.name == name) { *property = candidate.id; return UI_OK; }
        }
    }
    ui_property builtin;
    if (!properties::find(name, builtin)) return UI_ERROR_INVALID_ARGUMENT;
    Target target;
    const auto id = ui_property_id(builtin) + 1;
    if (auto result = resolve(node, id, target); result != UI_OK) return result;
    *property = id;
    return UI_OK;
}

ui_result ui_node_describe_registered_property(ui_node* node, ui_property_id property, ui_registered_property_descriptor* descriptor) {
    if (!descriptor || descriptor->struct_size != sizeof(*descriptor)) return UI_ERROR_INVALID_ARGUMENT;
    if (descriptor->version != 1) return UI_ERROR_VERSION_MISMATCH;
    Target target;
    if (auto result = resolve(node, property, target); result != UI_OK) return result;
    ui_registered_property_descriptor output{};
    output.struct_size = sizeof(output); output.version = 1; output.id = property;
    if (target.custom) {
        const auto& value = *target.custom;
        output.name = value.name.c_str();
        output.owner_namespace = node->component->type->key.first.c_str();
        output.owner_name = node->component->type->key.second.c_str();
        output.kind = value.value.kind; output.flags = value.metadata.flags;
        output.read_only = value.read_only; output.default_value = value.value;
    } else {
        ui_property_descriptor builtin{sizeof(builtin), 1};
        if (auto result = ui_property_describe(node->kind, target.builtin, &builtin); result != UI_OK) return result;
        output.name = builtin.name;
        output.owner_namespace = "urn:sunfleck:v1"; output.owner_name = "VisualElement";
        output.kind = builtin.kind; output.flags = builtin.flags; output.default_value = builtin.default_value;
    }
    *descriptor = output;
    return UI_OK;
}

ui_result ui_node_get_registered_property(ui_node* node, ui_property_id property, ui_binding_value* value) {
    Target target;
    if (auto result = resolve(node, property, target); result != UI_OK) return result;
    return target.custom ? ui_component_get_property(node->component.get(), target.custom->name.c_str(), value)
                         : ui_node_get_property(node, target.builtin, value);
}
ui_result ui_node_get_registered_base_property(ui_node* node, ui_property_id property, ui_binding_value* value) {
    Target target;
    if (auto result = resolve(node, property, target); result != UI_OK) return result;
    return target.custom ? ui_component_get_base_property(node->component.get(), target.custom->name.c_str(), value)
                         : ui_node_get_base_property(node, target.builtin, value);
}
ui_result ui_node_get_registered_property_source(ui_node* node, ui_property_id property, ui_property_source* source) {
    Target target;
    if (auto result = resolve(node, property, target); result != UI_OK) return result;
    return target.custom ? ui_component_get_property_source(node->component.get(), target.custom->name.c_str(), source)
                         : ui_node_get_property_source(node, target.builtin, source);
}
ui_result ui_node_set_registered_property(ui_node* node, ui_property_id property, ui_value_precedence precedence, const ui_binding_value* value) {
    Target target;
    if (auto result = resolve(node, property, target); result != UI_OK) return result;
    if (target.custom && target.custom->read_only) return UI_ERROR_INVALID_ARGUMENT;
    return target.custom ? ui_component_set_property_layer(node->component.get(), target.custom->name.c_str(), precedence, value)
                         : ui_node_set_property_layer(node, target.builtin, precedence, value);
}
ui_result ui_node_clear_registered_property(ui_node* node, ui_property_id property, ui_value_precedence precedence) {
    Target target;
    if (auto result = resolve(node, property, target); result != UI_OK) return result;
    if (target.custom && target.custom->read_only) return UI_ERROR_INVALID_ARGUMENT;
    return target.custom ? ui_component_clear_property_layer(node->component.get(), target.custom->name.c_str(), precedence)
                         : ui_node_clear_property_layer(node, target.builtin, precedence);
}
ui_result ui_node_bind_registered_property(ui_node* node, ui_model* model, const ui_registered_binding_options* options) {
    if (!options || options->struct_size != sizeof(*options)) return UI_ERROR_INVALID_ARGUMENT;
    if (options->version != 1) return UI_ERROR_VERSION_MISMATCH;
    Target target;
    if (auto result = resolve(node, options->property, target); result != UI_OK) return result;
    if (target.custom) {
        if (options->trigger != UI_UPDATE_PROPERTY_CHANGED || (options->string_format && *options->string_format))
            return UI_ERROR_INVALID_ARGUMENT;
        return ui_component_bind_property(node->component.get(), target.custom->name.c_str(), model, options->path, options->mode);
    }
    const ui_binding_options binding{sizeof(binding), 1, target.builtin, options->path, options->mode, options->trigger, options->string_format};
    return ui_node_bind_property(node, model, &binding);
}

ui_result ui_node_bind_registered_context_property(ui_node* node, const ui_registered_binding_options* options) {
    if (!options || options->struct_size != sizeof(*options)) return UI_ERROR_INVALID_ARGUMENT;
    if (options->version != 1) return UI_ERROR_VERSION_MISMATCH;
    Target target;
    if (auto result = resolve(node, options->property, target); result != UI_OK) return result;
    if (target.custom) {
        if (options->trigger != UI_UPDATE_PROPERTY_CHANGED || (options->string_format && *options->string_format))
            return UI_ERROR_INVALID_ARGUMENT;
        return components::bind_context_property(*node->component, target.custom->name.c_str(), options->path, options->mode);
    }
    const ui_binding_options binding{sizeof(binding), 1, target.builtin, options->path, options->mode, options->trigger, options->string_format};
    return ui_node_bind_context_property(node, &binding);
}
