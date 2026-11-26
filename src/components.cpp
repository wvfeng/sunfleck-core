#include "components.h"
#include "dock_json.h"
#include "ui_tree.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {
bool parse_number(const std::string &text, double &value) {
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    input >> std::noskipws >> value;
    return !input.fail() && input.eof() && std::isfinite(value);
}
struct CallbackScope {
    ui_app *app;
    bool previous;
    explicit CallbackScope(ui_app *app)
        : app(app), previous(async_app_set_dispatching(app, true)) {}
    ~CallbackScope() { async_app_set_dispatching(app, previous); }
};
void require(ui_result result) {
    if (result != UI_OK) {
        throw std::range_error(
            "Component operation failed (binding, property or lifecycle)");
    }
}
ui_result check(ui_component *c) {
    return !c || c->destroyed || !c->host ? UI_ERROR_INVALID_ARGUMENT
                                          : binding_app_check(c->host->owner);
}
ui_result read(ui_model *model, const std::string &path, BindingValue &value) {
    if (!model) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    auto p = model->properties.find(path);
    if (p == model->properties.end()) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    ++model->callbacks;
    ui_result result;
    try {
        result = p->second.get(p->second.context, &value.value);
    } catch (...) {
        result = UI_ERROR_INVALID_STATE;
    }
    --model->callbacks;
    return result == UI_OK
               ? binding_copy_value(value.value, value, p->second.kind)
               : result;
}
bool animatable(ui_value_kind kind) {
    return kind == UI_VALUE_INT || kind == UI_VALUE_DOUBLE || kind == UI_VALUE_COLOR ||
        property_values::dimensions(kind) || kind == UI_VALUE_LENGTH;
}
uint32_t inferred_flags(ui_value_kind kind, bool read_only) {
    return uint32_t(UI_PROPERTY_BINDABLE) | (read_only ? 0u : uint32_t(UI_PROPERTY_TWO_WAY)) |
        (animatable(kind) ? uint32_t(UI_PROPERTY_ANIMATABLE) : 0u);
}
bool valid_metadata(const ui_component_property_metadata& metadata, ui_value_kind kind, bool read_only) {
    constexpr uint32_t supported = UI_PROPERTY_BINDABLE | UI_PROPERTY_TWO_WAY | UI_PROPERTY_ANIMATABLE |
        UI_PROPERTY_AFFECTS_MEASURE | UI_PROPERTY_AFFECTS_ARRANGE | UI_PROPERTY_AFFECTS_RENDER | UI_PROPERTY_AFFECTS_INPUT | UI_PROPERTY_INHERITS;
    return metadata.struct_size == sizeof(metadata) && metadata.version == 1 && !(metadata.flags & ~supported) &&
        (!(metadata.flags & UI_PROPERTY_ANIMATABLE) || animatable(kind)) &&
        (!(metadata.flags & UI_PROPERTY_TWO_WAY) || (!read_only && (metadata.flags & UI_PROPERTY_BINDABLE)));
}
ui_result effective_value(ui_component& c, const components::Property& property, BindingValue& effective) {
    const auto& metadata = property.metadata;
    if (!metadata.validate && !metadata.coerce) return UI_OK;
    try {
        CallbackScope scope(c.host->owner);
        if (metadata.coerce) {
            ui_binding_value coerced{};
            if (auto result = metadata.coerce(&c, &effective.value, &coerced, metadata.context); result != UI_OK) return result;
            if (auto result = binding_copy_value(coerced, effective, property.value.kind); result != UI_OK) return result;
        }
        if ((effective.value.kind == UI_VALUE_MODEL && effective.value.model && effective.value.model->owner != c.host->owner) ||
            (effective.value.kind == UI_VALUE_COMMAND && effective.value.command && effective.value.command->owner != c.host->owner)) return UI_ERROR_INVALID_ARGUMENT;
        return metadata.validate ? metadata.validate(&c, &effective.value, metadata.context) : UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (...) { return UI_ERROR_INVALID_STATE; }
}
const components::Property& default_property(const ui_component& c, const std::string& name) {
    return *std::find_if(c.type->properties.begin(), c.type->properties.end(), [&](const auto& p) { return p.name == name; });
}
struct PropertyChangeScope {
    bool& flag;
    bool previous;
    explicit PropertyChangeScope(ui_component& component)
        : flag(component.changing_property), previous(std::exchange(flag, true)) {}
    ~PropertyChangeScope() { flag = previous; }
};

struct PreparedCommit {
    using Connections = decltype(ui_component::connections);
    std::vector<Connections::node_type> connections;
    std::vector<std::pair<ui_component*, std::string>> changed;

    explicit PreparedCommit(const std::vector<components::ContextBindingUpdate>& updates);
    ui_result publish(std::vector<components::ContextBindingUpdate>& updates);
};
ui_result set(ui_component &c, const std::string &name,
              const ui_binding_value &value, bool from_binding,
              ui_value_precedence precedence = UI_PRECEDENCE_LOCAL, bool clear = false) {
    const auto found = c.values.find(name);
    if (found == c.values.end()) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    auto &target = found->second;
    if (c.changing_property) return UI_ERROR_INVALID_STATE;
    if (precedence < UI_PRECEDENCE_INHERITED || precedence > UI_PRECEDENCE_ANIMATION ||
        (precedence == UI_PRECEDENCE_INHERITED && (target.metadata.flags & UI_PROPERTY_INHERITS)) ||
        (precedence == UI_PRECEDENCE_ANIMATION && !(target.metadata.flags & UI_PROPERTY_ANIMATABLE))) return UI_ERROR_INVALID_ARGUMENT;
    BindingValue copied;
    if (binding_copy_value(value, copied, target.value.kind) != UI_OK) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if ((value.kind == UI_VALUE_COMMAND && value.command &&
         value.command->owner != c.host->owner) ||
        (value.kind == UI_VALUE_MODEL && value.model &&
         value.model->owner != c.host->owner)) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (target.read_only && from_binding) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    auto layers = target.layers;
    if (clear) layers.values.erase(precedence);
    else layers.values.insert_or_assign(precedence, copied);
    const auto* selected = layers.effective();
    BindingValue effective(selected ? *selected : default_property(c, name));
    {
        struct Guard { bool& flag; ~Guard() { flag = false; } } guard{c.changing_property};
        c.changing_property = true;
        if (auto status = effective_value(c, target, effective); status != UI_OK) return status;
    }
    std::vector<components::ContextBindingUpdate> updates(1);
    auto& update = updates.front();
    update.component = &c; update.name = name;
    update.layers = std::move(layers); update.effective = std::move(effective);
    update.disconnect = clear && precedence == UI_PRECEDENCE_LOCAL;
    if (target.metadata.flags & UI_PROPERTY_INHERITS)
        if (auto status = components::prepare_inherited_subtree(*c.host, updates); status != UI_OK) return status;
    PreparedCommit commit(updates);
    if (!from_binding && !clear && precedence == UI_PRECEDENCE_LOCAL) {
        const auto connected = c.connections.find(name);
        if (connected != c.connections.end() &&
            connected->second.mode == UI_BIND_TWO_WAY &&
            connected->second.model) {
            auto &link = connected->second;
            auto *model = link.model;
            const auto property = model->properties.find(link.path);
            if (property == model->properties.end() || !property->second.set ||
                model->callbacks || model->notifying) {
                return UI_ERROR_INVALID_STATE;
            }
            PropertyChangeScope writing_source(c);
            ++model->callbacks;
            ui_result result;
            try {
                result = property->second.set(property->second.context, &value);
            } catch (...) {
                result = UI_ERROR_INVALID_STATE;
            }
            --model->callbacks;
            if (result != UI_OK) {
                return result;
            }
            // Notify after the local value is committed; the cycle terminates
            // on equality.
        }
    }
    auto result = commit.publish(updates);
    if (!from_binding && !clear && precedence == UI_PRECEDENCE_LOCAL) {
        const auto connected = c.connections.find(name);
        if (connected != c.connections.end() &&
            connected->second.mode == UI_BIND_TWO_WAY &&
            connected->second.model) {
            const auto status = ui_model_notify_property_changed(
                connected->second.model, connected->second.path.c_str());
            if (result == UI_OK) {
                result = status;
            }
        }
    }
    return result;
}
ui_result get_value(void *context, ui_binding_value *result) {
    auto &value = *static_cast<ui_component::Value *>(context);
    *result = value.value;
    if (result->kind == UI_VALUE_STRING) {
        result->string = value.text.c_str();
    }
    return UI_OK;
}
ui_result set_value(void *context, const ui_binding_value *value) {
    auto &target = *static_cast<ui_component::Value *>(context);
    return set(*target.component, target.name, *value, false);
}
ui_model *source_for(ui_node &node, const fleck::Binding &binding,
                     ui_model *context) {
    if (binding.source == fleck::Binding::Context) {
        return context;
    }
    auto component = binding.source == fleck::Binding::Self && node.component
                         ? node.component
                         : node.component_scope;
    return component ? component->properties.get() : nullptr;
}
ui_result bind_node(ui_node &node, const fleck::Element &element, ui_model *page_model) {
    // Both component hosts and template children inherit through the node tree.
    // Self/TemplatedParent remain explicit references to component properties.
    for (int phase = 0; phase != 2; ++phase) for (const auto &binding : element.bindings) {
        const bool context_target = binding.property == UI_PROPERTY_DATA_CONTEXT || binding.target == "DataContext";
        if (context_target != (phase == 0)) continue;
        if (context_target && node.data_context_edited) continue;
        if (binding.source==fleck::Binding::NamedElement || binding.source==fleck::Binding::Ancestor) {
            auto selector=element.properties.find(".BindingSource."+std::to_string(binding.property));
            if (selector==element.properties.end()) return UI_ERROR_INVALID_ARGUMENT;
            ui_node* source=nullptr;
            if (binding.source==fleck::Binding::Ancestor) {
                for (auto* parent=node.inheritance_parent(); parent; parent=parent->inheritance_parent())
                    if (selector->second==devtools::control_name(parent->kind) ||
                        (parent->component && selector->second==parent->component->type->key.second)) { source=parent; break; }
            } else {
                auto* root=&node;
                while (root->parent) root=root->parent;
                auto find=[&](auto& self,ui_node* current)->ui_node* {
                    if (current->component_scope==node.component_scope && current->fleck_id==selector->second) return current;
                    for (auto* child : current->children) if (auto* found=self(self,child)) return found;
                    return nullptr;
                };
                source=find(find,root);
            }
            const ui_binding_options options{sizeof(options),1,binding.property,binding.path.c_str(),binding.mode,binding.trigger,binding.format.c_str()};
            if (auto status=ui_node_bind_element_property(&node,source,&options); status!=UI_OK) return status;
            continue;
        }
        if (!binding.target.empty() && !context_target) {
            if (!node.component || !binding.format.empty() || binding.trigger != UI_UPDATE_PROPERTY_CHANGED)
                return UI_ERROR_INVALID_ARGUMENT;
            ui_result status;
            if (binding.source == fleck::Binding::Context) {
                status = components::bind_context_property(*node.component, binding.target.c_str(), binding.path.c_str(), binding.mode);
            } else {
                ui_property_id id = 0;
                status = ui_node_resolve_property(&node, binding.target.c_str(), &id);
                if (status == UI_OK) {
                    const ui_registered_binding_options options{sizeof(options), 1, id, binding.path.c_str(), binding.mode,
                        binding.trigger, binding.format.c_str()};
                    status = ui_node_bind_registered_property(&node, source_for(node, binding, nullptr), &options);
                }
            }
            if (status != UI_OK) return status;
            continue;
        }
        const ui_binding_options options{sizeof(options), 1, context_target ? UI_PROPERTY_DATA_CONTEXT : binding.property,
            binding.path.c_str(), binding.mode, binding.trigger, binding.format.c_str()};
        bool has_context = node.component || node.component_scope;
        for (auto* ancestor = &node; !has_context && ancestor; ancestor = ancestor->inheritance_parent())
            has_context = ancestor->has_local_data_context;
        ui_result status;
        if (binding.source == fleck::Binding::Context && has_context && (!context_target || node.parent)) {
            status = ui_node_bind_context_property(&node, &options);
        } else {
            status = ui_node_bind_property(&node, source_for(node, binding, page_model), &options);
        }
        if (status != UI_OK) return status;
    }
    return UI_OK;
}
void expand_node(components::Registry &registry, fleck::Element &node,
                 std::vector<components::Key> &stack, unsigned depth,
                 unsigned &count) {
    try {
        if (depth > 64 || ++count > 16384) {
            throw std::range_error("Component expansion limit exceeded");
        }
        const bool button = fleck::native_template(node);
        if (!node.type_name.empty() || button) {
            const components::Key key = button ? components::Key{"urn:sunfleck:v1",devtools::control_name(node.kind)} :
                components::Key{node.type_namespace,node.type_name};
            if (std::find(stack.begin(), stack.end(), key) != stack.end()) {
                std::string chain = "Recursive component: ";
                for (const auto &item : stack) {
                    chain += item.second + " -> ";
                }
                throw std::range_error(chain + key.second);
            }
            auto definition = registry.documents.find(key);
            auto type = registry.types.find(key);
            if (definition != registry.documents.end() &&
                definition->second.document_kind == "UserControl") {
                if (!node.template_name.empty()) {
                    throw std::range_error(
                        "UserControl has a fixed visual tree");
                }
            } else {
                if (!button && type == registry.types.end()) {
                    throw std::range_error("Unknown component type: " +
                                           key.first + ":" + key.second);
                }
                components::Key selected =
                    node.template_name.empty()
                        ? type->second->default_template
                        : components::Key{node.template_namespace,
                                          node.template_name};
                definition = registry.documents.find(selected);
                if (definition == registry.documents.end() ||
                    definition->second.document_kind != "ControlTemplate") {
                    throw std::range_error(
                        "Control template is not registered: " +
                        selected.second);
                }
                if (components::Key{definition->second.target_namespace,
                                    definition->second.target_name} != key) {
                    throw std::range_error(
                        "ControlTemplate TargetType mismatch");
                }
                node.template_namespace = selected.first;
                node.template_name = selected.second;
            }
            node.supplied_content = std::move(node.children);
            node.children = {definition->second.root};
            fleck::Element* presenter = nullptr;
            if (!node.supplied_content.empty()) {
                auto locate = [&](auto& self, fleck::Element& visual) -> void {
                    if (!visual.type_name.empty() || fleck::native_template(visual)) return;
                    if (visual.kind == UI_CONTENT_PRESENTER) {
                        if (presenter) throw std::range_error("Component content requires one ContentPresenter");
                        presenter = &visual;
                        return;
                    }
                    for (auto& child : visual.children) self(self, child);
                };
                locate(locate, node.children.front());
                if (!presenter) throw std::range_error("Component has no ContentPresenter");
                presenter->children.clear();
            }
            stack.push_back(key);
            for (auto &child : node.children) {
                expand_node(registry, child, stack, depth + 1, count);
            }
            stack.pop_back();
            if (presenter) {
                presenter->children = node.supplied_content;
                presenter->children.front().projected = true;
                expand_node(registry, presenter->children.front(), stack, depth + 1, count);
            }
        } else {
            for (auto &child : node.children) {
                expand_node(registry, child, stack, depth + 1, count);
            }
        }
    } catch (const components::Error &) {
        throw;
    } catch (const std::range_error &error) {
        std::string message = error.what();
        if (!stack.empty()) {
            message += " (referenced through ";
            for (size_t i = 0; i < stack.size(); ++i) {
                if (i) {
                    message += " -> ";
                }
                message += stack[i].second;
            }
            message += ')';
        }
        throw components::Error(node, message);
    }
}
} // namespace

