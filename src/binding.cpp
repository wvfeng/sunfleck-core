#include "binding.h"
#include "components.h"
#include "ui_tree.h"
#include "utf8_text.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <cstring>
#include <charconv>

namespace {
ui_property canonical(ui_property property)
{
    return property == UI_PROPERTY_CONTENT ? UI_PROPERTY_TEXT : property;
}
using Value = BindingValue;
ui_binding_options describe(const PropertyBinding& binding)
{
    return {sizeof(ui_binding_options), 1, binding.property, binding.path.c_str(), binding.mode, binding.trigger, binding.format.c_str()};
}
void outcome(ui_node& node, const PropertyBinding& binding, ui_result status, const char* code, const char* phase, const char* message, const char* actual = nullptr)
{
    for (auto& active : node.bindings) {
        if (active.property == binding.property && active.model == binding.model && active.path == binding.path) {
            const bool invalid = status != UI_OK;
            if (active.invalid != invalid) {
                active.invalid = invalid;
                binding_app_dirty(node.owner);
            }
            break;
        }
    }
    if (status == UI_OK) { devtools::recover(node, binding.property, binding.path); }
    else { devtools::report(node, binding.model, describe(binding), status, code, phase, message, actual); }
}
ui_result read(ui_model& model, const ui_model::Property& property, Value& value, const char*& code)
{
    ++model.callbacks;
    ui_result result;
    try { result = property.get(property.context, &value.value); }
    catch (...) { result = UI_ERROR_INVALID_STATE; }
    --model.callbacks;
    code = "getter_failed";
    if (result != UI_OK) { return result; }
    code = value.value.kind != property.kind ? "type_mismatch" : "invalid_source_value";
    return binding_copy_value(value.value, value, property.kind);
}
bool text_target(const ui_node& node)
{
    return devtools::text_target(node.kind);
}
bool numeric_target(const ui_node& node)
{
    return devtools::numeric_target(node.kind);
}
bool boolean_target(const ui_node& node)
{
    return devtools::boolean_target(node.kind);
}
bool writable(const ui_node& node, ui_property property)
{
    return property == UI_PROPERTY_TEXT ? is_editor(node.kind)
        : property == UI_PROPERTY_VALUE && (boolean_target(node) || (numeric_target(node) && node.kind != UI_PROGRESS_BAR));
}
bool valid_format(std::string_view format)
{
    if (format.empty()) { return true; }
    const auto at = format.find("{0}");
    return format.size() <= 16384 && at != format.npos &&
        format.find_first_of("{}") == at && format.find_first_of("{}", at + 3) == format.npos;
}
const char* apply_error(const ui_node& node, ui_property property, ui_value_kind type)
{
    if (text_format::property(property)) return type == text_format::kind(property) ? "invalid_target_value" : "type_mismatch";
    if (visual_state::property(property)) return type == visual_state::kind(property) ? "invalid_target_value" : "type_mismatch";
    if (border::property(property)) return type == border::kind(property) ? "invalid_target_value" : "type_mismatch";
    if (visual_layout::scalar_metric(property)) return type==UI_VALUE_INT || type==UI_VALUE_DOUBLE ? "invalid_target_value" : "type_mismatch";
    if (property==UI_PROPERTY_WEIGHT) return type==UI_VALUE_INT ? "invalid_target_value" : "type_mismatch";
    if (visual_layout::dimension(property)) return type==UI_VALUE_LENGTH || type==UI_VALUE_INT || type==UI_VALUE_DOUBLE ? "invalid_target_value" : "type_mismatch";
    if (visual_layout::property(property)) return type != (property==UI_PROPERTY_MARGIN ? UI_VALUE_THICKNESS : visual_layout::numeric(property) ? UI_VALUE_INT : UI_VALUE_STRING) ? "type_mismatch" : "invalid_target_value";
    switch (canonical(property)) {
    case UI_PROPERTY_TEXT: return type == UI_VALUE_COMMAND ? "type_mismatch" : "invalid_target_value";
    case UI_PROPERTY_SOURCE: return type != UI_VALUE_STRING ? "type_mismatch" : "invalid_target_value";
    case UI_PROPERTY_IS_ENABLED: return type != UI_VALUE_BOOL ? "type_mismatch" : "invalid_target_value";
    case UI_PROPERTY_VALUE:
        return type != UI_VALUE_INT && type != UI_VALUE_DOUBLE && !(type == UI_VALUE_BOOL && boolean_target(node)) ? "type_mismatch" : "invalid_target_value";
    case UI_PROPERTY_HEIGHT: return type != UI_VALUE_INT ? "type_mismatch" : "invalid_target_value";
    case UI_PROPERTY_COMMAND: return type != UI_VALUE_COMMAND ? "type_mismatch" : "command_mismatch";
    case UI_PROPERTY_ITEMS_SOURCE: case UI_PROPERTY_DATA_CONTEXT: return type != UI_VALUE_MODEL ? "type_mismatch" : "invalid_target_value";
    default: return "invalid_binding";
    }
}
ui_result convert(ui_node& node, const PropertyBinding& binding, const Value& value, Value& target, bool validate = true)
{
    const auto& v = value.value;
    if (visual_layout::dimension(binding.property) || visual_layout::scalar_metric(binding.property)) {
        const auto normalized=visual_layout::normalize(binding.property,v);
        if (auto status=binding_copy_value(normalized,target,visual_layout::scalar_metric(binding.property)?UI_VALUE_DOUBLE:UI_VALUE_LENGTH); status!=UI_OK) return status;
        return validate?properties::validate(node,binding.property,target.value):UI_OK;
    }
    if (canonical(binding.property) != UI_PROPERTY_TEXT && binding.property != UI_PROPERTY_VALUE) target = value;
    if (binding.property == UI_PROPERTY_LAYOUT_TRANSFORM || binding.property == UI_PROPERTY_RENDER_TRANSFORM || binding.property == UI_PROPERTY_RENDER_TRANSFORM_ORIGIN || binding.property == UI_PROPERTY_OPACITY || visual_layout::property(binding.property) || properties::metric(binding.property) || border::property(binding.property) || visual_state::property(binding.property) || text_format::property(binding.property)) return validate ? properties::validate(node, binding.property, v) : UI_OK;
    switch (canonical(binding.property)) {
    case UI_PROPERTY_TEXT: {
        if (v.kind >= UI_VALUE_COMMAND) { return UI_ERROR_INVALID_ARGUMENT; }
        std::string text = v.kind == UI_VALUE_STRING ? value.text : v.kind == UI_VALUE_DOUBLE ? std::to_string(v.number)
            : v.kind == UI_VALUE_BOOL ? (v.integer ? "true" : "false") : std::to_string(v.integer);
        if (!binding.format.empty()) {
            const auto at = binding.format.find("{0}");
            text = binding.format.substr(0, at) + text + binding.format.substr(at + 3);
        }
        ui_binding_value converted{}; converted.kind = UI_VALUE_STRING; converted.string = text.c_str();
        if (auto status = binding_copy_value(converted, target, converted.kind); status != UI_OK) return status;
        break;
    }
    case UI_PROPERTY_IS_ENABLED:
        break;
    case UI_PROPERTY_VALUE: {
        if (v.kind != UI_VALUE_INT && v.kind != UI_VALUE_DOUBLE && !(boolean_target(node) && v.kind == UI_VALUE_BOOL)) { return UI_ERROR_INVALID_ARGUMENT; }
        const double number = v.kind == UI_VALUE_DOUBLE ? v.number : static_cast<double>(v.integer);
        // Existing native Value is integral; never silently truncate floating point data.
        if (number < 0 || number > (boolean_target(node) ? 1 : 100) || std::floor(number) != number) { return UI_ERROR_INVALID_ARGUMENT; }
        ui_binding_value converted{}; converted.kind = UI_VALUE_INT; converted.integer = static_cast<int64_t>(number);
        if (auto status = binding_copy_value(converted, target, converted.kind); status != UI_OK) return status;
        break;
    }
    case UI_PROPERTY_HEIGHT:
        if (v.kind != UI_VALUE_INT || v.integer < -1 || v.integer > 4096) return UI_ERROR_INVALID_ARGUMENT;
        break;
    case UI_PROPERTY_COMMAND:
        if (v.kind != UI_VALUE_COMMAND) { return UI_ERROR_INVALID_ARGUMENT; }
        break;
    case UI_PROPERTY_SOURCE:
        if (v.kind != UI_VALUE_STRING) return UI_ERROR_INVALID_ARGUMENT;
        break;
    case UI_PROPERTY_ITEMS_SOURCE: case UI_PROPERTY_DATA_CONTEXT:
        if (v.kind != UI_VALUE_MODEL) return UI_ERROR_INVALID_ARGUMENT;
        break;
    default: return UI_ERROR_INVALID_ARGUMENT;
    }
    return validate ? properties::validate(node, binding.property, target.value) : UI_OK;
}
ui_result apply(ui_node& node, const PropertyBinding& binding, Value& value)
{
    Value target;
    if (auto status = convert(node, binding, value, target, false); status != UI_OK) return status;
    struct Guard { ui_node& node; bool previous; ~Guard() { node.applying_binding = previous; } } guard{node, std::exchange(node.applying_binding, true)};
    return ui_node_set_property(&node, binding.property, &target.value);
}
struct DataMatch {
    ui_node* node;
    ui_node::TriggerSetter* trigger;
    bool matches;
};
ui_result prepare_data_match(ui_node& node, ui_node::TriggerSetter& trigger, const Value* actual, std::vector<DataMatch>& updates)
{
    bool matches = false;
    if (actual) {
        Value expected;
        if (auto status = binding_parse_value(trigger.data_value,actual->value.kind,expected); status != UI_OK) return status;
        matches = actual->equals(expected);
    }
    if (matches != trigger.data_matches) updates.push_back({&node,&trigger,matches});
    return UI_OK;
}
ui_result prepare_data_context(ui_node& node, ui_model* model, std::vector<DataMatch>& updates)
{
    for (auto& trigger : node.triggers) {
        if (trigger.data_path.empty()) continue;
        Value value;
        if (model) {
            const auto found = model->properties.find(trigger.data_path);
            if (found == model->properties.end()) return UI_ERROR_INVALID_ARGUMENT;
            const char* code = nullptr;
            if (auto status = read(*model,found->second,value,code); status != UI_OK) return status;
        }
        if (auto status = prepare_data_match(node,trigger,model ? &value : nullptr,updates); status != UI_OK) return status;
    }
    return UI_OK;
}
ui_result commit_data_matches(std::vector<DataMatch>& updates)
{
    for (auto& update : updates) update.trigger->data_matches = update.matches;
    ui_node* previous = nullptr;
    for (const auto& update : updates) {
        if (update.node == previous) continue;
        previous = update.node;
        if (auto status = properties::update_triggers(*previous); status != UI_OK) return status;
    }
    return UI_OK;
}
ui_result refresh(ui_model& model, const std::set<std::string>& paths)
{
    const bool all = paths.contains("");
    ui_result first = UI_OK;
    for (const auto& [path, property] : model.properties) {
        if (!all && !paths.contains(path)) { continue; }
        Value value;
        const char* code = nullptr;
        const auto status = read(model, property, value, code);
        if (status != UI_OK && first == UI_OK) { first = status; }
        // Context changes can reconnect the same node's other bindings. Keep
        // only these rare targets in an owned snapshot before changing them.
        std::vector<std::pair<ui_node*, PropertyBinding>> contexts;
        if (model.has_context_targets) for (auto& node : binding_app_nodes(model.owner))
            for (const auto& binding : node->bindings)
                if (binding.property == UI_PROPERTY_DATA_CONTEXT && binding.model == &model && binding.path == path)
                    contexts.emplace_back(node.get(), binding);
        for (auto& [node, binding] : contexts) {
            if (std::none_of(node->bindings.begin(), node->bindings.end(), [&](const auto& current) {
                return current.property == UI_PROPERTY_DATA_CONTEXT && current.model == &model && current.path == path;
            })) continue;
            const auto applied = status == UI_OK ? apply(*node, binding, value) : status;
            outcome(*node, binding, applied, status == UI_OK ? apply_error(*node, binding.property, value.value.kind) : code,
                "refresh", "Context source cannot be applied to the target subtree");
            if (first == UI_OK && applied != UI_OK) first = applied;
        }
        for (auto& node : binding_app_nodes(model.owner)) {
            if (node->table && node->table->sort_path == path && node->table->sorted_models.contains(model.debug_id)) {
                node->table->sorted_revision = -1;
                queue_items_refresh(*node);
            }
            if (status == UI_OK && !node->triggers.empty() && binding_data_context(*node) == &model) {
                std::vector<DataMatch> matches;
                for (auto& trigger : node->triggers) {
                    if (trigger.data_path != path) continue;
                    const auto prepared = prepare_data_match(*node,trigger,&value,matches);
                    if (prepared != UI_OK && first == UI_OK) first = prepared;
                }
                if (auto updated = commit_data_matches(matches); updated != UI_OK && first == UI_OK) first = updated;
            }
            for (const auto& binding : node->bindings) {
                if (binding.property == UI_PROPERTY_DATA_CONTEXT || binding.model != &model || binding.path != path || binding.pending) { continue; }
                if (status != UI_OK) {
                    outcome(*node, binding, status, code, "read", "Model getter failed or returned an invalid value", code == std::string_view("getter_failed") ? nullptr : devtools::value_type(value.value.kind));
                    continue;
                }
                const auto applied = apply(*node, binding, value);
                outcome(*node, binding, applied, apply_error(*node, binding.property, value.value.kind), "refresh", "Source value cannot be applied to the target property");
                if (first == UI_OK && applied != UI_OK) { first = applied; }
            }
        }
    }
    if (model.collection && (all || paths.contains("Revision"))) {
        for (const auto& host : binding_app_nodes(model.owner)) {
            if (host->items_source == &model) queue_items_refresh(*host);
        }
        binding_app_dirty(model.owner);
    }
    const auto observers = model.observers;
    for (const auto& weak : observers) {
        if (auto observer = weak.lock()) {
            if (all) {
                const auto status = observer->notify("", false);
                if (first == UI_OK && status != UI_OK) first = status;
            } else for (const auto& path : paths) {
                const auto status = observer->notify(path, false);
                if (first == UI_OK && status != UI_OK) first = status;
            }
        }
    }
    std::erase_if(model.observers, [](const auto& weak) { return weak.expired(); });
    return first;
}
ui_result drain(ui_model& model)
{
    if (model.batch || model.notifying || model.callbacks) { return UI_OK; }
    model.notifying = true;
    struct Guard { ui_model& model; ~Guard() { model.notifying = false; } } guard{model};
    ui_result first = UI_OK;
    // Bound self-notification; callbacks cannot keep an event loop alive indefinitely.
    for (unsigned pass = 0; !model.pending.empty() && pass < 32; ++pass) {
        auto pending = std::exchange(model.pending, {});
        if (!pending.contains("")) {
            std::vector<std::string> changed(pending.begin(),pending.end());
            for (size_t i=0; i<changed.size(); ++i) {
                if (auto related=model.dependents.find(changed[i]); related!=model.dependents.end())
                    for (auto& path : related->second)
                        if (pending.insert(path).second) changed.push_back(path);
            }
        }
        const auto status = refresh(model, pending);
        if (first == UI_OK) { first = status; }
    }
    if (!model.pending.empty()) { model.pending.clear(); return UI_ERROR_INVALID_STATE; }
    return first;
}
}

