#pragma once
#include <sunfleck/ui.hpp>
#include <exception>
#include <functional>
#include <limits>
#include <map>
#include <span>
#include <string_view>
#include <type_traits>
#include <algorithm>
#include <source_location>
#include <cstring>

namespace sunfleck {
using PropertyChangedHandler = std::function<void(std::string_view)>;

class PropertyChangedConnection {
public:
    PropertyChangedConnection() = default;
    explicit PropertyChangedConnection(std::function<void()> disconnect) : disconnect_(std::move(disconnect)) {}
    PropertyChangedConnection(PropertyChangedConnection&& other) noexcept : disconnect_(std::exchange(other.disconnect_, {})) {}
    PropertyChangedConnection& operator=(PropertyChangedConnection&& other) noexcept
    {
        if (this != &other) { reset(); disconnect_ = std::exchange(other.disconnect_, {}); }
        return *this;
    }
    PropertyChangedConnection(const PropertyChangedConnection&) = delete;
    PropertyChangedConnection& operator=(const PropertyChangedConnection&) = delete;
    ~PropertyChangedConnection() { reset(); }
    void reset() noexcept { if (auto disconnect = std::exchange(disconnect_, {})) { disconnect(); } }
private:
    std::function<void()> disconnect_;
};

// Use each event on its model's thread. Copies/snapshots never copy observers; assignment preserves
// the destination's subscriptions when an async model snapshot is committed.
class PropertyChangedEvent {
    struct State { size_t next = 0; std::map<size_t, PropertyChangedHandler> handlers; };
public:
    PropertyChangedEvent() = default;
    PropertyChangedEvent(const PropertyChangedEvent&) {}
    PropertyChangedEvent(PropertyChangedEvent&&) {}
    PropertyChangedEvent& operator=(const PropertyChangedEvent&) noexcept { return *this; }
    PropertyChangedEvent& operator=(PropertyChangedEvent&&) noexcept { return *this; }
    PropertyChangedConnection subscribe(PropertyChangedHandler handler)
    {
        if (!handler) { throw Error(UI_ERROR_INVALID_ARGUMENT); }
        const auto key = state_->next++;
        state_->handlers.emplace(key, std::move(handler));
        return PropertyChangedConnection([weak = std::weak_ptr<State>(state_), key] {
            if (auto state = weak.lock()) { state->handlers.erase(key); }
        });
    }
    void notify(std::string_view path = {})
    {
        // Snapshot IDs, not callbacks: a preceding handler may unsubscribe another.
        std::vector<size_t> ids;
        for (const auto& [id, handler] : state_->handlers) { (void)handler; ids.push_back(id); }
        for (auto id : ids) {
            auto found = state_->handlers.find(id);
            if (found != state_->handlers.end()) { auto handler = found->second; handler(path); }
        }
    }
private:
    std::shared_ptr<State> state_ = std::make_shared<State>();
};

template<class T>
concept NotifiesPropertyChanged = requires(T& model, PropertyChangedHandler handler) {
    { model.subscribePropertyChanged(std::move(handler)) } -> std::same_as<PropertyChangedConnection>;
};

class PropertyModel {
    struct Entry {
        PropertyModel& owner;
        std::function<void(ui_binding_value&, std::string&)> read;
        std::function<void(const ui_binding_value&)> write;
        std::string text;
        std::string path;
    };
public:
    explicit PropertyModel(App& app) { detail::check(ui_model_create(app.nativeHandle(), &model_)); }
    ~PropertyModel() { if (model_) { (void)ui_model_destroy(model_); } }
    PropertyModel(const PropertyModel&) = delete;
    PropertyModel& operator=(const PropertyModel&) = delete;
    ui_model* nativeHandle() const noexcept { return model_; }
    void enableTextCollection() { detail::check(ui_model_enable_text_collection(model_)); }
    void addConverter(const char* name,ui_value_kind output,ui_binding_convert convert,void* context=nullptr) {
        detail::check(ui_model_add_converter(model_,name,output,convert,context));
    }
    void addMultiBinding(const char* path,const char* converter,std::span<const char* const> inputs) {
        if (inputs.size()>64) throw Error(UI_ERROR_INVALID_ARGUMENT);
        detail::check(ui_model_add_multi_binding(model_,path,converter,inputs.data(),static_cast<uint32_t>(inputs.size())));
    }
    void setCollectionView(PropertyModel& source, ui_collection_sort sort = UI_COLLECTION_SOURCE_ORDER, const char* filter = "") {
        detail::check(ui_collection_set_view(model_,source.nativeHandle(),sort,filter));
    }
    void setGroupByLabel(bool enabled) { detail::check(ui_collection_set_group_by_label(model_,enabled)); }
    uint32_t groupCount() const {
        uint32_t count{};
        detail::check(ui_collection_group_count(model_,&count));
        return count;
    }
    struct Group { std::string key; uint32_t first, count; };
    Group group(uint32_t index) const {
        const char* key{};
        uint32_t first{}, count{};
        detail::check(ui_collection_group(model_,index,&key,&first,&count));
        return {key,first,count};
    }
    uint32_t itemCount() const {
        uint32_t count{};
        detail::check(ui_collection_size(model_, &count));
        return count;
    }
    std::pair<uint64_t,std::string> item(uint32_t index) const {
        uint64_t identity{};
        const char* text{};
        detail::check(ui_collection_item(model_, index, &identity, &text));
        return {identity,text};
    }
    void spliceItems(uint32_t index, uint32_t removed, std::span<const char* const> inserted) {
        if (inserted.size() > UINT32_MAX) throw Error(UI_ERROR_INVALID_ARGUMENT);
        detail::check(ui_collection_splice(model_, index, removed, inserted.data(), static_cast<uint32_t>(inserted.size())));
    }
    void spliceModels(uint32_t index, uint32_t removed, std::span<const char* const> labels,
                      std::span<ui_model* const> contexts) {
        if (labels.size() != contexts.size() || labels.size() > UINT32_MAX) throw Error(UI_ERROR_INVALID_ARGUMENT);
        detail::check(ui_collection_splice_models(model_,index,removed,labels.data(),contexts.data(),static_cast<uint32_t>(labels.size())));
    }
    ui_model* itemModel(uint32_t index) const {
        ui_model* context{};
        detail::check(ui_collection_item_model(model_,index,&context));
        return context;
    }
    void moveItems(uint32_t index, uint32_t count, uint32_t destination) {
        detail::check(ui_collection_move(model_, index, count, destination));
    }
    std::string describe(const char* name) const
    {
        return detail::json([&](char* value, size_t* size) { return ui_model_describe(model_, name, value, size); });
    }
    bool running() const { uint32_t value = 0; detail::check(ui_model_is_running(model_, &value)); return value != 0; }
    void setCommandModes(const char* path, uint32_t modes)
    {
        detail::check(ui_model_set_command_modes(model_, path, modes));
    }
    template<class T>
    static void encode(T&& input, ui_binding_value& value, std::string& text)
    {
        using V = std::remove_cvref_t<T>;
        value = {};
        value.kind = kind<V>();
        if constexpr (kind<V>() == UI_VALUE_STRING) { text = std::string(input); value.string = text.c_str(); }
        else if constexpr (kind<V>() == UI_VALUE_COMMAND) { value.command = input; }
        else if constexpr (kind<V>() == UI_VALUE_MODEL) { value.model = input; }
        else if constexpr (kind<V>() == UI_VALUE_DOUBLE) { value.number = input; }
        else if constexpr (kind<V>() == UI_VALUE_COLOR) { value.integer = input.argb; }
        else if constexpr (kind<V>() == UI_VALUE_LENGTH) { value.number = input.value; value.integer = input.unit; }
        else if constexpr (kind<V>() >= UI_VALUE_THICKNESS && kind<V>() <= UI_VALUE_MATRIX) { value.geometry = reinterpret_cast<const double*>(&input); }
        else { value.integer = static_cast<int64_t>(input); }
    }
    template<class T> static T decode(const ui_binding_value& value) {
        if (value.kind != kind<T>()) throw Error(UI_ERROR_INVALID_ARGUMENT);
        if constexpr (kind<T>() == UI_VALUE_STRING) return T(value.string);
        else if constexpr (kind<T>() == UI_VALUE_COMMAND) return value.command;
        else if constexpr (kind<T>() == UI_VALUE_MODEL) return value.model;
        else if constexpr (kind<T>() == UI_VALUE_DOUBLE) return static_cast<T>(value.number);
        else if constexpr (kind<T>() == UI_VALUE_COLOR) return {static_cast<uint32_t>(value.integer)};
        else if constexpr (kind<T>() == UI_VALUE_LENGTH) return {value.number, static_cast<ui_length_unit>(value.integer)};
        else if constexpr (kind<T>() >= UI_VALUE_THICKNESS && kind<T>() <= UI_VALUE_MATRIX) {
            if (!value.geometry) throw Error(UI_ERROR_INVALID_ARGUMENT);
            T result{}; std::memcpy(&result, value.geometry, sizeof(T)); return result;
        } else {
            if constexpr (std::integral<T> && !std::same_as<T, bool>) {
                if (static_cast<long double>(value.integer) < std::numeric_limits<T>::lowest() ||
                    static_cast<long double>(value.integer) > std::numeric_limits<T>::max()) throw Error(UI_ERROR_INVALID_ARGUMENT);
            }
            return static_cast<T>(value.integer);
        }
    }
    template<class T> static T parse(const char* literal) {
        ui_binding_value value{};
        double geometry[6]{};
        detail::check(ui_value_parse(kind<T>(), literal, &value, geometry, 6));
        return decode<T>(value);
    }
    void textSource(const char* path, const TextSource& source)
    {
        if (!path) { throw Error(UI_ERROR_INVALID_ARGUMENT); }
        std::string name(path);
        adapters_.reserve(adapters_.size() + 1);
        detail::check(ui_model_add_text_source(model_, path, source.nativeHandle()));
        adapters_.push_back(std::move(name));
    }
    void command(const char* path, ui_command* command)
    {
        if (!path) { throw Error(UI_ERROR_INVALID_ARGUMENT); }
        std::string name(path);
        adapters_.reserve(adapters_.size() + 1);
        detail::check(ui_model_add_command(model_, path, command));
        adapters_.push_back(std::move(name));
    }
    void notifyIfRegistered(std::string_view path)
    {
        if (path.empty() || std::any_of(entries_.begin(), entries_.end(), [&](const auto& e) { return e->path == path; }) ||
            std::find(adapters_.begin(), adapters_.end(), path) != adapters_.end()) { notify(path); }
    }