components::Error::Error(const fleck::Element &element,
                         const std::string &message)
    : std::range_error(
          (element.source_uri.empty()
               ? std::string()
               : element.source_uri + ":" +
                     std::to_string(element.location.element.line) + ":" +
                     std::to_string(element.location.element.column) + ": ") +
          message),
      line(element.location.element.line),
      column(element.location.element.column) {}

void components::report(const fleck::Element &element, const char *message,
                        ui_fleck_diagnostic &diagnostic) {
    Error error(element, message);
    fleck::report({}, 0, UI_FLECK_VALUE, error.what(), diagnostic);
    diagnostic.line = error.line;
    diagnostic.column = error.column;
}

ui_result components::validate_binding(ui_node &node,
                                       const fleck::Binding &binding) {
    if (!binding.target.empty()) {
        if (!node.component || !binding.format.empty() ||
            binding.trigger != UI_UPDATE_PROPERTY_CHANGED) {
            return UI_ERROR_INVALID_ARGUMENT;
        }
        const auto property = node.component->values.find(binding.target);
        if (binding.target != "DataContext" &&
            (property == node.component->values.end() ||
             property->second.read_only || !(property->second.metadata.flags & UI_PROPERTY_BINDABLE))) {
            return UI_ERROR_INVALID_ARGUMENT;
        }
        if (binding.source != fleck::Binding::Context) {
            auto *source = source_for(node, binding, nullptr);
            if (!source) {
                return UI_ERROR_INVALID_ARGUMENT;
            }
            const auto found = source->properties.find(binding.path);
            if (found == source->properties.end() ||
                found->second.kind != (binding.target == "DataContext"
                                           ? UI_VALUE_MODEL
                                           : property->second.value.kind)) {
                return UI_ERROR_INVALID_ARGUMENT;
            }
            auto mode =
                binding.mode == UI_BIND_DEFAULT
                    ? (binding.target == "DataContext" ? UI_BIND_ONE_WAY
                                                       : property->second.mode)
                    : binding.mode;
            if (mode == UI_BIND_TWO_WAY && (!found->second.set || binding.target == "DataContext" || !(property->second.metadata.flags & UI_PROPERTY_TWO_WAY))) {
                return UI_ERROR_INVALID_ARGUMENT;
            }
        }
        return UI_OK;
    }
    ui_binding_options options{sizeof(options),       1,
                               binding.property,      binding.path.c_str(),
                               binding.mode,          binding.trigger,
                               binding.format.c_str()};
    if (binding.source==fleck::Binding::NamedElement || binding.source==fleck::Binding::Ancestor)
        return binding_validate(node,options);
    if (binding.source != fleck::Binding::Context) {
        auto *source = source_for(node, binding, nullptr);
        if (!source) {
            return UI_ERROR_INVALID_ARGUMENT;
        }
        // These are framework-owned property slots, never business getters.
        return ui_node_bind_property(&node, source, &options);
    }
    return binding_validate(node, options);
}

