#pragma once
#include <span>
#include <sunfleck/property_model.hpp>

namespace sunfleck {
inline ui_component_property_metadata componentPropertyMetadata(uint32_t flags,
    ui_component_property_validate validate = nullptr, ui_component_property_coerce coerce = nullptr, void* context = nullptr) {
    return {sizeof(ui_component_property_metadata), 1, flags, validate, coerce, context};
}
template<class T> struct ComponentAttribute { const char *name; bool bindable; };
/// Borrowed component instance. The owning page and App must outlive this
/// wrapper.
class FrameworkElement {
  public:
    inline static constexpr ComponentAttribute<ui_model *> DataContextProperty{"DataContext", true};
    explicit FrameworkElement(ui_component *component) : component_(component) {}
    static FrameworkElement create(App& app, const char* namespace_uri, const char* name, PropertyModel* model = nullptr) {
        ui_component* component = nullptr;
        ui_fleck_diagnostic diagnostic{};
        detail::check(ui_component_create(app.nativeHandle(), namespace_uri, name, model ? model->nativeHandle() : nullptr, &component, &diagnostic));
        return FrameworkElement(component);
    }
    explicit FrameworkElement(Node node) {
        detail::check(ui_node_get_component(node.nativeHandle(), &component_));
    }
    ui_component *nativeHandle() const noexcept { return component_; }
    Node node() const {
        if (node_) { return Node(node_); }
        ui_node *value = nullptr;
        detail::check(ui_component_get_node(component_, &value));
        return Node(value);
    }
    Node findNode(const char *id) const {
        ui_node *value = nullptr;
        detail::check(ui_component_find_node(component_, id, &value));
        return Node(value);
    }
    template <class T> T get(const char *name) const {
        ui_binding_value value{};
        detail::check(ui_component_get_property(component_, name, &value));
        return PropertyModel::decode<T>(value);
    }

    template <class T> void set(const char *name, T &&value) {
        ui_binding_value encoded{};
        std::string text;
        PropertyModel::encode(std::forward<T>(value), encoded, text);
        detail::check(ui_component_set_property(component_, name, &encoded));
    }
    template<class T> T baseValue(const char* name) const {
        ui_binding_value value{};
        detail::check(ui_component_get_base_property(component_, name, &value));
        return PropertyModel::decode<T>(value);
    }
    template<class T> void setLayer(const char* name, ui_value_precedence precedence, const T& value) {
        ui_binding_value encoded{}; std::string text;
        PropertyModel::encode(value, encoded, text);
        detail::check(ui_component_set_property_layer(component_, name, precedence, &encoded));
    }
    void clearLayer(const char* name, ui_value_precedence precedence = UI_PRECEDENCE_LOCAL) {
        detail::check(ui_component_clear_property_layer(component_, name, precedence));
    }
    ui_property_source valueSource(const char* name) const {
        ui_property_source source{}; detail::check(ui_component_get_property_source(component_, name, &source)); return source;
    }
    ui_component_property_descriptor describeProperty(const char* name) const {
        ui_component_property_descriptor descriptor{}; descriptor.struct_size = sizeof(descriptor); descriptor.version = 1;
        detail::check(ui_component_describe_property(component_, name, &descriptor)); return descriptor;
    }
    void bind(const char *name, PropertyModel &model, const char *path,
              ui_binding_mode mode = UI_BIND_DEFAULT) {
        detail::check(ui_component_bind_property(
            component_, name, model.nativeHandle(), path, mode));
    }
    void setDataContext(PropertyModel *model) {
        detail::check(ui_component_set_data_context(
            component_, model ? model->nativeHandle() : nullptr));
    }
    ui_model *dataContext() const {
        ui_model *value = nullptr;
        detail::check(ui_component_get_data_context(component_, &value));
        return value;
    }
    void clearDataContext() { detail::check(ui_component_clear_data_context(component_)); }
    class Subscription {
        struct State {
            std::function<void(FrameworkElement, const ui_binding_value *)> handler;
        };
        ui_component_subscription *subscription_ = nullptr;
        std::unique_ptr<State> state_;