ui_result binding_copy_value(const ui_binding_value& input, BindingValue& output, ui_value_kind expected)
{
    if (input.kind != expected || expected < UI_VALUE_STRING || expected > UI_VALUE_ENUM) return UI_ERROR_INVALID_ARGUMENT;
    if (expected == UI_VALUE_BOOL && input.integer != 0 && input.integer != 1) return UI_ERROR_INVALID_ARGUMENT;
    if (expected == UI_VALUE_DOUBLE && !std::isfinite(input.number)) return UI_ERROR_INVALID_ARGUMENT;
    if (expected == UI_VALUE_COLOR && (input.integer < 0 || uint64_t(input.integer) > UINT32_MAX)) return UI_ERROR_INVALID_ARGUMENT;
    if (expected == UI_VALUE_LENGTH && (input.integer < UI_LENGTH_PIXELS || input.integer > UI_LENGTH_INFINITY ||
        (input.integer == UI_LENGTH_PIXELS && (!std::isfinite(input.number) || input.number < 0)))) return UI_ERROR_INVALID_ARGUMENT;
    BindingValue next; next.value = input;
    if (const auto count = property_values::dimensions(expected)) {
        if (!input.geometry) return UI_ERROR_INVALID_ARGUMENT;
        std::memcpy(next.geometry.data(), input.geometry, count * sizeof(double));
        for (unsigned i = 0; i < count; ++i) {
            if (!std::isfinite(next.geometry[i]) || ((expected == UI_VALUE_SIZE || expected == UI_VALUE_CORNER_RADIUS) && next.geometry[i] < 0)) return UI_ERROR_INVALID_ARGUMENT;
        }
    }
    if (expected == UI_VALUE_STRING) {
        if (!input.string) return UI_ERROR_INVALID_ARGUMENT;
        size_t length = 0;
        while (length <= 16384 && input.string[length]) ++length;
        if (length > 16384) return UI_ERROR_INVALID_ARGUMENT;
        next.text.assign(input.string, length);
    }
    output = std::move(next);
    return UI_OK;
}
ui_result binding_read_value(ui_model& model, const std::string& path, BindingValue& result)
{
    auto property = model.properties.find(path);
    if (property == model.properties.end()) return UI_ERROR_INVALID_ARGUMENT;
    const char* diagnostic = nullptr;
    return read(model,property->second,result,diagnostic);
}