void components::expand(Registry &registry, fleck::Element &element) {
    std::vector<Key> stack;
    unsigned count = 0;
    expand_node(registry, element, stack, 0, count);
}

ui_result ui_app_register_component_type(ui_app *app,
                                         const ui_component_type *descriptor) {
    if (const auto status = binding_app_check(app); status != UI_OK) {
        return status;
    }
    if (descriptor && descriptor->version != UI_COMPONENT_TYPE_VERSION) return UI_ERROR_VERSION_MISMATCH;
    if (!descriptor || descriptor->struct_size != sizeof(*descriptor) ||
        !descriptor->namespace_uri ||
        !*descriptor->namespace_uri || !descriptor->name ||
        !fleck::valid_binding_name(descriptor->name) ||
        descriptor->property_count > 256 || descriptor->event_count > 256 ||
        descriptor->part_count > 256 ||
        (descriptor->property_count && !descriptor->properties) ||
        (descriptor->event_count && !descriptor->events) ||
        (descriptor->part_count && !descriptor->parts)) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto type = std::make_shared<components::Type>();
        type->key = {descriptor->namespace_uri, descriptor->name};
        type->default_template = {
            descriptor->template_namespace ? descriptor->template_namespace
                                           : "",
            descriptor->template_name ? descriptor->template_name : ""};
        type->lifecycle = descriptor->lifecycle;
        type->changed = descriptor->changed;
        type->context = descriptor->context;
        std::set<std::string> names;
        for (size_t i = 0; i < descriptor->property_count; ++i) {
            const auto &p = descriptor->properties[i];
            if (!p.name || !binding_path(p.name) ||
                !names.insert(p.name).second ||
                std::string_view(p.name) == "DataContext" || p.read_only > 1 ||
                p.default_value.kind < UI_VALUE_STRING ||
                p.default_value.kind > UI_VALUE_ENUM ||
                p.default_mode < UI_BIND_ONE_WAY ||
                p.default_mode > UI_BIND_DEFAULT) {
                return UI_ERROR_INVALID_ARGUMENT;
            }
            if ((p.default_value.kind == UI_VALUE_MODEL ||
                 p.default_value.kind == UI_VALUE_COMMAND) &&
                p.default_value.command) {
                return UI_ERROR_INVALID_ARGUMENT;
            }
            BindingValue copy;
            if (binding_copy_value(p.default_value, copy,
                                   p.default_value.kind) != UI_OK) {
                return UI_ERROR_INVALID_ARGUMENT;
            }
            type->properties.push_back(
                {p.name, copy, p.read_only != 0,
                 p.default_mode == UI_BIND_DEFAULT ? UI_BIND_ONE_WAY
                                                   : p.default_mode});
            auto& stored = type->properties.back();
            stored.id = properties::allocate_id();
            if (!stored.id) return UI_ERROR_OUT_OF_MEMORY;
            if (p.metadata && (p.metadata->struct_size != sizeof(*p.metadata) || p.metadata->version != 1)) return UI_ERROR_INVALID_ARGUMENT;
            stored.metadata = p.metadata ? *p.metadata : ui_component_property_metadata{
                sizeof(ui_component_property_metadata), 1, inferred_flags(copy.value.kind, stored.read_only), nullptr, nullptr, nullptr};
            if (!valid_metadata(stored.metadata, stored.value.kind, stored.read_only) ||
                (stored.mode == UI_BIND_TWO_WAY && !(stored.metadata.flags & UI_PROPERTY_TWO_WAY))) return UI_ERROR_INVALID_ARGUMENT;
        }
        names.clear();
        for (size_t i = 0; i < descriptor->event_count; ++i) {
            if (!descriptor->events[i] ||
                !binding_path(descriptor->events[i]) ||
                !names.insert(descriptor->events[i]).second) {
                return UI_ERROR_INVALID_ARGUMENT;
            }
            type->events.emplace_back(descriptor->events[i]);
        }
        names.clear();
        for (size_t i = 0; i < descriptor->part_count; ++i) {
            const auto &p = descriptor->parts[i];
            if (!p.name || !fleck::valid_binding_name(p.name) ||
                !names.insert(p.name).second || p.kind < UI_STACK_PANEL ||
                p.kind > UI_PATH || p.required > 1) {
                return UI_ERROR_INVALID_ARGUMENT;
            }
            type->parts.push_back({p.name, p.kind, p.required != 0});
        }
        auto &types = components::registry(app).types;
        if (types.contains(type->key)) {
            return UI_ERROR_INVALID_ARGUMENT;
        }
        types.emplace(type->key, std::move(type));
        return UI_OK;
    } catch (const std::bad_alloc &) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}

ui_result
ui_app_register_fleck_resources(ui_app *app,
                                const ui_fleck_resource_entry *entries,
                                size_t count, ui_fleck_diagnostic *diagnostic) {
    if (const auto status = binding_app_check(app); status != UI_OK) {
        return status;
    }
    if ((!entries && count) || count > 1024) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    ui_fleck_diagnostic local{};
    if (!diagnostic) {
        diagnostic = &local;
    }
    *diagnostic = {};
    try {
        auto documents = components::registry(app).documents;
        auto dictionaries = components::registry(app).dictionaries;
        std::set<std::string> incoming;
        for (size_t i = 0; i < count; ++i) {
            const auto& entry = entries[i];
            if (!entry.uri || (!entry.utf8 && entry.length) || !incoming.emplace(entry.uri).second)
                return UI_ERROR_INVALID_ARGUMENT;
            if (entry.utf8 && fleck::is_dictionary({entry.utf8,entry.length}))
                dictionaries.insert_or_assign(entry.uri,std::string(entry.utf8,entry.length));
            else dictionaries.erase(entry.uri);
        }
        std::set<components::Key> seen;
        for (size_t i = 0; i < count; ++i) {
            if (!entries[i].utf8) {
                std::erase_if(documents, [&](const auto &entry) {
                    return entry.second.source_uri == entries[i].uri;
                });
                continue;
            }
            fleck::Page page;
            if (!fleck::parse({entries[i].utf8, entries[i].length}, page,
                              *diagnostic,dictionaries,entries[i].uri)) {
                const auto original = *diagnostic;
                fleck::Element source;
                source.source_uri = entries[i].uri;
                source.location.element.line = original.line;
                source.location.element.column = original.column;
                components::report(source, original.message, *diagnostic);
                diagnostic->code = original.code;
                return UI_ERROR_INVALID_ARGUMENT;
            }
            if (page.document_kind == "ResourceDictionary") {
                std::erase_if(documents,[&](const auto& item){ return item.second.source_uri == entries[i].uri; });
                continue;
            }
            if (page.document_kind == "Window") {
                fleck::report({}, 0, UI_FLECK_ELEMENT,
                              "Resource must be UserControl or ControlTemplate",
                              *diagnostic);
                return UI_ERROR_INVALID_ARGUMENT;
            }
            page.source_uri = entries[i].uri;
            auto stamp = [&](auto &self, fleck::Element &element) -> void {
                element.source_uri = page.source_uri;
                for (auto &child : element.children) {
                    self(self, child);
                }
            };
            stamp(stamp, page.root);
            components::Key key{
                page.type_namespace,
                page.class_name.substr(page.class_name.find_last_of('.') + 1)};
            if (!seen.insert(key).second ||
                (documents.contains(key) &&
                 documents.at(key).source_uri != page.source_uri)) {
                fleck::report({}, 0, UI_FLECK_VALUE,
                              "Duplicate component resource type", *diagnostic);
                return UI_ERROR_INVALID_ARGUMENT;
            }
            std::erase_if(documents, [&](const auto &entry) {
                return entry.first != key &&
                       entry.second.source_uri == page.source_uri;
            });
            documents.insert_or_assign(key, std::move(page));
        }
        components::registry(app).documents.swap(documents);
        components::registry(app).dictionaries.swap(dictionaries);
        return UI_OK;
    } catch (const std::bad_alloc &) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}