      public:
        Subscription(
            ui_component *component, const char *event,
            std::function<void(FrameworkElement, const ui_binding_value *)> handler)
            : state_(std::make_unique<State>(State{std::move(handler)})) {
            detail::check(ui_component_subscribe_scoped(
                component, event,
                [](ui_component *c, const ui_binding_value *value,
                   void *context) {
                    static_cast<State *>(context)->handler(FrameworkElement(c),
                                                           value);
                },
                state_.get(), &subscription_));
        }
        Subscription(Subscription &&other) noexcept
            : subscription_(std::exchange(other.subscription_, nullptr)),
              state_(std::move(other.state_)) {}
        Subscription(const Subscription &) = delete;
        ~Subscription() {
            (void)ui_component_subscription_destroy(subscription_);
        }
    };
    Subscription subscribe(
        const char *event,
        std::function<void(FrameworkElement, const ui_binding_value *)> handler) {
        return Subscription(component_, event, std::move(handler));
    }
    template <class T> void raise(const char *event, T &&value) {
        ui_binding_value encoded{};
        std::string text;
        PropertyModel::encode(std::forward<T>(value), encoded, text);
        detail::check(ui_component_raise(component_, event, &encoded));
    }

  protected:
    // Built-in elements own no component host; they wrap an ordinary node.
    FrameworkElement(Node node, std::nullptr_t) : node_(node.nativeHandle()) {}
    ui_node *node_ = nullptr;
    ui_component *component_ = nullptr;
};
class Control : public FrameworkElement {
  public:
    inline static constexpr ComponentAttribute<std::string> TemplateProperty{"Template", false};
    using FrameworkElement::FrameworkElement;
    Node findTemplatePart(const char *name) const {
        ui_node *value = nullptr;
        detail::check(
            ui_component_find_template_part(component_, name, &value));
        return Node(value);
    }
    void applyTemplate(const char *namespace_uri, const char *name) {
        ui_fleck_diagnostic diagnostic{};
        detail::check(ui_component_apply_template(component_, namespace_uri,
                                                  name, &diagnostic));
    }
};
class UserControl : public Control {
  public:
    using Control::Control;
};
template <class T>
void registerComponentClass(App &app, ui_component_type type) {
    type.lifecycle = [](ui_component *component, ui_component_phase phase,
                        void *) noexcept -> ui_result {
        try {
            if (phase == UI_COMPONENT_INITIALIZE) {
                auto instance = std::make_unique<T>(component);
                detail::check(ui_component_set_state(
                    component, instance.get(),
                    [](void *state) { delete static_cast<T *>(state); }));
                auto *value = instance.release();
                if constexpr (requires { value->onInitialize(); }) {
                    value->onInitialize();
                }
                return UI_OK;
            }
            void *state = nullptr;
            detail::check(ui_component_get_state(component, &state));
            auto *value = static_cast<T *>(state);
            if (phase == UI_COMPONENT_MOUNT) {
                if constexpr (requires { value->onMounted(); }) {
                    value->onMounted();
                }
            }
            if (phase == UI_COMPONENT_UNMOUNT) {
                if constexpr (requires { value->onUnmounted(); }) {
                    value->onUnmounted();
                }
            }
            if (phase == UI_COMPONENT_DESTROY) {
                if constexpr (requires { value->onDestroyed(); }) {
                    value->onDestroyed();
                }
            }
            if (phase == UI_COMPONENT_TEMPLATE_APPLIED) {
                if constexpr (requires { value->onTemplateApplied(); }) {
                    value->onTemplateApplied();
                }
            }
            return UI_OK;
        } catch (...) {
            return UI_ERROR_INVALID_STATE;
        }
    };
    type.changed = [](ui_component *component, const char *name, void *) {
        void *state = nullptr;
        detail::check(ui_component_get_state(component, &state));
        auto *value = static_cast<T *>(state);
        if constexpr (requires { value->onPropertyChanged(name); }) {
            value->onPropertyChanged(name);
        }
    };
    detail::check(ui_app_register_component_type(app.nativeHandle(), &type));
}
inline void registerComponentResource(App &app,
                                      std::span<const std::byte> bytes) {
    ui_fleck_diagnostic diagnostic{};
    detail::check(ui_app_register_component_resource(
        app.nativeHandle(), bytes.data(), bytes.size(), &diagnostic));
}
inline void registerComponent(App &app, const ui_component_type &type) {
    detail::check(ui_app_register_component_type(app.nativeHandle(), &type));
}
inline void
registerFleckResources(App &app,
                       std::span<const ui_fleck_resource_entry> resources) {
    ui_fleck_diagnostic diagnostic{};
    detail::check(ui_app_register_fleck_resources(
        app.nativeHandle(), resources.data(), resources.size(), &diagnostic));
}
} // namespace sunfleck
