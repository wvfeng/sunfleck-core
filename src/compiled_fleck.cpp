#include "fleck.h"
#include "ui_tree.h"

#include <bit>
#include <stdexcept>
#include <limits>
#include <algorithm>

struct ui_fleck_builder {
    struct Node {
        fleck::Element element;
        uint32_t parent;
        unsigned depth;
    };
    std::vector<Node> nodes;
    ui_result status = UI_OK;
    uint32_t api_version = 0;
};

ui_result ui_fleck_set_dock_icon(ui_fleck_builder* builder, uint32_t index, const char* icon)
{
    if (!builder || builder->status != UI_OK || index >= builder->nodes.size() || !icon) { return UI_ERROR_INVALID_ARGUMENT; }
    auto& node = builder->nodes[index].element;
    if ((node.kind != UI_DOCK_DOCUMENT && node.kind != UI_DOCK_TOOL) || std::string_view(icon).size() > 64) { return builder->status = UI_ERROR_INVALID_ARGUMENT; }
    try { node.dock_icon = icon; return UI_OK; } catch (const std::bad_alloc&) { return builder->status = UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_fleck_set_dock_options(ui_fleck_builder* builder, uint32_t index, uint32_t capabilities, uint32_t min_width, uint32_t min_height)
{
    if (!builder || builder->status != UI_OK || index >= builder->nodes.size()) { return UI_ERROR_INVALID_ARGUMENT; }
    auto& node = builder->nodes[index].element;
    if ((node.kind != UI_DOCK_DOCUMENT && node.kind != UI_DOCK_TOOL) || capabilities > 7 || min_width < 32 || min_width > 4096 || min_height < 32 || min_height > 4096) {
        return builder->status = UI_ERROR_INVALID_ARGUMENT;
    }
    node.dock_capabilities = capabilities; node.dock_min_width = min_width; node.dock_min_height = min_height; return UI_OK;
}

namespace {
constexpr size_t max_resource = 9 * 1024 * 1024;

ui_result fail(ui_fleck_diagnostic& diagnostic, ui_result status)
{
    fleck::report({}, 0, status == UI_ERROR_OUT_OF_MEMORY ? UI_FLECK_RUNTIME : UI_FLECK_VALUE,
                  status == UI_ERROR_VERSION_MISMATCH
                      ? "Fleck SDK/resource version mismatch; regenerate the page with the current SDK fleckc"
                      : "Invalid or unsupported compiled Fleck page", diagnostic);
    return status;
}

ui_result install(ui_app* app, fleck::Page& page, ui_fleck_builder& builder,
                  ui_fleck_diagnostic& diagnostic, const ui_fleck_data_context* context, ui_model* model)
{
    if (builder.api_version != UI_API_VERSION) return fail(diagnostic, UI_ERROR_VERSION_MISMATCH);
    if (builder.status != UI_OK || builder.nodes.empty()) {
        return fail(diagnostic, builder.status == UI_OK ? UI_ERROR_INVALID_ARGUMENT : builder.status);
    }
    for (size_t i = builder.nodes.size() - 1; i > 0; --i) {
        auto& node = builder.nodes[i];
        std::reverse(node.element.children.begin(), node.element.children.end());
        auto& children = builder.nodes[node.parent].element.children;
        children.push_back(std::move(node.element));
    }
    std::reverse(builder.nodes.front().element.children.begin(), builder.nodes.front().element.children.end());
    page.root = std::move(builder.nodes.front().element);
    return fleck::install(app, std::move(page), {}, diagnostic, true, context, model);
}


}

ui_result ui_fleck_require_version(ui_fleck_builder* builder, uint32_t api_version)
{
    if (!builder) return UI_ERROR_INVALID_ARGUMENT;
    if (builder->status != UI_OK) return builder->status;
    if (!builder->nodes.empty() || builder->api_version) return UI_ERROR_INVALID_STATE;
    if (api_version != UI_API_VERSION) return builder->status = UI_ERROR_VERSION_MISMATCH;
    builder->api_version = api_version;
    return UI_OK;
}
ui_result ui_fleck_set_command_mode(ui_fleck_builder* builder, uint32_t index, ui_command_mode mode)
{
    if (!builder || builder->status != UI_OK || index >= builder->nodes.size() || mode < UI_COMMAND_DEFAULT || mode > UI_COMMAND_ASYNC) { return UI_ERROR_INVALID_ARGUMENT; }
    builder->nodes[index].element.async_command = mode == UI_COMMAND_ASYNC;
    return UI_OK;
}
ui_result ui_fleck_set_enabled(ui_fleck_builder* builder, uint32_t index, uint32_t enabled)
{
    if (!builder || builder->status != UI_OK || index >= builder->nodes.size() || enabled > 1) { return UI_ERROR_INVALID_ARGUMENT; }
    builder->nodes[index].element.enabled = enabled != 0;
    return UI_OK;
}

ui_result ui_fleck_add_binding(ui_fleck_builder* builder, uint32_t index, const ui_binding_options* options)
{
    if (!builder || builder->status != UI_OK || index >= builder->nodes.size() || !options) { return UI_ERROR_INVALID_ARGUMENT; }
    auto& element = builder->nodes[index].element;
    ui_node probe{};
    probe.kind = element.kind;
    if (binding_validate(probe, *options) != UI_OK) { return builder->status = UI_ERROR_INVALID_ARGUMENT; }
    const auto canonical = [](ui_property p) { return p == UI_PROPERTY_CONTENT ? UI_PROPERTY_TEXT : p; };
    const auto p = canonical(options->property);
    for (const auto& binding : element.bindings) { if (canonical(binding.property) == p) { return builder->status = UI_ERROR_INVALID_ARGUMENT; } }
    try {
        element.bindings.push_back({options->property, options->path, options->string_format ? options->string_format : "", options->mode, options->trigger});
        return UI_OK;
    } catch (const std::bad_alloc&) { return builder->status = UI_ERROR_OUT_OF_MEMORY; }
}

ui_result ui_fleck_add_node(ui_fleck_builder* builder, uint32_t parent, const ui_fleck_node_spec* spec)
{
    if (!builder) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (builder->status != UI_OK) {
        return builder->status;
    }
    if (builder->api_version != UI_API_VERSION) return builder->status = UI_ERROR_VERSION_MISMATCH;
    auto& nodes = builder->nodes;
    if (!spec || nodes.size() >= UI_FLECK_NO_PARENT ||
        (nodes.empty() ? parent != UI_FLECK_NO_PARENT : parent >= nodes.size()) ||
        spec->kind < UI_STACK_PANEL || spec->kind > UI_PATH ||
        spec->dock < UI_DOCK_LEFT || spec->dock > UI_DOCK_BOTTOM ||
        !visual_layout::valid_metric(UI_PROPERTY_SPACING,spec->spacing) || !visual_layout::valid_metric(UI_PROPERTY_PADDING,spec->padding) ||
        spec->has_value > 1 || spec->weight > 1024) {
        return builder->status = UI_ERROR_INVALID_ARGUMENT;
    }
    const unsigned depth = nodes.empty() ? 2 : nodes[parent].depth + 1;
    if (depth > 64) {
        return builder->status = UI_ERROR_INVALID_ARGUMENT;
    }
    try {
        fleck::Element node;
        node.kind = spec->kind;
        node.text = spec->text ? spec->text : "";
        node.id = spec->id ? spec->id : "";
        node.anchor = spec->anchor ? spec->anchor : "";
        node.value = spec->value;
        node.has_value = spec->has_value != 0;
        node.weight = spec->weight;
        node.spacing = spec->spacing;
        node.padding = spec->padding;
        node.dock = spec->dock;
        if (node.text.size() > 16384 || node.id.size() > 64 || node.anchor.size() > 64) {
            return builder->status = UI_ERROR_INVALID_ARGUMENT;
        }
        nodes.push_back({std::move(node), parent, depth});
        return UI_OK;
    } catch (const std::bad_alloc&) {
        return builder->status = UI_ERROR_OUT_OF_MEMORY;
    }
}

ui_result ui_app_build_fleck(ui_app* app, const char* title, uint32_t width, uint32_t height,
                             ui_fleck_build_fn build, ui_fleck_diagnostic* diagnostic)
{
    return ui_app_build_fleck_with_context(app, title, width, height, build, nullptr, diagnostic);
}

ui_result ui_app_build_fleck_with_context(ui_app* app, const char* title, uint32_t width, uint32_t height,
                                         ui_fleck_build_fn build, const ui_fleck_data_context* context,
                                         ui_fleck_diagnostic* diagnostic)
{
    return ui_app_build_fleck_with_model(app, title, width, height, build, nullptr, context, diagnostic);
}

ui_result ui_app_build_fleck_with_model(ui_app* app, const char* title, uint32_t width, uint32_t height,
    ui_fleck_build_fn build, ui_model* model, const ui_fleck_data_context* context, ui_fleck_diagnostic* diagnostic)
{
    ui_fleck_diagnostic local{};
    auto& result = diagnostic ? *diagnostic : local;
    result = {};
    if (const auto status = fleck::can_load(app); status != UI_OK) {
        return status;
    }
    if (!title || !build || width == 0 || height == 0 || width > 4096 || height > 4096) {
        return fail(result, UI_ERROR_INVALID_ARGUMENT);
    }
    try {
        fleck::Page page;
        page.title = title;
        page.width = static_cast<int>(width);
        page.height = static_cast<int>(height);
        ui_fleck_builder builder;
        if (const auto status = build(&builder); status != UI_OK) {
            return fail(result, status);
        }
        if (const auto status = fleck::can_load(app); status != UI_OK) {
            return status;
        }
        return install(app, page, builder, result, context, model);
    } catch (const std::bad_alloc&) {
        return fail(result, UI_ERROR_OUT_OF_MEMORY);
    }
}

std::string fleck::compile_resource(const Page& page)
{
    return compile_components(page);
}

ui_result fleck::check_resource_version(std::string_view bytes, ui_fleck_diagnostic& diagnostic)
{
    if (bytes.size() < 6 || !bytes.starts_with("FLECK") || bytes.size() > max_resource)
        return fail(diagnostic, UI_ERROR_INVALID_ARGUMENT);
    return static_cast<unsigned char>(bytes[5]) == UI_FLECK_RESOURCE_VERSION
        ? UI_OK : fail(diagnostic, UI_ERROR_VERSION_MISMATCH);
}

ui_result ui_app_load_fleck_resource(ui_app* app, const void* bytes, size_t size,
                                     ui_fleck_diagnostic* diagnostic)
{
    return ui_app_load_fleck_resource_with_context(app, bytes, size, nullptr, diagnostic);
}

ui_result ui_app_load_fleck_resource_with_context(ui_app* app, const void* bytes, size_t size,
                                                 const ui_fleck_data_context* context,
                                                 ui_fleck_diagnostic* diagnostic)
{
    return ui_app_load_fleck_resource_with_model(app, bytes, size, nullptr, context, diagnostic);
}

ui_result ui_app_load_fleck_resource_with_model(ui_app* app, const void* bytes, size_t size,
    ui_model* model, const ui_fleck_data_context* context, ui_fleck_diagnostic* diagnostic)
{
    ui_fleck_diagnostic local{};
    auto& result = diagnostic ? *diagnostic : local;
    result = {};
    if (const auto status = fleck::can_load(app); status != UI_OK) {
        return status;
    }
    if (!bytes || size > max_resource) {
        return fail(result, UI_ERROR_INVALID_ARGUMENT);
    }
    const std::string_view input(static_cast<const char*>(bytes), size);
    if (auto status = fleck::check_resource_version(input, result); status != UI_OK) return status;
    try { return fleck::install_components(app, fleck::read_components(input), model, context, result); }
    catch (const std::bad_alloc&) { return fail(result, UI_ERROR_OUT_OF_MEMORY); }
    catch (const std::exception& error) {
        fleck::report({}, 0, UI_FLECK_VALUE, error.what(), result);
        return UI_ERROR_INVALID_ARGUMENT;
    }
}