void components::prepare(ui_node &node, const fleck::Element &element,
                         ui_model *inherited) {
    const bool button = fleck::native_template(element);
    if (element.type_name.empty() && !button) {
        return;
    }
    auto c = std::make_shared<ui_component>();
    c->host = &node;
    (void)inherited;
    c->declaration = element;
    c->declaration.children = std::move(c->declaration.supplied_content);
    auto &r = registry(node.owner);
    const Key key = button ? Key{"urn:sunfleck:v1",devtools::control_name(element.kind)} : Key{element.type_namespace,element.type_name};
    const auto type = r.types.find(key);
    if (type != r.types.end()) {
        c->type = type->second;
    } else {
        c->type = std::make_shared<Type>();
        c->type->key = key;
    }
    c->is_control = !element.template_name.empty();
    c->selected_template = {element.template_namespace, element.template_name};
    c->properties = std::make_unique<ui_model>();
    c->properties->owner = node.owner;
    c->properties->debug_id = devtools::next_id(node.owner);
    if (button && fleck::range_template(node.kind)) {
        ui_model_property range_value{sizeof(ui_model_property), 1, "Value", UI_VALUE_INT,
            [](void* context, ui_binding_value* result) -> ui_result {
                return ui_node_get_property(static_cast<ui_component*>(context)->host, UI_PROPERTY_VALUE, result);
            }, nullptr, c.get()};
        require(ui_model_add_property(c->properties.get(), &range_value));
    }
    for (const auto &p : c->type->properties) {
        auto &value = c->values[p.name];
        static_cast<Property &>(value) = p;
        value.component = c.get();
        if (value.value.kind == UI_VALUE_STRING) {
            value.value.string = value.text.c_str();
        }
        ui_model_property property{sizeof(property),
                                   1,
                                   p.name.c_str(),
                                   p.value.kind,
                                   get_value,
                                   p.read_only ? nullptr : set_value,
                                   &value};
        require(ui_model_add_property(c->properties.get(), &property));
    }
    node.component = c;
    for (auto& [name, value] : c->values) {
        BindingValue effective(value);
        struct Guard { bool& flag; ~Guard() { flag = false; } } guard{c->changing_property};
        c->changing_property = true;
        require(effective_value(*c, value, effective));
        static_cast<BindingValue&>(value) = std::move(effective);
    }
    c->initialized = true;
    if (c->type->lifecycle) {
        CallbackScope scope(node.owner);
        require(c->type->lifecycle(c.get(), UI_COMPONENT_INITIALIZE,
                                   c->type->context));
    }
    if (button) return; // Built-in properties were already applied by the node factory.
    for (const auto &[name, literal] : element.properties) {
        if (name==".Effect" || name==".OpacityMask" || name==".Storyboard" || name.starts_with(".State.") ||
            name.starts_with(".PointerBinding.") || name.starts_with(".KeyBinding.") || name.starts_with(".CommandBinding.") || name.starts_with(".Resource.") ||
            name.starts_with(".DynamicLocal.")) continue;
        if (name == "LayoutTransform" || name == "RenderTransform" || name == "RenderTransformOrigin" || name == "Opacity" || visual_layout::canvas_coordinate(name) || name == "Panel.ZIndex" || grid_layout::attribute(name)) continue;
        ui_property state; if (properties::find(name,state) && (visual_state::property(state) || text_format::property(state))) continue;
        if (name == "DataContext") {
            if (literal != "null") throw std::range_error("DataContext requires a model binding or null");
            continue;
        }
        auto found = c->values.find(name);
        if (found == c->values.end() || found->second.read_only) {
            throw std::range_error("Unknown or read-only component property: " +
                                   name);
        }
        BindingValue parsed;
        if (binding_parse_value(literal, found->second.value.kind, parsed) != UI_OK)
            throw std::range_error("Invalid literal for component property: " + name);
        require(set(*c, name, parsed.value, true));
    }
}

ui_result components::bind(ui_node &node, const fleck::Element &element,
                           ui_model *inherited) {
    if (node.component) {
        std::vector<ContextBindingUpdate> updates;
        if (auto status = prepare_inherited_values(node, updates); status != UI_OK) return status;
    // A descendant validator must not rewrite an ancestor whose value it is validating.
    std::optional<PropertyChangeScope> ancestor;
    if (node.component) ancestor.emplace(*node.component);
        if (auto status = commit_context_bindings(updates); status != UI_OK) return status;
    }
    node.command_mode =
        element.async_command ? UI_COMMAND_ASYNC : UI_COMMAND_SYNC;
    if (auto status = bind_node(node, element, inherited); status != UI_OK) return status;
    if (!node.routed_command.empty()) {
        for (auto* owner = &node; owner; owner = owner->inheritance_parent()) {
            auto path = owner->command_paths.find(node.routed_command);
            if (path == owner->command_paths.end()) continue;
            ui_command* command = nullptr;
            if (auto status = binding_read_command(*owner,path->second,command); status != UI_OK) return status;
            if (command && command->asynchronous() != element.async_command) return UI_ERROR_INVALID_ARGUMENT;
            break;
        }
    }
    return binding_initialize_data_triggers(node);
}
ui_result components::resolve_retained_values(ui_component& c) {
    if (c.changing_property) return UI_ERROR_INVALID_STATE;
    struct Guard { bool& flag; ~Guard() { flag = false; } } guard{c.changing_property};
    c.changing_property = true;
    std::map<std::string, BindingValue> prepared;
    for (auto& [name, value] : c.values) {
        const auto* selected = value.layers.effective();
        BindingValue effective(selected ? *selected : default_property(c, name));
        if (auto status = effective_value(c, value, effective); status != UI_OK) return status;
        prepared.emplace(name, std::move(effective));
    }
    for (auto& [name, value] : prepared) static_cast<BindingValue&>(c.values.at(name)) = std::move(value);
    return UI_OK;
}
void components::validate_parts(ui_node &node) {
    if (!node.component) {
        return;
    }
    auto &c = *node.component;
    for (const auto &part : c.type->parts) {
        ui_node *found = nullptr;
        auto result = ui_component_find_node(&c, part.name.c_str(), &found);
        if ((result != UI_OK && part.required) ||
            (found && found->kind != part.kind)) {
            throw std::range_error("Missing or incompatible template part: " +
                                   part.name);
        }
    }
}
void components::mount(ui_node &node) {
    if (!node.component) {
        return;
    }
    auto &c = *node.component;
    CallbackScope scope(node.owner);
    if (c.type->lifecycle && c.is_control) {
        require(c.type->lifecycle(&c, UI_COMPONENT_TEMPLATE_APPLIED,
                                  c.type->context));
    }
    c.mounted = true;
    if (c.type->lifecycle) {
        require(c.type->lifecycle(&c, UI_COMPONENT_MOUNT, c.type->context));
    }
}
void components::shutdown(ui_node &node) {
    if (!node.component || node.component->destroyed) {
        return;
    }
    auto &c = *node.component;
    if (c.type->lifecycle) {
        CallbackScope scope(node.owner);
        try {
            if (c.mounted) {
                (void)c.type->lifecycle(&c, UI_COMPONENT_UNMOUNT,
                                        c.type->context);
            }
            (void)c.type->lifecycle(&c, UI_COMPONENT_DESTROY, c.type->context);
        } catch (...) {
            binding_app_error(node.owner, UI_ERROR_INVALID_STATE);
        }
    }
    if (c.release_state) {
        try {
            c.release_state(c.state);
        } catch (...) {
            binding_app_error(node.owner, UI_ERROR_INVALID_STATE);
        }
        c.release_state = nullptr;
        c.state = nullptr;
    }
    c.mounted = false;
    c.destroyed = true;
    c.connections.clear();
    c.events.clear();
    model_destroyed(*c.properties);
}
ui_component::~ui_component() {
    // Staging failures have not attached the host to the app yet.
    if (!destroyed && initialized && type && type->lifecycle) {
        try {
            if (mounted) {
                (void)type->lifecycle(this, UI_COMPONENT_UNMOUNT,
                                      type->context);
            }
            (void)type->lifecycle(this, UI_COMPONENT_DESTROY, type->context);
        } catch (...) {
        }
    }
    if (release_state) {
        try {
            release_state(state);
        } catch (...) {
        }
    }
}
ui_result ui_component_set_state(ui_component *c, void *state,
                                 void (*release)(void *)) {
    if (auto status = check(c); status != UI_OK) {
        return status;
    }
    if (c->state || c->release_state) {
        return UI_ERROR_INVALID_STATE;
    }
    c->state = state;
    c->release_state = release;
    return UI_OK;
}
ui_result ui_component_get_state(ui_component *c, void **state) {
    if (auto status = check(c); status != UI_OK) {
        return status;
    }
    if (!state) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    *state = c->state;
    return UI_OK;
}
void components::clear_reference(ui_app *app, ui_value_kind kind,
                                 const void *reference) {
    CallbackScope scope(app);
    for (const auto &node : binding_app_nodes(app)) {
        for (auto& [property, slot] : node->property_slots) {
            for (auto& [precedence, stored] : slot.values) {
                if (stored.value.kind == kind && stored.value.command == reference) stored.value.command = nullptr;
            }
        }
        if (!node->component || node->component->destroyed) {
            continue;
        }
        auto &component = *node->component;
        for (auto &[name, value] : component.values) {
            for (auto& [precedence, stored] : value.layers.values) {
                if (stored.value.kind == kind && stored.value.command == reference) stored.value.command = nullptr;
            }
            if (value.value.kind != kind || value.value.command != reference) {
                continue;
            }
            value.value.command = nullptr;
            binding_app_error(app,
                              ui_model_notify_property_changed(
                                  component.properties.get(), name.c_str()));
            if (component.type->changed) {
                try {
                    component.type->changed(&component, name.c_str(),
                                            component.type->context);
                } catch (...) {
                    binding_app_error(app, UI_ERROR_INVALID_STATE);
                }
            }
        }
    }
}
void components::model_destroyed(ui_model &model) {
    const auto observers = model.observers;
    for (const auto &weak : observers) {
        if (auto observer = weak.lock()) {
            (void)observer->notify("", true);
        }
    }
    model.observers.clear();
    clear_reference(model.owner, UI_VALUE_MODEL, &model);
}