bool binding_path(std::string_view path)
{
    if (path.empty() || path.size() > 128) { return false; }
    bool start = true;
    for (char c : path) {
        if (c == '.') { if (start) { return false; } start = true; continue; }
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (!start && c >= '0' && c <= '9'))) { return false; }
        start = false;
    }
    return !start;
}
ui_result binding_validate(ui_node& node, const ui_binding_options& options, const char** diagnostic)
{
    if (diagnostic) { *diagnostic = "Invalid target property, binding mode or format"; }
    if (options.struct_size != sizeof(options) || options.version != 1 || !options.path || !binding_path(options.path) ||
        !properties::valid(options.property) || options.mode < UI_BIND_ONE_WAY ||
        options.mode > UI_BIND_DEFAULT || options.trigger < UI_UPDATE_PROPERTY_CHANGED || options.trigger > UI_UPDATE_LOST_FOCUS ||
        !valid_format(options.string_format ? options.string_format : "")) { return UI_ERROR_INVALID_ARGUMENT; }
    const auto p = canonical(options.property);
    ui_property_descriptor descriptor{}; descriptor.struct_size = sizeof(descriptor); descriptor.version = 1;
    if (ui_property_describe(node.kind, options.property, &descriptor) != UI_OK || !(descriptor.flags & UI_PROPERTY_BINDABLE)) return UI_ERROR_INVALID_ARGUMENT;
    if (options.mode == UI_BIND_TWO_WAY && !writable(node, p)) {
        if (diagnostic) {
            *diagnostic = options.property == UI_PROPERTY_CONTENT ? "Button.Content does not support TwoWay; use OneWay for model changes"
                : "TwoWay requires an editable Text or Value property; use OneWay for display";
        }
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (options.trigger == UI_UPDATE_LOST_FOCUS && !writable(node, p)) {
        if (diagnostic) { *diagnostic = "LostFocus requires an editable Text or Value property"; }
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (options.string_format && *options.string_format && (p != UI_PROPERTY_TEXT || options.mode == UI_BIND_TWO_WAY || (options.mode == UI_BIND_DEFAULT && is_editor(node.kind)))) {
        if (diagnostic) { *diagnostic = "StringFormat requires a OneWay or OneTime Text/Content binding"; }
        return UI_ERROR_INVALID_ARGUMENT;
    }
    return UI_OK;
}
void binding_detach(ui_node& node, ui_property property)
{
    if (!node.applying_binding) {
        for (const auto& binding : node.declared_bindings) {
            if (canonical(binding.property) == canonical(property)) { devtools::recover(node, property, binding.path); }
        }
        std::erase_if(node.bindings, [&](const auto& b) {
            if (canonical(b.property) != canonical(property)) return false;
            if (b.invalid) binding_app_dirty(node.owner);
            return true;
        });
        std::erase_if(node.declared_bindings, [&](const auto& b) { return canonical(b.property) == canonical(property); });
    }
}
ui_result ui_node_clear_binding(ui_node* node, ui_property property)
{
    if (!node || !properties::valid(property)) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = binding_app_check(node->owner); status != UI_OK) { return status; }
    binding_detach(*node, property);
    if (property == UI_PROPERTY_DATA_CONTEXT) node->data_context_edited = true;
    return UI_OK;
}
namespace {
ui_result collection_access(ui_model* model, bool edit) {
    if (!model) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = binding_app_check(model->owner); status != UI_OK) return status;
    if (!model->collection || (edit && (model->collection->read_only || model->notifying || model->callbacks))) return UI_ERROR_INVALID_STATE;
    return UI_OK;
}
}

ui_result ui_model_enable_text_collection(ui_model* model) {
    if (!model) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = binding_app_check(model->owner); status != UI_OK) return status;
    if (model->collection) return UI_OK;
    if (model->notifying || model->callbacks || model->properties.size() > 510 ||
        model->properties.contains("Count") || model->properties.contains("Revision")) return UI_ERROR_INVALID_STATE;
    try {
        auto collection = std::make_unique<TextCollection>();
        decltype(model->properties) paths;
        auto count = +[](void* data, ui_binding_value* result) -> ui_result {
            result->kind = UI_VALUE_INT;
            result->integer = static_cast<TextCollection*>(data)->items.size();
            return UI_OK;
        };
        auto revision = +[](void* data, ui_binding_value* result) -> ui_result {
            result->kind = UI_VALUE_INT;
            result->integer = static_cast<TextCollection*>(data)->revision;
            return UI_OK;
        };
        paths.emplace("Count", ui_model::Property{UI_VALUE_INT,count,nullptr,collection.get()});
        paths.emplace("Revision", ui_model::Property{UI_VALUE_INT,revision,nullptr,collection.get()});
        model->properties.merge(paths);
        model->collection = std::move(collection);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

namespace {
std::vector<CollectionItem> project_items(const ui_model* source, ui_collection_sort sort, const std::string& filter)
{
    std::vector<CollectionItem> result;
    if (!source) return result;
    for (const auto& item : source->collection->items)
        if (item.text.find(filter) != std::string::npos) result.push_back(item);
    if (sort != UI_COLLECTION_SOURCE_ORDER) {
        std::stable_sort(result.begin(),result.end(),[sort](const auto& left,const auto& right){
            return sort == UI_COLLECTION_ASCENDING ? left.text < right.text : right.text < left.text;
        });
    }
    return result;
}
ui_result publish_view(ui_model& model, std::vector<CollectionItem> items, bool grouped)
{
    auto& view = *model.collection;
    std::vector<TextCollection::Group> groups;
    if (grouped) {
        std::map<std::string, size_t> indices;
        std::vector<std::vector<CollectionItem>> buckets;
        for (auto& item : items) {
            auto [group, inserted] = indices.try_emplace(item.text, buckets.size());
            if (inserted) buckets.emplace_back();
            buckets[group->second].push_back(std::move(item));
        }
        items.clear();
        for (auto& bucket : buckets) {
            groups.push_back({static_cast<uint32_t>(items.size()), static_cast<uint32_t>(bucket.size())});
            for (auto& item : bucket) items.push_back(std::move(item));
        }
    }
    std::set<std::string> changed{"Revision"};
    if (view.items.size() != items.size()) changed.emplace("Count");
    view.items.swap(items);
    view.groups.swap(groups);
    view.group_by_label = grouped;
    ++view.revision;
    model.pending.merge(changed);
    return drain(model);
}
}
ui_result ui_collection_set_view(ui_model* model, ui_model* source, ui_collection_sort sort, const char* filter)
{
    if (!model || !source || source->owner != model->owner || !filter ||
        sort < UI_COLLECTION_SOURCE_ORDER || sort > UI_COLLECTION_DESCENDING) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = collection_access(source,false); status != UI_OK) return status;
    if (model->callbacks || model->notifying || source->notifying || source->callbacks) return UI_ERROR_INVALID_STATE;
    if (model->collection && !model->collection->read_only && model->collection->revision) return UI_ERROR_INVALID_STATE;
    for (auto* ancestor = source; ancestor; ancestor = ancestor->collection->source)
        if (ancestor == model) return UI_ERROR_INVALID_ARGUMENT;
    try {
        std::string match(filter);
        (void)utf8_text::decode(match);
        auto result = project_items(source,sort,match);
        auto subscription = std::make_shared<ModelObserver>();
        subscription->notify = [model,source](const std::string& path,bool destroyed) {
            auto& view = *model->collection;
            if (view.source != source || (!path.empty() && path != "Revision")) return UI_OK;
            if (destroyed) { view.source = nullptr; view.subscription.reset(); }
            return publish_view(*model,project_items(view.source,view.sort,view.filter),view.group_by_label);
        };
        source->observers.reserve(source->observers.size()+1);
        if (auto status = ui_model_enable_text_collection(model); status != UI_OK) return status;
        auto& view = *model->collection;
        // IDs belong to their source. Changing the source starts a new selection domain.
        if (view.source != source)
            for (auto& host : binding_app_nodes(model->owner))
                if (host->items_source == model) host->realized_source_id = 0;
        source->observers.push_back(subscription);
        view.subscription = std::move(subscription);
        view.source = source;
        view.sort = sort;
        view.filter = std::move(match);
        view.read_only = true;
        return publish_view(*model,std::move(result),view.group_by_label);
    } catch (const std::range_error&) { return UI_ERROR_INVALID_ARGUMENT; }
      catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

ui_result ui_collection_set_group_by_label(ui_model* model, uint32_t enabled) {
    if (enabled > 1) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = collection_access(model,false); status != UI_OK) return status;
    auto& view = *model->collection;
    if (!view.read_only || model->callbacks || model->notifying) return UI_ERROR_INVALID_STATE;
    if (view.group_by_label == (enabled != 0)) return UI_OK;
    try { return publish_view(*model,project_items(view.source,view.sort,view.filter),enabled != 0); }
    catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_collection_group_count(ui_model* model, uint32_t* count) {
    if (!count) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = collection_access(model,false); status != UI_OK) return status;
    *count = static_cast<uint32_t>(model->collection->groups.size());
    return UI_OK;
}
ui_result ui_collection_group(ui_model* model, uint32_t index, const char** key, uint32_t* first, uint32_t* count) {
    if (!key || !first || !count) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = collection_access(model,false); status != UI_OK) return status;
    auto& view = *model->collection;
    if (index >= view.groups.size()) return UI_ERROR_INVALID_ARGUMENT;
    const auto group = view.groups[index];
    *first = group.first;
    *count = group.count;
    *key = view.items[group.first].text.c_str();
    return UI_OK;
}