    template<class Read>
    void property(const char* path, Read read, std::source_location source = std::source_location::current())
    {
        add(path, std::move(read), nullptr);
        detail::check(ui_model_set_property_source(model_, path, source.file_name(), source.line(), source.column()));
    }
    template<class Read, class Write>
    void property(const char* path, Read read, Write write, std::source_location source = std::source_location::current())
    {
        add(path, std::move(read), std::move(write));
        detail::check(ui_model_set_property_source(model_, path, source.file_name(), source.line(), source.column()));
    }
    void notify(std::string_view path = {})
    {
        const std::string name(path);
        detail::check(ui_model_notify_property_changed(model_, name.c_str()));
    }
    void beginUpdate() { detail::check(ui_model_begin_update(model_)); }
    void endUpdate() { detail::check(ui_model_end_update(model_)); }
    std::exception_ptr takeError() noexcept { return std::exchange(error_, {}); }
    void bind(Node node, ui_property property, const char* path, ui_binding_mode mode = UI_BIND_DEFAULT,
              ui_update_source_trigger trigger = UI_UPDATE_PROPERTY_CHANGED, const char* format = "")
    {
        const ui_binding_options options{sizeof(options), 1, property, path, mode, trigger, format};
        detail::check(ui_node_bind_property(node.nativeHandle(), model_, &options));
    }
public:
    template<class T> static constexpr ui_value_kind kind()
    {
        if constexpr (std::same_as<T, bool>) { return UI_VALUE_BOOL; }
        else if constexpr (std::integral<T>) { return UI_VALUE_INT; }
        else if constexpr (std::floating_point<T>) { return UI_VALUE_DOUBLE; }
        else if constexpr (std::same_as<T, ui_command*>) { return UI_VALUE_COMMAND; }
        else if constexpr (std::same_as<T, ui_model*>) { return UI_VALUE_MODEL; }
        else if constexpr (std::same_as<T, ui_color>) return UI_VALUE_COLOR;
        else if constexpr (std::same_as<T, ui_thickness>) return UI_VALUE_THICKNESS;
        else if constexpr (std::same_as<T, ui_corner_radius>) return UI_VALUE_CORNER_RADIUS;
        else if constexpr (std::same_as<T, ui_point>) return UI_VALUE_POINT;
        else if constexpr (std::same_as<T, ui_size>) return UI_VALUE_SIZE;
        else if constexpr (std::same_as<T, ui_matrix>) return UI_VALUE_MATRIX;
        else if constexpr (std::same_as<T, ui_length>) return UI_VALUE_LENGTH;
        else if constexpr (std::is_enum_v<T>) return UI_VALUE_ENUM;
        else { static_assert(std::convertible_to<T, std::string_view>, "Unsupported property type"); return UI_VALUE_STRING; }
    }
private:
    template<class Read, class Write>
    void add(const char* path, Read read, Write write)
    {
        if (!path) { throw Error(UI_ERROR_INVALID_ARGUMENT); }
        using T = std::remove_cvref_t<std::invoke_result_t<Read>>;
        auto entry = std::make_unique<Entry>(Entry{*this, [read = std::move(read), storage = T{}](ui_binding_value& value, std::string& text) mutable {
            storage = std::invoke(read);
            encode(storage, value, text);
        }, {}, {}, path});
        if constexpr (!std::same_as<Write, std::nullptr_t>) {
            entry->write = [write = std::move(write)](const ui_binding_value& value) { std::invoke(write, decode<T>(value)); };
        }
        const ui_model_property descriptor{sizeof(descriptor), 1, path, kind<T>(),
            [](void* context, ui_binding_value* value) noexcept -> ui_result {
                auto& entry = *static_cast<Entry*>(context);
                try { entry.read(*value, entry.text); return UI_OK; }
                catch (...) { entry.owner.error_ = std::current_exception(); return UI_ERROR_INVALID_STATE; }
            }, entry->write ? +[](void* context, const ui_binding_value* value) noexcept -> ui_result {
                auto& entry = *static_cast<Entry*>(context);
                try { entry.write(*value); return UI_OK; }
                catch (...) { entry.owner.error_ = std::current_exception(); return UI_ERROR_INVALID_STATE; }
            } : nullptr, entry.get()};
        entries_.reserve(entries_.size() + 1);
        detail::check(ui_model_add_property(model_, &descriptor));
        entries_.push_back(std::move(entry));
    }
    ui_model* model_ = nullptr;
    std::vector<std::unique_ptr<Entry>> entries_;
    std::vector<std::string> adapters_;
    std::exception_ptr error_;
};
}