ui_result ui_node_get_component(ui_node *node, ui_component **result) {
    if (!node || !result) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    *result = nullptr;
    if (auto status = binding_app_check(node->owner); status != UI_OK) {
        return status;
    }
    *result = node->component.get();
    return *result ? UI_OK : UI_ERROR_INVALID_ARGUMENT;
}
ui_result ui_component_get_node(ui_component *c, ui_node **result) {
    if (!result) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    *result = nullptr;
    if (auto status = check(c); status != UI_OK) {
        return status;
    }
    *result = c->host;
    return UI_OK;
}
ui_result ui_component_find_node(ui_component *c, const char *id,
                                 ui_node **result) {
    if (!result || !id) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    *result = nullptr;
    if (auto status = check(c); status != UI_OK) {
        return status;
    }
    auto find = [&](auto &self, ui_node *node) -> void {
        if (node->component_scope.get() != c) {
            return;
        }
        if (node->fleck_id == id) {
            *result = node;
            return;
        }
        if (!node->component) {
            for (auto *child : node->children) {
                self(self, child);
            }
        }
    };
    for (auto *child : c->host->children) {
        find(find, child);
    }
    return *result ? UI_OK : UI_ERROR_INVALID_ARGUMENT;
}
ui_result ui_component_find_template_part(ui_component *c, const char *id,
                                          ui_node **result) {
    if (!c || !c->is_control) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    return ui_component_find_node(c, id, result);
}
ui_result ui_component_get_property(ui_component *c, const char *name,
                                    ui_binding_value *value) {
    if (auto status = check(c); status != UI_OK) {
        return status;
    }
    if (!name || !value) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    auto found = c->values.find(name);
    return found == c->values.end() ? UI_ERROR_INVALID_ARGUMENT
                                    : get_value(&found->second, value);
}
ui_result ui_component_set_property(ui_component *c, const char *name,
                                    const ui_binding_value *value) {
    if (auto status = check(c); status != UI_OK) {
        return status;
    }
    if (!name || !value) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    try {
        return set(*c, name, *value, false);
    } catch (const std::bad_alloc &) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}