ui_result ui_collection_size(ui_model* model, uint32_t* count) {
    if (!count) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = collection_access(model,false); status != UI_OK) return status;
    *count = static_cast<uint32_t>(model->collection->items.size());
    return UI_OK;
}

ui_result ui_collection_item(ui_model* model, uint32_t index, uint64_t* identity, const char** text) {
    if (!identity || !text) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = collection_access(model,false); status != UI_OK) return status;
    const auto& items = model->collection->items;
    if (index >= items.size()) return UI_ERROR_INVALID_ARGUMENT;
    *identity = items[index].identity;
    *text = items[index].text.c_str();
    return UI_OK;
}

ui_result ui_collection_item_model(ui_model* model, uint32_t index, ui_model** context) {
    if (!context) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = collection_access(model,false); status != UI_OK) return status;
    if (index >= model->collection->items.size()) return UI_ERROR_INVALID_ARGUMENT;
    *context = model->collection->items[index].model;
    return UI_OK;
}

ui_result ui_collection_splice(ui_model* model, uint32_t index, uint32_t removed,
                               const char* const* inserted, uint32_t count) {
    return ui_collection_splice_models(model,index,removed,inserted,nullptr,count);
}

ui_result ui_collection_splice_models(ui_model* model, uint32_t index, uint32_t removed,
                               const char* const* inserted, ui_model* const* contexts, uint32_t count) {
    if (auto status = collection_access(model,true); status != UI_OK) return status;
    auto& collection = *model->collection;
    auto& items = collection.items;
    if (index > items.size() || removed > items.size()-index || (count && !inserted) ||
        items.size()-removed > UINT32_MAX-count) return UI_ERROR_INVALID_ARGUMENT;
    if (!removed && !count) return UI_OK;
    if (collection.revision == INT64_MAX || UINT64_MAX-collection.next_identity < count) return UI_ERROR_INVALID_STATE;
    try {
        std::vector<CollectionItem> added;
        added.reserve(count);
        for (uint32_t i=0; i<count; ++i) {
            if (!inserted[i]) return UI_ERROR_INVALID_ARGUMENT;
            (void)utf8_text::decode(inserted[i]);
            auto* context = contexts ? contexts[i] : nullptr;
            if (context && context->owner != model->owner) return UI_ERROR_INVALID_ARGUMENT;
            added.push_back({collection.next_identity+i,inserted[i],context});
        }
        std::set<std::string> changes{"Revision"};
        if (removed != count) changes.emplace("Count");
        const size_t required = items.size()-removed+count;
        if (required > items.capacity())
            items.reserve(std::max(required, items.capacity() + items.capacity()/2));
        auto at = items.erase(items.begin()+index,items.begin()+index+removed);
        items.insert(at,std::make_move_iterator(added.begin()),std::make_move_iterator(added.end()));
        collection.next_identity += count;
        ++collection.revision;
        model->pending.merge(changes);
        return drain(*model);
    } catch (const std::range_error&) { return UI_ERROR_INVALID_ARGUMENT; }
      catch (const std::length_error&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

ui_result ui_collection_move(ui_model* model, uint32_t index, uint32_t count, uint32_t destination) {
    if (auto status = collection_access(model,true); status != UI_OK) return status;
    auto& collection = *model->collection;
    auto& items = collection.items;
    if (index > items.size() || count > items.size()-index || destination > items.size()-count)
        return UI_ERROR_INVALID_ARGUMENT;
    if (!count || index == destination) return UI_OK;
    if (collection.revision == INT64_MAX) return UI_ERROR_INVALID_STATE;
    try {
        std::set<std::string> changes{"Revision"};
        auto begin = items.begin();
        if (destination < index) std::rotate(begin+destination,begin+index,begin+index+count);
        else std::rotate(begin+index,begin+index+count,begin+destination+count);
        ++collection.revision;
        model->pending.merge(changes);
        return drain(*model);
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_model_add_property(ui_model* model, const ui_model_property* property)
{
    if (!model || !property || property->struct_size != sizeof(*property) || property->version != 1 || !property->path ||
        !binding_path(property->path) || !property->get || property->kind < UI_VALUE_STRING || property->kind > UI_VALUE_ENUM) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = binding_app_check(model->owner); status != UI_OK) { return status; }
    if (model->callbacks || model->notifying || model->properties.size() >= 512) { return UI_ERROR_INVALID_STATE; }
    try {
        return model->properties.emplace(property->path, ui_model::Property{property->kind, property->get, property->set, property->context}).second ? UI_OK : UI_ERROR_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_model_notify_property_changed(ui_model* model, const char* path)
{
    if (!model) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = binding_app_check(model->owner); status != UI_OK) { return status; }
    if (path && *path && !model->properties.contains(path)) { return UI_ERROR_INVALID_ARGUMENT; }
    try {
        model->pending.insert(path ? path : "");
        return drain(*model);
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_model_begin_update(ui_model* model)
{
    if (!model) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = binding_app_check(model->owner); status != UI_OK) { return status; }
    if (model->batch >= 64) { return UI_ERROR_INVALID_STATE; }
    ++model->batch;
    return UI_OK;
}
ui_result ui_model_end_update(ui_model* model)
{
    if (!model) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = binding_app_check(model->owner); status != UI_OK) { return status; }
    if (!model->batch) { return UI_ERROR_INVALID_STATE; }
    --model->batch;
    try { return drain(*model); } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
namespace {
struct NestedMember {
    ui_model* root;
    std::string path;
    std::vector<std::string> segments;
    std::vector<std::shared_ptr<ModelObserver>> subscriptions;
    ui_model* leaf = nullptr;
    BindingValue cached;
    ui_value_kind kind = UI_VALUE_STRING;
    bool writable = false;

    ui_result resolve(bool initial) {
        ui_model* current = root;
        std::vector<std::shared_ptr<ModelObserver>> next;
        for (size_t i=0; i<segments.size(); ++i) {
            if (!current) break;
            const auto& name=segments[i];
            auto found=current->properties.find(name);
            if (found==current->properties.end()) return UI_ERROR_INVALID_ARGUMENT;
            if (i) {
                auto observer=std::make_shared<ModelObserver>();
                observer->notify=[this,name](const std::string& changed,bool destroyed){
                    if (destroyed) { leaf=nullptr; return UI_OK; }
                    if (changed.empty() || changed==name) return ui_model_notify_property_changed(root,path.c_str());
                    return UI_OK;
                };
                std::erase_if(current->observers, [](const auto& weak){ return weak.expired(); });
                current->observers.push_back(observer);
                next.push_back(std::move(observer));
            }
            if (i+1==segments.size()) {
                if (initial) { kind=found->second.kind; writable=found->second.set!=nullptr; }
                else if (kind!=found->second.kind) return UI_ERROR_INVALID_ARGUMENT;
                break;
            }
            BindingValue link;
            if (found->second.kind!=UI_VALUE_MODEL) return UI_ERROR_INVALID_ARGUMENT;
            if (auto status=binding_read_value(*current,name,link);status!=UI_OK) return status;
            current=link.value.model;
            if (current && current->owner!=root->owner) return UI_ERROR_INVALID_ARGUMENT;
        }
        if (initial && !current) return UI_ERROR_INVALID_ARGUMENT;
        subscriptions.swap(next);
        leaf=current;
        return UI_OK;
    }
    static ui_result get(void* data,ui_binding_value* value) {
        auto& member=*static_cast<NestedMember*>(data);
        if (auto status=member.resolve(false);status!=UI_OK) return status;
        if (member.leaf) {
            if (auto status=binding_read_value(*member.leaf,member.segments.back(),member.cached);status!=UI_OK) return status;
        } else {
            member.cached={};
            member.cached.value.kind=member.kind;
            member.cached.repoint();
        }
        *value=member.cached.value;
        return UI_OK;
    }
    static ui_result set(void* data,const ui_binding_value* value) {
        auto& member=*static_cast<NestedMember*>(data);
        if (auto status=member.resolve(false);status!=UI_OK) return status;
        if (!member.leaf) return UI_ERROR_INVALID_STATE;
        auto& prop=member.leaf->properties.at(member.segments.back());
        if (!prop.set) return UI_ERROR_INVALID_STATE;
        ++member.leaf->callbacks;
        ui_result status;
        try { status=prop.set(prop.context,value); } catch (...) { status=UI_ERROR_INVALID_STATE; }
        --member.leaf->callbacks;
        return status==UI_OK ? ui_model_notify_property_changed(member.leaf,member.segments.back().c_str()) : status;
    }
};
ui_result ensure_model_path(ui_model& model,const std::string& path) {
    if (model.properties.contains(path) || path.find('.')==path.npos) return UI_OK;
    try {
        auto member=std::make_shared<NestedMember>();
        member->root=&model;
        member->path=path;
        size_t at=0;
        while (at<path.size()) {
            auto end=path.find('.',at);
            member->segments.push_back(path.substr(at,end-at));
            if (end==path.npos) break;
            at=end+1;
        }
        if (auto status=member->resolve(true);status!=UI_OK) return status;
        model.derived_properties.reserve(model.derived_properties.size()+1);
        auto& dependents=model.dependents[member->segments.front()];
        dependents.reserve(dependents.size()+1);
        std::string dependency=path;
        ui_model_property descriptor{sizeof(descriptor),1,path.c_str(),member->kind,NestedMember::get,
                                    member->writable ? NestedMember::set : nullptr,member.get()};
        if (auto status=ui_model_add_property(&model,&descriptor);status!=UI_OK) return status;
        dependents.push_back(std::move(dependency));
        model.derived_properties.push_back(std::move(member));
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
}
ui_result ui_model_add_converter(ui_model* model,const char* name,ui_value_kind output,ui_binding_convert convert,void* context)
{
    if (!model || !name || !binding_path(name) || !convert || output<UI_VALUE_STRING || output>UI_VALUE_ENUM) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status=binding_app_check(model->owner);status!=UI_OK) return status;
    if (model->callbacks || model->notifying) return UI_ERROR_INVALID_STATE;
    try {
        return model->converters.emplace(name,ui_model::Converter{output,convert,context}).second ? UI_OK : UI_ERROR_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

ui_result ui_model_add_multi_binding(ui_model* model,const char* path,const char* converter,const char* const* inputs,uint32_t count)
{
    if (!model || !path || !binding_path(path) || !converter || !inputs || !count || count>64) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status=binding_app_check(model->owner);status!=UI_OK) return status;
    if (model->callbacks || model->notifying) return UI_ERROR_INVALID_STATE;
    auto method=model->converters.find(converter);
    if (method==model->converters.end()) return UI_ERROR_INVALID_ARGUMENT;
    struct Computed {
        ui_model* model;
        ui_model::Converter converter;
        std::vector<std::string> inputs;
        BindingValue result;
        static ui_result get(void* data,ui_binding_value* output) {
            auto& self=*static_cast<Computed*>(data);
            std::vector<BindingValue> owned(self.inputs.size());
            std::vector<ui_binding_value> args;
            args.reserve(owned.size());
            for (size_t i=0;i<owned.size();++i) {
                if (auto status=binding_read_value(*self.model,self.inputs[i],owned[i]);status!=UI_OK) return status;
                args.push_back(owned[i].value);
            }
            ui_binding_value result{};
            if (auto status=self.converter.convert(self.converter.context,args.data(),static_cast<uint32_t>(args.size()),&result);status!=UI_OK) return status;
            if (auto status=binding_copy_value(result,self.result,self.converter.output);status!=UI_OK) return status;
            *output=self.result.value;
            return UI_OK;
        }
    };
    try {
        auto state=std::make_shared<Computed>();
        state->model=model;
        state->converter=method->second;
        std::vector<std::string> specification{converter};
        for (uint32_t i=0;i<count;++i) {
            if (!inputs[i] || !binding_path(inputs[i]) || std::string_view(inputs[i])==path) return UI_ERROR_INVALID_ARGUMENT;
            state->inputs.emplace_back(inputs[i]);
            specification.emplace_back(inputs[i]);
        }
        if (model->properties.contains(path)) {
            auto old=model->conversion_inputs.find(path);
            return old!=model->conversion_inputs.end() && old->second==specification ? UI_OK : UI_ERROR_INVALID_ARGUMENT;
        }
        for (const auto& input : state->inputs) {
            if (auto status=ensure_model_path(*model,input);status!=UI_OK) return status;
            if (!model->properties.contains(input)) return UI_ERROR_INVALID_ARGUMENT;
        }
        auto dependencies=model->dependents;
        auto conversions=model->conversion_inputs;
        for (const auto& input : state->inputs) dependencies[input].emplace_back(path);
        conversions.emplace(path,std::move(specification));
        model->derived_properties.reserve(model->derived_properties.size()+1);
        ui_model_property property{sizeof(property),1,path,method->second.output,Computed::get,nullptr,state.get()};
        if (auto status=ui_model_add_property(model,&property);status!=UI_OK) return status;
        model->derived_properties.push_back(std::move(state));
        model->dependents.swap(dependencies);
        model->conversion_inputs.swap(conversions);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

static ui_result ensure_node_path(ui_node& node,ui_model& model,const std::string& path)
{
    auto found=node.binding_conversions.find(path);
    if (found==node.binding_conversions.end()) return ensure_model_path(model,path);
    const auto& values=found->second;
    const char* inputs[64];
    for (size_t i=1; i<values.size(); ++i) inputs[i-1]=values[i].c_str();
    return ui_model_add_multi_binding(&model,path.c_str(),values.front().c_str(),inputs,static_cast<uint32_t>(values.size()-1));
}

void binding_element_changed(ui_node& node, ui_property property)
{
    if (!node.element_property_source) return;
    auto model = node.element_property_source->model.lock();
    if (!model) return;
    const char* name = properties::name(properties::canonical(property));
    if (model->properties.contains(name)) binding_app_error(node.owner,ui_model_notify_property_changed(model.get(),name));
}

ui_result ui_node_bind_element_property(ui_node* target,ui_node* source,const ui_binding_options* options)
{
    if (!target || !source || source==target || target->owner!=source->owner || !options || !options->path)
        return UI_ERROR_INVALID_ARGUMENT;
    if (auto status=binding_app_check(target->owner);status!=UI_OK) return status;
    if (auto status=binding_validate(*target,*options);status!=UI_OK) return status;
    ui_property property;
    if (!properties::find(options->path,property)) return UI_ERROR_INVALID_ARGUMENT;
    property=properties::canonical(property);
    ui_property_descriptor descriptor{sizeof(descriptor),1};
    if (auto status=ui_property_describe(source->kind,property,&descriptor);status!=UI_OK) return status;
    try {
        auto* upstream=source;
        auto path=property;
        while (upstream) {
            if (upstream==target && path==properties::canonical(options->property)) return UI_ERROR_INVALID_ARGUMENT;
            auto link=std::find_if(upstream->bindings.begin(),upstream->bindings.end(),[&](const auto& b){
                return b.property==path && b.element_source;
            });
            if (link==upstream->bindings.end()) break;
            auto owner=link->element_source->element_owner.lock();
            if (!owner || !properties::find(link->path,path)) break;
            upstream=owner->node;
        }
        auto& anchor=source->element_property_source;
        if (!anchor) anchor=std::make_shared<ElementPropertySource>();
        anchor->node=source;
        auto model=anchor->model.lock();
        if (!model) {
            model=std::make_shared<ui_model>();
            model->owner=source->owner;
            model->debug_id=devtools::next_id(source->owner);
            model->element_owner=anchor;
            anchor->model=model;
        }
        const auto* name=properties::name(property);
        if (!model->properties.contains(name)) {
            struct Slot {
                std::weak_ptr<ElementPropertySource> owner;
                ui_property property;
                static ui_result get(void* ctx,ui_binding_value* value) {
                    auto& slot=*static_cast<Slot*>(ctx);
                    auto owner=slot.owner.lock();
                    return owner ? ui_node_get_property(owner->node,slot.property,value) : UI_ERROR_INVALID_STATE;
                }
                static ui_result set(void* ctx,const ui_binding_value* value) {
                    auto& slot=*static_cast<Slot*>(ctx);
                    auto owner=slot.owner.lock();
                    return owner ? ui_node_set_property(owner->node,slot.property,value) : UI_ERROR_INVALID_STATE;
                }
            };
            auto slot=std::make_shared<Slot>(Slot{anchor,property});
            model->derived_properties.reserve(model->derived_properties.size()+1);
            ui_model_property member{sizeof(member),1,name,descriptor.kind,Slot::get,Slot::set,slot.get()};
            if (auto status=ui_model_add_property(model.get(),&member);status!=UI_OK) return status;
            model->derived_properties.push_back(std::move(slot));
        }
        auto binding=*options;
        binding.path=name;
        if (auto status=ui_node_bind_property(target,model.get(),&binding);status!=UI_OK) return status;
        for (auto& item : target->bindings) if (item.model==model.get()) item.element_source=model;
        for (auto& item : target->declared_bindings) if (item.model==model.get()) item.element_source=model;
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

ui_result ui_node_bind_property(ui_node* node, ui_model* model, const ui_binding_options* options)
{
    if (!node || !options || options->struct_size != sizeof(*options) || options->version != 1) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = binding_app_check(node->owner); status != UI_OK) { return status; }
    const auto fail = [&](ui_result status, const char* code, const char* message) {
        devtools::report(*node, model, *options, status, code, "bind", message);
        return status;
    };
    if (!model) { return fail(UI_ERROR_INVALID_ARGUMENT, "model_missing", "Binding requires an explicit model"); }
    if (model->owner != node->owner) { return fail(UI_ERROR_INVALID_ARGUMENT, "model_owner", "Binding model belongs to another App"); }
    if (model->notifying || model->callbacks) { return fail(UI_ERROR_INVALID_STATE, "model_busy", "Binding model is executing a callback"); }
    const char* validation = nullptr;
    if (const auto status = binding_validate(*node, *options, &validation); status != UI_OK) { return fail(status, "invalid_binding", validation); }
    if (auto status=ensure_node_path(*node,*model,options->path);status!=UI_OK) return fail(status,"path_missing","Cannot resolve the model path or converter inputs");
    const auto found = model->properties.find(options->path);
    if (found == model->properties.end()) { return fail(UI_ERROR_INVALID_ARGUMENT, "path_missing", "Binding path is not exported by this model"); }
    try {
        const auto property = canonical(options->property);
        const auto mode = options->mode == UI_BIND_DEFAULT ? (writable(*node, property) ? UI_BIND_TWO_WAY : UI_BIND_ONE_WAY) : options->mode;
        if (mode == UI_BIND_TWO_WAY && !found->second.set) { return fail(UI_ERROR_INVALID_ARGUMENT, "read_only", "TwoWay binding requires a writable source property"); }
        if (mode == UI_BIND_TWO_WAY && property == UI_PROPERTY_TEXT && found->second.kind != UI_VALUE_STRING &&
            !(node->grid_cell_editor && found->second.kind <= UI_VALUE_DOUBLE)) { return fail(UI_ERROR_INVALID_ARGUMENT, "type_mismatch", "Unsupported source type for editable text"); }
        PropertyBinding binding{model, property, options->path, options->string_format ? options->string_format : "", mode, options->trigger};
        binding.model_id = model->debug_id;
        Value value;
        const char* code = nullptr;
        if (const auto status = read(*model, found->second, value, code); status != UI_OK) {
            devtools::report(*node, model, *options, status, code, "bind", "Model getter failed or returned an invalid value", code == std::string_view("getter_failed") ? nullptr : devtools::value_type(value.value.kind));
            return status;
        }
        node->bindings.reserve(node->bindings.size() + 1);
        node->declared_bindings.reserve(node->declared_bindings.size() + 1);
        auto declaration = binding;
        if (const auto status = apply(*node, binding, value); status != UI_OK) { return fail(status, apply_error(*node, property, value.value.kind), "Source type, value or command mode is incompatible with the target"); }
        binding_detach(*node, property);
        if (mode != UI_BIND_ONE_TIME) { node->bindings.push_back(std::move(binding)); }
        node->declared_bindings.push_back(std::move(declaration));
        if (property == UI_PROPERTY_DATA_CONTEXT) model->has_context_targets = true;
        properties::written(*node, property);
        devtools::recover(*node, property, options->path);
        if (property == UI_PROPERTY_TEXT) { node->text_source = nullptr; }
        return drain(*model);
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result binding_read_command(ui_node& node,const std::string& path,ui_command*& command)
{
    command = nullptr;
    auto* model = binding_data_context(node);
    if (!model) return UI_OK;
    auto found = model->properties.find(path);
    if (found == model->properties.end() || found->second.kind != UI_VALUE_COMMAND) return UI_ERROR_INVALID_ARGUMENT;
    BindingValue value;
    const char* code = nullptr;
    if (auto status = read(*model,found->second,value,code); status != UI_OK) return status;
    if (value.value.command && value.value.command->owner != node.owner) return UI_ERROR_INVALID_ARGUMENT;
    command = value.value.command;
    return UI_OK;
}

ui_model* binding_data_context(const ui_node& node)
{
    for (auto* current = &node; current; current = current->inheritance_parent())
        if (current->has_local_data_context) return current->local_data_context;
    return nullptr;
}
namespace {
thread_local bool changing_context = false;
thread_local bool committing_context = false;
thread_local bool releasing_model = false;
struct ContextUpdate {
    ui_node* node;
    PropertyBinding binding;
    Value value;
};
struct ContextPlan {
    std::vector<DataMatch> data_matches;
    std::vector<ContextUpdate> nodes;
    std::vector<components::ContextBindingUpdate> components;
};
ui_result prepare_node_context(ui_node& node, ui_model* model, ContextPlan& updates)
{
    if (auto status = prepare_data_context(node,model,updates.data_matches); status != UI_OK) return status;
    auto layout = node.layout;
    for (const auto& declaration : node.declared_bindings) {
        if (declaration.property == UI_PROPERTY_DATA_CONTEXT) continue;
        auto* source = declaration.uses_data_context ? model : declaration.model;
        const auto* prepared = declaration.mode == UI_BIND_ONE_TIME && !declaration.uses_data_context ? nullptr :
            components::prepared_binding_value(updates.components, source, declaration.path);
        if (source == declaration.model && !prepared) continue;
        ContextUpdate update{&node, declaration, {}};
        update.binding.model = source;
        update.binding.model_id = source ? source->debug_id : 0;
        update.binding.pending = false;
        if (source) {
            if (source->callbacks) return UI_ERROR_INVALID_STATE;
            if (auto status=ensure_node_path(node,*source,declaration.path);status!=UI_OK) return status;
            auto property = source->properties.find(declaration.path);
            if (property == source->properties.end() ||
                (declaration.mode == UI_BIND_TWO_WAY && (!property->second.set ||
                 (declaration.property == UI_PROPERTY_TEXT && property->second.kind != UI_VALUE_STRING &&
                  !(node.grid_cell_editor && property->second.kind <= UI_VALUE_DOUBLE)))))
                return UI_ERROR_INVALID_ARGUMENT;
            Value value;
            const char* code = nullptr;
            if (prepared) value = *prepared;
            else if (auto status = read(*source, property->second, value, code); status != UI_OK) return status;
            if (auto status = convert(node, update.binding, value, update.value); status != UI_OK) return status;
        }
        if (visual_layout::property(declaration.property)) {
            ui_property_descriptor descriptor{};
            descriptor.struct_size = sizeof(descriptor); descriptor.version = 1;
            if (auto status = ui_property_describe(node.kind, declaration.property, &descriptor); status != UI_OK) return status;
            auto slot = node.property_slots.find(declaration.property);
            if (slot == node.property_slots.end() || !slot->second.values.contains(UI_PRECEDENCE_ANIMATION)) {
                auto fallback = slot == node.property_slots.end() ? properties::Slot{} : slot->second;
                fallback.values.erase(UI_PRECEDENCE_LOCAL);
                const auto* selected = fallback.effective();
                const auto& next = source ? update.value.value : selected ? selected->value : descriptor.default_value;
                if (!visual_layout::set(layout, declaration.property, next) || !visual_layout::valid(layout)) return UI_ERROR_INVALID_ARGUMENT;
            }
        }
        updates.nodes.push_back(std::move(update));
    }
    return UI_OK;
}
ui_result prepare_context(ui_node& node, ui_model* inherited, ContextPlan& updates,
                          bool override_context = false, ui_model* proposed = nullptr, bool local = false,
                          ui_model* releasing = nullptr)
{
    auto* effective = override_context ? (local ? proposed : inherited) :
        node.has_local_data_context ? node.local_data_context : inherited;
    if (releasing && effective == releasing) effective = nullptr;
    if (!override_context) for (const auto& declaration : node.declared_bindings) {
        if (declaration.property != UI_PROPERTY_DATA_CONTEXT) continue;
        auto* source = declaration.uses_data_context ? inherited : declaration.model;
        if (releasing && source == releasing) source = nullptr;
        const auto* prepared = declaration.mode == UI_BIND_ONE_TIME && !declaration.uses_data_context ? nullptr :
            components::prepared_binding_value(updates.components, source, declaration.path);
        if (source == declaration.model && !prepared) continue;
        ContextUpdate update{&node, declaration, {}};
        update.binding.model = source;
        update.binding.model_id = source ? source->debug_id : 0;
        update.binding.pending = false;
        update.value.value.kind = UI_VALUE_MODEL;
        if (source) {
            if (source->callbacks) return UI_ERROR_INVALID_STATE;
            if (auto status=ensure_node_path(node,*source,declaration.path);status!=UI_OK) return status;
            const auto property = source->properties.find(declaration.path);
            if (property == source->properties.end()) return UI_ERROR_INVALID_ARGUMENT;
            Value value; const char* code = nullptr;
            if (prepared) value = *prepared;
            else if (auto status = read(*source, property->second, value, code); status != UI_OK) return status;
            if (auto status = convert(node, update.binding, value, update.value); status != UI_OK) return status;
        }
        effective = update.value.value.model;
        if (releasing && effective == releasing) effective = update.value.value.model = nullptr;
        updates.nodes.push_back(std::move(update));
    }
    if (node.component) {
        if (auto status = components::prepare_inherited_values(node, updates.components); status != UI_OK) return status;
        if (auto status = components::prepare_context_bindings(*node.component, effective, updates.components); status != UI_OK) return status;
    }
    if (auto status = prepare_node_context(node, effective, updates); status != UI_OK) return status;
    for (auto* child : node.children) {
        if (child->content_owner) continue;
        if (auto status = prepare_context(*child, effective, updates, false, nullptr, false, releasing); status != UI_OK) return status;
    }
    if (node.component) {
        auto visit = [&](auto& self, ui_node& visual) -> ui_result {
            if (visual.content_owner == &node) return prepare_context(visual, effective, updates, false, nullptr, false, releasing);
            if (visual.component) return UI_OK;
            for (auto* child : visual.children) if (auto status = self(self, *child); status != UI_OK) return status;
            return UI_OK;
        };
        for (auto* visual : node.children) if (auto status = visit(visit, *visual); status != UI_OK) return status;
    }
    return UI_OK;
}
ui_result commit_context(ContextPlan& updates)
{
    // Reserve before publishing any values. The prepared values own strings and
    // geometry; no user getter is called during the commit pass.
    for (auto& update : updates.nodes) update.node->bindings.reserve(update.node->declared_bindings.size());
    struct Guard { ~Guard() { committing_context = false; } } guard;
    committing_context = true;
    if (auto status = components::commit_context_bindings(updates.components); status != UI_OK) return status;
    for (auto& update : updates.nodes) {
        auto& node = *update.node;
        const auto property = update.binding.property;
        const bool previous = std::exchange(node.applying_binding, true);
        const auto status = update.binding.model || property == UI_PROPERTY_DATA_CONTEXT
            ? ui_node_set_property(&node, property, &update.value.value)
            : ui_node_clear_property(&node, property);
        node.applying_binding = previous;
        if (status != UI_OK) return status;
        std::erase_if(node.bindings, [&](const auto& binding) { return binding.property == property; });
        for (auto& declaration : node.declared_bindings)
            if (declaration.property == property) declaration = update.binding;
        if (update.binding.model && update.binding.mode != UI_BIND_ONE_TIME)
        {
            if (property == UI_PROPERTY_DATA_CONTEXT) update.binding.model->has_context_targets = true;
            node.bindings.push_back(std::move(update.binding));
        }
    }
    return commit_data_matches(updates.data_matches);
}
ui_result refresh_context(ui_node& root, ui_model* proposed, bool local, bool reevaluate_root)
{
    ContextPlan updates;
    if (auto status = prepare_context(root, root.inheritance_parent() ? binding_data_context(*root.inheritance_parent()) : nullptr, updates,
        !reevaluate_root, proposed, local); status != UI_OK) return status;
    return commit_context(updates);
}
ui_result change_context(ui_node* node, ui_model* model, bool local, bool reevaluate_root = false)
{
    if (!node || (model && model->owner != node->owner)) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = binding_app_check(node->owner); status != UI_OK) return status;
    if (changing_context || (model && model->callbacks)) return UI_ERROR_INVALID_STATE;
    struct Guard { ~Guard() { changing_context = false; } } guard;
    changing_context = true;
    ui_result status;
    try { status = refresh_context(*node, model, local, reevaluate_root); }
    catch (const std::bad_alloc&) { status = UI_ERROR_OUT_OF_MEMORY; }
    if (status == UI_OK && !reevaluate_root) {
        node->local_data_context = model;
        node->has_local_data_context = local;
    }
    return status;
}
}
ui_result binding_initialize_data_triggers(ui_node& node)
{
    std::vector<DataMatch> updates;
    if (auto status = prepare_data_context(node,binding_data_context(node),updates); status != UI_OK) return status;
    return commit_data_matches(updates);
}
ui_result binding_refresh_data_context(ui_node& root)
{
    return change_context(&root, root.local_data_context, root.has_local_data_context, true);
}
ui_result binding_set_data_context(ui_node* node, ui_model* model, bool has_value)
{
    if (committing_context) {
        node->local_data_context = model;
        node->has_local_data_context = has_value;
        return UI_OK;
    }
    return change_context(node, model, has_value);
}
ui_result binding_release_data_context(ui_model& model)
{
    if (changing_context) return UI_ERROR_INVALID_STATE;
    struct Guard { ~Guard() { changing_context = false; releasing_model = false; } } guard;
    changing_context = true;
    releasing_model = true;
    try {
        ContextPlan updates;
        for (auto& node : binding_app_nodes(model.owner)) {
            if (!node->parent)
                if (auto status = prepare_context(*node, nullptr, updates, false, nullptr, false, &model); status != UI_OK) return status;
        }
        if (auto status = commit_context(updates); status != UI_OK) return status;
        // Keep the local-value marker: destroying a local context creates an
        // explicit null boundary, rather than unexpectedly exposing a parent.
        for (auto& node : binding_app_nodes(model.owner)) {
            if (node->items_source == &model) {
                node->items_source = nullptr;
                queue_items_refresh(*node);
                node->realized_source_id = 0;
                binding_app_dirty(model.owner);
            }
            if (auto slot = node->property_slots.find(UI_PROPERTY_ITEMS_SOURCE); slot != node->property_slots.end())
                for (auto& [_, value] : slot->second.values)
                    if (value.value.model == &model) value.value.model = nullptr;
            if (node->local_data_context == &model) node->local_data_context = nullptr;
            if (auto slot = node->property_slots.find(UI_PROPERTY_DATA_CONTEXT); slot != node->property_slots.end())
                for (auto& [_, value] : slot->second.values)
                    if (value.value.model == &model) value.value.model = nullptr;
        }
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
bool binding_releasing_model() { return releasing_model; }
ui_result ui_node_set_data_context(ui_node* node, ui_model* model) {
    ui_binding_value value{}; value.kind = UI_VALUE_MODEL; value.model = model;
    return ui_node_set_property(node, UI_PROPERTY_DATA_CONTEXT, &value);
}
ui_result ui_node_clear_data_context(ui_node* node) {
    return ui_node_clear_property(node, UI_PROPERTY_DATA_CONTEXT);
}
ui_result ui_node_get_data_context(const ui_node* node, ui_model** model)
{
    if (!node || !model) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = binding_app_check(node->owner); status != UI_OK) return status;
    *model = binding_data_context(*node);
    return UI_OK;
}
ui_result ui_node_get_data_context_source(const ui_node* node, ui_property_source* source)
{
    return ui_node_get_property_source(node, UI_PROPERTY_DATA_CONTEXT, source);
}
ui_result ui_node_bind_context_property(ui_node* node, const ui_binding_options* options)
{
    if (!node || !options) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = binding_app_check(node->owner); status != UI_OK) return status;
    if (changing_context) return UI_ERROR_INVALID_STATE;
    if (auto status = binding_validate(*node, *options); status != UI_OK) return status;
    const auto property = canonical(options->property);
    auto* source = property == UI_PROPERTY_DATA_CONTEXT ? (node->inheritance_parent() ? binding_data_context(*node->inheritance_parent()) : nullptr) : binding_data_context(*node);
    if (auto* model = source) {
        if (auto status = ui_node_bind_property(node, model, options); status != UI_OK) return status;
        for (auto& binding : node->bindings) if (binding.property == property) binding.uses_data_context = true;
        for (auto& binding : node->declared_bindings) if (binding.property == property) binding.uses_data_context = true;
        return UI_OK;
    }
    try {
        PropertyBinding declaration{nullptr, property, options->path, options->string_format ? options->string_format : "",
            options->mode == UI_BIND_DEFAULT ? (writable(*node, property) ? UI_BIND_TWO_WAY : UI_BIND_ONE_WAY) : options->mode, options->trigger};
        declaration.uses_data_context = true;
        node->declared_bindings.reserve(node->declared_bindings.size() + 1);
        ui_binding_value empty{}; empty.kind = UI_VALUE_MODEL;
        if (auto status = property == UI_PROPERTY_DATA_CONTEXT ? ui_node_set_property(node, property, &empty) : ui_node_clear_property(node, property); status != UI_OK) return status;
        node->declared_bindings.push_back(std::move(declaration));
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_node_update_source(ui_node* node, ui_property property)
{
    if (!node) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = binding_app_check(node->owner); status != UI_OK) { return status; }
    auto found = std::find_if(node->bindings.begin(), node->bindings.end(), [&](const auto& b) { return b.property == canonical(property); });
    if (found == node->bindings.end() || found->mode != UI_BIND_TWO_WAY) { return UI_ERROR_INVALID_ARGUMENT; }
    const auto slot = node->property_slots.find(canonical(property));
    if (slot == node->property_slots.end() || !slot->second.values.contains(UI_PRECEDENCE_ANIMATION)) {
        properties::capture(*node, property);
    }
    auto* model = found->model;
    const auto binding = *found;
    const auto fail = [&](ui_result status, const char* code, const char* message) {
        outcome(*node, binding, status, code, "write", message);
        return status;
    };
    found->pending = false;
    if (model->callbacks || model->notifying) { return fail(UI_ERROR_INVALID_STATE, "model_busy", "Cannot write back during a model callback"); }
    const auto path = found->path;
    const auto source = model->properties.find(path);
    if (source == model->properties.end() || !source->second.set) { return fail(UI_ERROR_INVALID_STATE, "read_only", "Binding source is missing or read-only"); }
    try {
        Value current;
        const char* code = nullptr;
        if (const auto status = read(*model, source->second, current, code); status != UI_OK) {
            outcome(*node, binding, status, code, "write", "Cannot read the current source before writeback", code == std::string_view("getter_failed") ? nullptr : devtools::value_type(current.value.kind));
            return status;
        }
        ui_binding_value value{};
        value.kind = source->second.kind;
        std::string text;
        bool equal;
        Value parsed;
        if (canonical(property) == UI_PROPERTY_TEXT) {
            uint32_t size = 0;
            if (const auto status = ui_node_get_text(node, nullptr, &size); status != UI_OK) { return status; }
            text.resize(size);
            if (const auto status = ui_node_get_text(node, text.data(), &size); status != UI_OK) { return status; }
            text.resize(size - 1);
            if (text.find_first_of("\r\n") != std::string::npos && !binding_allows_line_breaks(*node)) {
                (void)apply(*node, binding, current);
                return fail(UI_ERROR_INVALID_ARGUMENT, "invalid_source_value", "A single-line editor cannot accept line breaks");
            }
            if (value.kind == UI_VALUE_STRING) {
                value.string = text.c_str();
                equal = text == current.text;
            } else {
                if (auto status = binding_parse_value(text, value.kind, parsed); status != UI_OK) {
                    (void)apply(*node, binding, current);
                    return fail(status, "invalid_source_value", "Cell text does not match the column's source type");
                }
                value = parsed.value;
                equal = current.equals(parsed);
            }
        } else {
            ui_binding_value base{};
            if (auto status = ui_node_get_base_property(node, property, &base); status != UI_OK) return status;
            value.integer = base.integer;
            value.number = static_cast<double>(base.integer);
            equal = value.kind == UI_VALUE_DOUBLE ? value.number == current.value.number : value.integer == current.value.integer;
        }
        if (!equal) {
            ++model->callbacks;
            ui_result status;
            try { status = source->second.set(source->second.context, &value); } catch (...) { status = UI_ERROR_INVALID_STATE; }
            --model->callbacks;
            if (status != UI_OK) {
                (void)apply(*node, binding, current);
                (void)drain(*model);
                return fail(status, "setter_failed", "Model setter rejected the value or raised an exception");
            }
        }
        outcome(*node, binding, UI_OK, nullptr, "write", nullptr);
        return ui_model_notify_property_changed(model, path.c_str());
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
bool binding_allows_line_breaks(const ui_node& node)
{
    for (const auto& binding : node.bindings) {
        if (binding.property != UI_PROPERTY_TEXT) { continue; }
        for (const auto& other : binding_app_nodes(node.owner)) {
            if (!is_editor(other->kind) || other->kind == UI_TEXT_AREA) { continue; }
            for (const auto& candidate : other->bindings) {
                if (candidate.property == UI_PROPERTY_TEXT && candidate.model == binding.model && candidate.path == binding.path) { return false; }
            }
        }
    }
    return true;
}
void binding_user_changed(ui_node& node, ui_property property)
{
    properties::capture(node, property);
    for (auto& binding : node.bindings) {
        if (binding.property != canonical(property) || binding.mode != UI_BIND_TWO_WAY) { continue; }
        if (binding.trigger == UI_UPDATE_LOST_FOCUS) { binding.pending = true; return; }
        binding_app_error(node.owner, ui_node_update_source(&node, property));
        return;
    }
}
void binding_flush_focus(ui_app* app)
{
    for (auto& node : binding_app_nodes(app)) {
        if (binding_node_focused(node.get())) { continue; }
        for (size_t i = 0; i < node->bindings.size(); ++i) {
            if (node->bindings[i].pending) { binding_app_error(app, ui_node_update_source(node.get(), node->bindings[i].property)); }
        }
    }
}

ui_result ui_model_add_text_source(ui_model* model, const char* path, ui_text_source* source)
{
    if (!model || !source || model->owner != source->owner || !path) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = binding_app_check(model->owner); status != UI_OK) { return status; }
    try {
        auto adapter = std::make_unique<ui_model::Adapter>();
        adapter->source = source;
        const ui_model_property property{sizeof(property), 1, path, UI_VALUE_STRING,
            [](void* context, ui_binding_value* value) -> ui_result {
                auto& adapter = *static_cast<ui_model::Adapter*>(context);
                if (!adapter.source) { return UI_ERROR_INVALID_STATE; }
                adapter.text = utf8_text::encode(adapter.source->value);
                *value = {}; value->kind = UI_VALUE_STRING; value->string = adapter.text.c_str(); return UI_OK;
            }, [](void* context, const ui_binding_value* value) -> ui_result {
                auto& adapter = *static_cast<ui_model::Adapter*>(context);
                return adapter.source ? ui_text_source_set(adapter.source, value->string) : UI_ERROR_INVALID_STATE;
            }, adapter.get()};
        model->adapters.reserve(model->adapters.size() + 1);
        source->property_observers.reserve(source->property_observers.size() + 1);
        std::pair<ui_model*, std::string> observer{model, path};
        if (const auto status = ui_model_add_property(model, &property); status != UI_OK) { return status; }
        model->adapters.push_back(std::move(adapter));
        source->property_observers.push_back(std::move(observer));
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_model_add_command(ui_model* model, const char* path, ui_command* command)
{
    if (!model || !command || command->owner != model->owner) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = binding_app_check(model->owner); status != UI_OK) { return status; }
    try {
        auto adapter = std::make_unique<ui_model::Adapter>();
        adapter->command = command;
        const ui_model_property property{sizeof(property), 1, path, UI_VALUE_COMMAND,
            [](void* context, ui_binding_value* value) -> ui_result {
                *value = {}; value->kind = UI_VALUE_COMMAND; value->command = static_cast<ui_model::Adapter*>(context)->command; return UI_OK;
            }, nullptr, adapter.get()};
        model->adapters.reserve(model->adapters.size() + 1);
        if (const auto status = ui_model_add_property(model, &property); status != UI_OK) { return status; }
        model->properties.at(path).command_modes = command->asynchronous() ? UI_COMMAND_SUPPORTS_ASYNC : UI_COMMAND_SUPPORTS_SYNC;
        model->adapters.push_back(std::move(adapter));
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
void binding_text_source_changed(ui_text_source& source)
{
    for (const auto& [model, path] : source.property_observers) {
        binding_app_error(source.owner, ui_model_notify_property_changed(model, path.c_str()));
    }
}

ui_result ui_value_parse(ui_value_kind kind, const char* utf8, ui_binding_value* value, double* geometry, uint32_t geometry_count) {
    if (!utf8 || !value) return UI_ERROR_INVALID_ARGUMENT;
    const auto count = property_values::dimensions(kind);
    if (count && (!geometry || geometry_count < count)) return UI_ERROR_INVALID_ARGUMENT;
    try {
        BindingValue parsed;
        const auto status = binding_parse_value(utf8, kind, parsed);
        if (status != UI_OK) return status;
        auto result = parsed.value;
        if (count) { std::memcpy(geometry, parsed.geometry.data(), count * sizeof(double)); result.geometry = geometry; }
        if (kind == UI_VALUE_STRING) result.string = utf8;
        *value = result;
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