ui_result ui_component_set_property_layer(ui_component* c, const char* name, ui_value_precedence precedence, const ui_binding_value* value) {
    if (auto status = check(c); status != UI_OK) return status;
    if (!name || !value) return UI_ERROR_INVALID_ARGUMENT;
    try { return set(*c, name, *value, precedence != UI_PRECEDENCE_LOCAL, precedence); }
    catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_component_clear_property_layer(ui_component* c, const char* name, ui_value_precedence precedence) {
    if (auto status = check(c); status != UI_OK) return status;
    if (!name || !c->values.contains(name)) return UI_ERROR_INVALID_ARGUMENT;
    try { return set(*c, name, default_property(*c, name).value, false, precedence, true); }
    catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_component_get_base_property(ui_component* c, const char* name, ui_binding_value* value) {
    if (auto status = check(c); status != UI_OK) return status;
    if (!name || !value) return UI_ERROR_INVALID_ARGUMENT;
    auto found = c->values.find(name);
    if (found == c->values.end()) return UI_ERROR_INVALID_ARGUMENT;
    const auto* base = found->second.layers.effective(true);
    *value = base ? base->value : default_property(*c, name).value;
    return UI_OK;
}
ui_result ui_component_get_property_source(ui_component* c, const char* name, ui_property_source* source) {
    if (auto status = check(c); status != UI_OK) return status;
    if (!name || !source) return UI_ERROR_INVALID_ARGUMENT;
    auto found = c->values.find(name);
    if (found == c->values.end()) return UI_ERROR_INVALID_ARGUMENT;
    if (found->second.layers.values.empty()) { *source = UI_PROPERTY_SOURCE_DEFAULT; return UI_OK; }
    const auto priority = found->second.layers.values.rbegin()->first;
    constexpr ui_property_source sources[] = {UI_PROPERTY_SOURCE_INHERITED, UI_PROPERTY_SOURCE_THEME, UI_PROPERTY_SOURCE_STYLE,
        UI_PROPERTY_SOURCE_TRIGGER, UI_PROPERTY_SOURCE_TEMPLATE, UI_PROPERTY_SOURCE_LOCAL, UI_PROPERTY_SOURCE_ANIMATION};
    *source = priority == UI_PRECEDENCE_LOCAL && c->connections.contains(name) ? UI_PROPERTY_SOURCE_BINDING : sources[priority];
    return UI_OK;
}
ui_result ui_component_describe_property(ui_component* c, const char* name, ui_component_property_descriptor* descriptor) {
    if (auto status = check(c); status != UI_OK) return status;
    if (!name || !descriptor || descriptor->struct_size != sizeof(*descriptor) || descriptor->version != 1 || !c->values.contains(name)) return UI_ERROR_INVALID_ARGUMENT;
    const auto& property = default_property(*c, name);
    *descriptor = {sizeof(*descriptor), 1, property.name.c_str(), property.value.kind,
        property.metadata.flags, uint32_t(property.read_only), property.value};
    return UI_OK;
}
namespace {
ui_result prepare_connection(ui_component& c, const char* name, ui_model* model, const char* path,
                             ui_binding_mode mode, bool context, components::ContextBindingUpdate& update,
                             const BindingValue* prepared = nullptr, const properties::Slot* pending_layers = nullptr) {
    if (!name || !path || !binding_path(path) || (!model && !context) ||
        (model && (model->owner != c.host->owner || model->callbacks)) || mode < UI_BIND_ONE_WAY || mode > UI_BIND_DEFAULT)
        return UI_ERROR_INVALID_ARGUMENT;
    const auto target = c.values.find(name);
    if (target == c.values.end() || target->second.read_only || !(target->second.metadata.flags & UI_PROPERTY_BINDABLE))
        return UI_ERROR_INVALID_ARGUMENT;
    if (c.changing_property) return UI_ERROR_INVALID_STATE;
    if (mode == UI_BIND_DEFAULT) mode = target->second.mode;
    if (mode == UI_BIND_TWO_WAY && !(target->second.metadata.flags & UI_PROPERTY_TWO_WAY)) return UI_ERROR_INVALID_ARGUMENT;
    update.component = &c; update.name = name; update.layers = pending_layers ? *pending_layers : target->second.layers;
    update.reconnect = true;
    update.connection = {model, path, mode, nullptr, context};
    if (model) {
        const auto source = model->properties.find(path);
        if (source == model->properties.end() || source->second.kind != target->second.value.kind ||
            (mode == UI_BIND_TWO_WAY && !source->second.set)) return UI_ERROR_INVALID_ARGUMENT;
        BindingValue value;
        if (prepared) value = *prepared;
        else if (auto status = read(model, path, value); status != UI_OK) return status;
        if (value.value.kind != target->second.value.kind ||
            (value.value.kind == UI_VALUE_MODEL && value.value.model && value.value.model->owner != c.host->owner) ||
            (value.value.kind == UI_VALUE_COMMAND && value.value.command && value.value.command->owner != c.host->owner)) return UI_ERROR_INVALID_ARGUMENT;
        update.layers.values.insert_or_assign(UI_PRECEDENCE_LOCAL, std::move(value));
    } else update.layers.values.erase(UI_PRECEDENCE_LOCAL);
    const auto* selected = update.layers.effective();
    update.effective = selected ? *selected : default_property(c, name);
    {
        struct Guard { bool& flag; ~Guard() { flag = false; } } guard{c.changing_property};
        c.changing_property = true;
        if (auto status = effective_value(c, target->second, update.effective); status != UI_OK) return status;
    }
    if (model && mode != UI_BIND_ONE_TIME) {
        auto observer = std::make_shared<ModelObserver>();
        std::weak_ptr<ui_component> weak = c.shared_from_this();
        const std::string target_name(name), source_path(path);
        observer->notify = [weak, model, source_path, target_name](const std::string& changed, bool destroyed) {
            auto component = weak.lock();
            if (!component || component->destroyed) return UI_OK;
            auto link = component->connections.find(target_name);
            if (link == component->connections.end() || link->second.model != model || link->second.path != source_path) return UI_OK;
            if (destroyed) { link->second.model = nullptr; return UI_OK; }
            if (!changed.empty() && changed != source_path) return UI_OK;
            BindingValue value;
            if (auto status = read(model, source_path, value); status != UI_OK) return status;
            return set(*component, target_name, value.value, true);
        };
        update.connection.observer = std::move(observer);
    }
    return UI_OK;
}
}
const BindingValue* components::prepared_binding_value(const std::vector<ContextBindingUpdate>& updates, ui_model* model, const std::string& path) {
    for (auto it = updates.rbegin(); it != updates.rend(); ++it)
        if (it->component->properties.get() == model && it->name == path) return &it->effective;
    return nullptr;
}
const components::ContextBindingUpdate* components::pending_property(const std::vector<ContextBindingUpdate>& updates,
    const ui_component& component, const std::string& name) {
    for (auto it = updates.rbegin(); it != updates.rend(); ++it)
        if (it->component == &component && it->name == name) return &*it;
    return nullptr;
}
void components::record_property_update(std::vector<ContextBindingUpdate>& updates, ContextBindingUpdate update) {
    for (auto& pending : updates) if (pending.component == update.component && pending.name == update.name) {
        if (!update.reconnect && !update.disconnect) {
            update.connection = std::move(pending.connection);
            update.reconnect = pending.reconnect; update.disconnect = pending.disconnect;
        }
        pending = std::move(update);
        return;
    }
    updates.push_back(std::move(update));
}
ui_result components::prepare_inherited_values(ui_node& node, std::vector<ContextBindingUpdate>& updates) {
    if (!node.component) return UI_OK;
    auto& component = *node.component;
    if (component.changing_property) return UI_ERROR_INVALID_STATE;
    for (const auto& property : component.type->properties) {
        if (!(property.metadata.flags & UI_PROPERTY_INHERITS)) continue;
        const BindingValue* incoming = nullptr;
        for (auto* ancestor = node.inheritance_parent(); ancestor && !incoming; ancestor = ancestor->inheritance_parent()) {
            if (!ancestor->component || ancestor->component->type != component.type) continue;
            auto& parent = *ancestor->component;
            const auto* pending = pending_property(updates, parent, property.name);
            const auto& stored = parent.values.at(property.name);
            const auto& layers = pending ? pending->layers : stored.layers;
            if (!layers.values.empty()) incoming = pending ? &pending->effective : &stored;
        }
        const auto* pending = pending_property(updates, component, property.name);
        const auto& stored = component.values.at(property.name);
        const auto& layers = pending ? pending->layers : stored.layers;
        const auto previous = std::find_if(layers.values.begin(), layers.values.end(), [](const auto& entry) {
            return entry.first == UI_PRECEDENCE_INHERITED;
        });
        if ((!incoming && previous == layers.values.end()) ||
            (incoming && previous != layers.values.end() && previous->second.equals(*incoming))) continue;
        ContextBindingUpdate update;
        update.component = &component; update.name = property.name; update.layers = layers;
        if (incoming) update.layers.values.insert_or_assign(UI_PRECEDENCE_INHERITED, *incoming);
        else update.layers.values.erase(UI_PRECEDENCE_INHERITED);
        const auto* selected = update.layers.effective();
        update.effective = selected ? *selected : property;
        {
            struct Guard { bool& flag; ~Guard() { flag = false; } } guard{component.changing_property};
            component.changing_property = true;
            if (auto status = effective_value(component, property, update.effective); status != UI_OK) return status;
        }
        record_property_update(updates, std::move(update));
    }
    return UI_OK;
}
ui_result components::prepare_inherited_subtree(ui_node& node, std::vector<ContextBindingUpdate>& updates) {
    if (auto status = prepare_inherited_values(node, updates); status != UI_OK) return status;
    for (auto* child : node.children)
        if (auto status = prepare_inherited_subtree(*child, updates); status != UI_OK) return status;
    return UI_OK;
}
ui_result components::prepare_context_bindings(ui_component& component, ui_model* model, std::vector<ContextBindingUpdate>& updates) {
    std::set<std::string> visiting, complete;
    const auto prepare = [&](auto& self, const std::string& name) -> ui_result {
        if (complete.contains(name)) return UI_OK;
        // A closed Self cycle has no dependency on the incoming DataContext.
        // Leave it untouched; normal model notification still enforces its
        // existing bounded cycle handling when one of its values is written.
        if (!visiting.insert(name).second) return UI_OK;
        const auto& connection = component.connections.at(name);
        auto* source = connection.uses_data_context ? model : connection.model;
        if (!connection.uses_data_context && connection.mode == UI_BIND_ONE_TIME) { complete.insert(name); return UI_OK; }
        if (source == component.properties.get() && component.connections.contains(connection.path)) {
            if (auto status = self(self, connection.path); status != UI_OK) return status;
        }
        const auto* value = prepared_binding_value(updates, source, connection.path);
        if (source == connection.model && !value) { complete.insert(name); return UI_OK; }
        ContextBindingUpdate update;
        const auto* pending = pending_property(updates, component, name);
        if (auto status = prepare_connection(component, name.c_str(), source, connection.path.c_str(), connection.mode,
            connection.uses_data_context, update, value, pending ? &pending->layers : nullptr); status != UI_OK) return status;
        record_property_update(updates, std::move(update));
        complete.insert(name);
        return UI_OK;
    };
    for (const auto& [name, connection] : component.connections) {
        if (auto status = prepare(prepare, name); status != UI_OK) return status;
    }
    return UI_OK;
}
namespace {
PreparedCommit::PreparedCommit(const std::vector<components::ContextBindingUpdate>& updates) {
    connections.reserve(updates.size());
    changed.reserve(updates.size());
    for (const auto& update : updates) {
        auto& c = *update.component;
        if (!c.values.at(update.name).equals(update.effective)) changed.emplace_back(&c, update.name);
        if (update.connection.observer) {
            auto& observers = update.connection.model->observers;
            observers.reserve(observers.size() + updates.size());
        }
        Connections staged;
        if (update.reconnect) staged.emplace(update.name, update.connection);
        connections.push_back(staged.empty() ? Connections::node_type{} : staged.extract(staged.begin()));
    }
}

ui_result PreparedCommit::publish(std::vector<components::ContextBindingUpdate>& updates) {
    for (size_t index = 0; index < updates.size(); ++index) {
        auto& update = updates[index];
        auto& c = *update.component;
        auto& target = c.values.at(update.name);
        const bool equal = target.equals(update.effective);
        if (update.disconnect) c.connections.erase(update.name);
        else if (update.reconnect) {
            c.connections.erase(update.name);
            auto inserted = c.connections.insert(std::move(connections[index]));
            auto& connection = inserted.position->second;
            if (connection.observer) connection.model->observers.push_back(connection.observer);
        }
        target.layers = std::move(update.layers);
        static_cast<BindingValue&>(target) = std::move(update.effective);
        if (!equal) {
            invalidate_layout(*c.host, target.metadata.flags);
            if (target.metadata.flags & (UI_PROPERTY_AFFECTS_MEASURE | UI_PROPERTY_AFFECTS_ARRANGE | UI_PROPERTY_AFFECTS_RENDER | UI_PROPERTY_AFFECTS_INPUT))
                binding_app_dirty(c.host->owner);
        }
    }
    for (const auto& [c, name] : changed) {
        if (auto status = ui_model_notify_property_changed(c->properties.get(), name.c_str()); status != UI_OK) return status;
        if (c->type->changed) {
            CallbackScope scope(c->host->owner);
            try { c->type->changed(c, name.c_str(), c->type->context); }
            catch (...) { return UI_ERROR_INVALID_STATE; }
        }
    }
    return UI_OK;
}
} // namespace

ui_result components::commit_context_bindings(std::vector<ContextBindingUpdate>& updates) {
    return PreparedCommit(updates).publish(updates);
}
ui_result components::bind_context_property(ui_component& component, const char* name, const char* path, ui_binding_mode mode) {
    if (auto status = check(&component); status != UI_OK) return status;
    try {
        std::vector<ContextBindingUpdate> updates(1);
        if (auto status = prepare_connection(component, name, binding_data_context(*component.host), path, mode, true, updates.front()); status != UI_OK) return status;
        if (component.values.at(name).metadata.flags & UI_PROPERTY_INHERITS)
            if (auto status = prepare_inherited_subtree(*component.host, updates); status != UI_OK) return status;
        return commit_context_bindings(updates);
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_component_bind_property(ui_component* component, const char* name, ui_model* model, const char* path, ui_binding_mode mode) {
    if (auto status = check(component); status != UI_OK) return status;
    if (name && std::string_view(name) == "DataContext") {
        const ui_binding_options options{sizeof(options), 1, UI_PROPERTY_DATA_CONTEXT, path, mode, UI_UPDATE_PROPERTY_CHANGED, nullptr};
        return ui_node_bind_property(component->host, model, &options);
    }
    try {
        std::vector<components::ContextBindingUpdate> updates(1);
        if (auto status = prepare_connection(*component, name, model, path, mode, false, updates.front()); status != UI_OK) return status;
        if (component->values.at(name).metadata.flags & UI_PROPERTY_INHERITS)
            if (auto status = components::prepare_inherited_subtree(*component->host, updates); status != UI_OK) return status;
        return components::commit_context_bindings(updates);
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_component_set_data_context(ui_component* c, ui_model* model) {
    if (auto status = check(c); status != UI_OK) return status;
    return ui_node_set_data_context(c->host, model);
}
ui_result ui_component_clear_data_context(ui_component* c) {
    if (auto status = check(c); status != UI_OK) return status;
    return ui_node_clear_data_context(c->host);
}
ui_result ui_component_get_data_context(ui_component* c, ui_model** model) {
    if (auto status = check(c); status != UI_OK) return status;
    return ui_node_get_data_context(c->host, model);
}
ui_result ui_component_subscribe(ui_component *c, const char *name,
                                 ui_component_event_handler handler,
                                 void *context, uint64_t *token) {
    if (auto status = check(c); status != UI_OK) {
        return status;
    }
    if (!name || !handler || !token ||
        std::find(c->type->events.begin(), c->type->events.end(), name) ==
            c->type->events.end()) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    try {
        *token = c->next_token++;
        c->events.push_back({*token, name, handler, context});
        return UI_OK;
    } catch (const std::bad_alloc &) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}
ui_result ui_component_unsubscribe(ui_component *c, uint64_t token) {
    if (auto status = check(c); status != UI_OK) {
        return status;
    }
    return std::erase_if(c->events,
                         [token](const auto &e) { return e.token == token; })
               ? UI_OK
               : UI_ERROR_INVALID_ARGUMENT;
}
struct ui_component_subscription {
    std::weak_ptr<ui_component> component;
    uint64_t token = 0;
};
ui_result ui_component_subscribe_scoped(ui_component *component,
                                        const char *name,
                                        ui_component_event_handler handler,
                                        void *context,
                                        ui_component_subscription **output) {
    if (!output) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    *output = nullptr;
    if (auto status = check(component); status != UI_OK) {
        return status;
    }
    try {
        auto subscription = std::make_unique<ui_component_subscription>();
        subscription->component = component->weak_from_this();
        auto status = ui_component_subscribe(component, name, handler, context,
                                             &subscription->token);
        if (status != UI_OK) {
            return status;
        }
        *output = subscription.release();
        return UI_OK;
    } catch (const std::bad_alloc &) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}
ui_result
ui_component_subscription_destroy(ui_component_subscription *subscription) {
    if (!subscription) {
        return UI_OK;
    }
    if (auto component = subscription->component.lock();
        component && !component->destroyed) {
        if (auto status = check(component.get()); status != UI_OK) {
            return status;
        }
        (void)ui_component_unsubscribe(component.get(), subscription->token);
    }
    delete subscription;
    return UI_OK;
}
ui_result ui_component_raise(ui_component *c, const char *name,
                             const ui_binding_value *value) {
    if (auto status = check(c); status != UI_OK) {
        return status;
    }
    if (!name || std::find(c->type->events.begin(), c->type->events.end(),
                           name) == c->type->events.end()) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    try {
        CallbackScope scope(c->host->owner);
        const auto events = c->events;
        for (const auto &e : events) {
            if (e.name == name &&
                std::any_of(c->events.begin(), c->events.end(),
                            [&](const auto &active) {
                                return active.token == e.token;
                            })) {
                e.handler(c, value, e.context);
            }
        }
        return UI_OK;
    } catch (...) {
        return UI_ERROR_INVALID_STATE;
    }
}

ui_result ui_app_register_component_manifest(ui_app *app, const char *json,
                                             size_t length,
                                             ui_fleck_diagnostic *diagnostic) {
    if (const auto status = binding_app_check(app); status != UI_OK) {
        return status;
    }
    if (!json || length > 1024 * 1024) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    ui_fleck_diagnostic local{};
    if (!diagnostic) {
        diagnostic = &local;
    }
    *diagnostic = {};
    auto &registry = components::registry(app);
    decltype(registry.types) previous;
    bool saved = false;
    try {
        previous = registry.types;
        saved = true;
        auto document = docking::json::Reader({json, length}).parse();
        if (document.at("version").number(1, 1) != 1) {
            throw std::range_error("Unsupported component manifest");
        }
        std::set<components::Key> seen;
        for (const auto &item : document.at("types").array()) {
            auto ns = item.at("namespace").string();
            auto name = item.at("name").string();
            if (!seen.emplace(ns, name).second) {
                throw std::range_error("Duplicate manifest component");
            }
            std::string template_namespace =
                item.has("templateNamespace")
                    ? item.at("templateNamespace").string()
                    : ns;
            std::string template_name =
                item.has("template") ? item.at("template").string() : "";
            std::vector<ui_component_property> properties;
            std::map<std::string, BindingValue> typed_defaults;
            std::map<std::string, ui_component_property_metadata> property_metadata;
            if (item.has("properties")) {
                for (const auto &property : item.at("properties").array()) {
                    ui_component_property p{};
                    p.name = property.at("name").string().c_str();
                    p.default_mode = UI_BIND_ONE_WAY;
                    auto kind = property.at("type").string();
                    if (kind == "string") {
                        p.default_value.kind = UI_VALUE_STRING;
                        p.default_value.string =
                            property.has("default")
                                ? property.at("default").string().c_str()
                                : "";
                    } else if (kind == "bool" || kind == "int") {
                        p.default_value.kind =
                            kind == "bool" ? UI_VALUE_BOOL : UI_VALUE_INT;
                        if (property.has("default")) {
                            const auto *integer = std::get_if<int64_t>(
                                &property.at("default").data);
                            if (!integer || (kind == "bool" && *integer != 0 &&
                                             *integer != 1)) {
                                throw std::range_error(
                                    "Invalid integer or boolean default");
                            }
                            p.default_value.integer = *integer;
                        }
                    } else if (kind == "double") {
                        p.default_value.kind = UI_VALUE_DOUBLE;
                        if (property.has("default")) {
                            const auto &number =
                                property.at("default").string();
                            if (!parse_number(number, p.default_value.number)) {
                                throw std::range_error(
                                    "Invalid double default");
                            }
                        }
                    } else if (kind == "command") {
                        p.default_value.kind = UI_VALUE_COMMAND;
                    } else if (kind == "model") {
                        p.default_value.kind = UI_VALUE_MODEL;
                    } else {
                        constexpr const char* typed_names[] = {"color", "thickness", "cornerRadius", "point", "size", "matrix", "length", "enum"};
                        auto found = std::find(std::begin(typed_names), std::end(typed_names), kind);
                        if (found == std::end(typed_names)) throw std::range_error("Unknown component property type");
                        const auto typed_kind = ui_value_kind(UI_VALUE_COLOR + (found - std::begin(typed_names)));
                        auto& owned = typed_defaults[p.name];
                        const auto fallback = typed_kind == UI_VALUE_COLOR ? "#00000000" : typed_kind == UI_VALUE_MATRIX ? "0,0,0,0,0,0" :
                            typed_kind == UI_VALUE_POINT || typed_kind == UI_VALUE_SIZE ? "0,0" : "0";
                        if (binding_parse_value(property.has("default") ? property.at("default").string() : fallback, typed_kind, owned) != UI_OK)
                            throw std::range_error("Invalid typed property default");
                        p.default_value = owned.value;
                    }
                    if (property.has("readOnly")) {
                        p.read_only = property.at("readOnly").number(0, 1);
                    }
                    if (property.has("mode")) {
                        auto mode = property.at("mode").string();
                        if (mode != "OneWay" && mode != "TwoWay" &&
                            mode != "OneTime") {
                            throw std::range_error(
                                "Unknown component binding mode");
                        }
                        p.default_mode = mode == "TwoWay"    ? UI_BIND_TWO_WAY
                                         : mode == "OneTime" ? UI_BIND_ONE_TIME
                                                             : UI_BIND_ONE_WAY;
                    }
                    if (property.has("flags")) {
                        auto& metadata = property_metadata[p.name];
                        metadata = {sizeof(metadata), 1, uint32_t(property.at("flags").number(0, 255)), nullptr, nullptr, nullptr};
                        p.metadata = &metadata;
                    }
                    properties.push_back(p);
                }
            }
            std::vector<const char *> events;
            if (item.has("events")) {
                for (const auto &e : item.at("events").array()) {
                    events.push_back(e.string().c_str());
                }
            }
            std::vector<ui_template_part> parts;
            if (item.has("parts")) {
                for (const auto &part : item.at("parts").array()) {
                    int kind = UI_STACK_PANEL;
                    for (; kind <= UI_PATH &&
                           part.at("kind").string() !=
                               devtools::control_name(ui_node_kind(kind));
                         ++kind) {
                    }
                    parts.push_back(
                        {part.at("name").string().c_str(), ui_node_kind(kind),
                         uint32_t(part.has("required")
                                      ? part.at("required").number(0, 1)
                                      : 1)});
                }
            }
            ui_component_type type{};
            type.struct_size = sizeof(type);
            type.version = UI_COMPONENT_TYPE_VERSION;
            type.namespace_uri = ns.c_str();
            type.name = name.c_str();
            type.template_namespace = template_namespace.c_str();
            type.template_name = template_name.c_str();
            type.properties = properties.data();
            type.property_count = properties.size();
            type.events = events.data();
            type.event_count = events.size();
            type.parts = parts.data();
            type.part_count = parts.size();
            // Runtime factories registered by the application take precedence
            // over schema-only declarations.
            const auto existing = registry.types.find({ns, name});
            if (existing == registry.types.end()) {
                if (ui_app_register_component_type(app, &type) != UI_OK) {
                    throw std::range_error("Invalid component manifest type");
                }
            } else {
                for (const auto &p : properties) {
                    auto found = std::find_if(
                        existing->second->properties.begin(),
                        existing->second->properties.end(),
                        [&](const auto &value) {
                            return value.name == p.name &&
                                   value.value.kind == p.default_value.kind &&
                                   (!p.metadata || value.metadata.flags == p.metadata->flags);
                        });
                    if (found == existing->second->properties.end()) {
                        throw std::range_error(
                            "Component metadata differs from registered type");
                    }
                }
            }
        }
        return UI_OK;
    } catch (const std::exception &error) {
        if (saved) {
            registry.types.swap(previous);
        }
        fleck::report({}, 0, UI_FLECK_VALUE, error.what(), *diagnostic);
        return UI_ERROR_INVALID_ARGUMENT;
    }
}

ui_node::~ui_node() { components::shutdown(*this); }

ui_result ui_app_validate_fleck(ui_app *app, const char *utf8, size_t length,
                                ui_fleck_diagnostic *diagnostic) {
    if (auto status = binding_app_check(app); status != UI_OK) {
        return status;
    }
    if (!utf8) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    ui_fleck_diagnostic local{};
    if (!diagnostic) {
        diagnostic = &local;
    }
    *diagnostic = {};
    try {
        fleck::Page page;
        if (!fleck::parse({utf8, length}, page, *diagnostic,components::registry(app).dictionaries)) {
            return UI_ERROR_INVALID_ARGUMENT;
        }
        if (page.document_kind == "ResourceDictionary") return UI_OK;
        ui_app_options options{sizeof(options), UI_API_VERSION, UI_SOFTWARE,
                               UI_HEADLESS};
        ui_app *probe = nullptr;
        if (auto status = ui_app_create(&options, &probe); status != UI_OK) {
            return status;
        }
        std::unique_ptr<ui_app, decltype(&ui_app_destroy)> owner(
            probe, ui_app_destroy);
        auto &target = components::registry(probe);
        target.documents = components::registry(app).documents;
        for (const auto &[key, type] : components::registry(app).types) {
            auto copy = std::make_shared<components::Type>(*type);
            copy->lifecycle = nullptr;
            copy->changed = nullptr;
            copy->context = nullptr;
            for (auto& property : copy->properties) {
                property.metadata.validate = nullptr;
                property.metadata.coerce = nullptr;
                property.metadata.context = nullptr;
            }
            target.types.emplace(key, std::move(copy));
        }
        if (page.document_kind != "Window") {
            const components::Key definition{
                page.type_namespace,
                page.class_name.substr(page.class_name.find_last_of('.') + 1)};
            fleck::Element host;
            host.kind = UI_PANEL;
            host.location = page.root.location;
            if (page.document_kind == "ControlTemplate") {
                unsigned kind = 0;
                while (kind <= UI_PATH && page.target_name != devtools::control_name(ui_node_kind(kind))) ++kind;
                if (page.target_namespace == "urn:sunfleck:v1" && kind <= UI_PATH && fleck::supports_template(ui_node_kind(kind))) {
                    host.kind = ui_node_kind(kind);
                } else {
                    host.type_namespace = page.target_namespace;
                    host.type_name = page.target_name;
                }
                host.template_namespace = definition.first;
                host.template_name = definition.second;
            } else {
                host.type_namespace = definition.first;
                host.type_name = definition.second;
            }
            target.documents.insert_or_assign(definition, std::move(page));
            page = {};
            page.root = std::move(host);
        }
        return fleck::install(probe, std::move(page), {utf8, length},
                              *diagnostic, false);
    } catch (const std::bad_alloc &) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}

ui_result ui_app_has_component_type(ui_app *app, const char *namespace_uri,
                                    const char *name, uint32_t *registered) {
    if (auto status = binding_app_check(app); status != UI_OK) {
        return status;
    }
    if (!namespace_uri || !name || !registered) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    *registered =
        components::registry(app).types.contains({namespace_uri, name});
    return UI_OK;
}
ui_result
ui_app_load_fleck_with_resources(ui_app *app, const char *utf8, size_t length,
                                 const ui_fleck_resource_entry *entries,
                                 size_t count, ui_model *model,
                                 ui_fleck_diagnostic *diagnostic) {
    if (auto status = fleck::can_load(app); status != UI_OK) {
        return status;
    }
    try {
        auto previous = components::registry(app).documents;
        auto dictionaries = components::registry(app).dictionaries;
        auto status =
            ui_app_register_fleck_resources(app, entries, count, diagnostic);
        if (status == UI_OK) {
            status = ui_app_load_fleck_with_model(app, utf8, length, model,
                                                  nullptr, diagnostic);
        }
        if (status != UI_OK) {
            components::registry(app).documents.swap(previous);
            components::registry(app).dictionaries.swap(dictionaries);
        }
        return status;
    } catch (const std::bad_alloc &) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}
