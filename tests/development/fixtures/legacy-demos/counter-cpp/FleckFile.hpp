#pragma once
#include "CounterModel.hpp"
#include "counter_bindings.hpp"
#include <filesystem>
#include <fstream>
#include <optional>

class FleckFile {
public:
    FleckFile(sunfleck::App& app, sunfleck::FleckPage<CounterModel>& bindings, const std::filesystem::path& path)
        : app_(app), bindings_(bindings), path_(std::filesystem::absolute(path))
    {
        auto source = read();
        if (!source) { throw std::runtime_error("Cannot read Fleck page: " + path_.string()); }
        observed_ = *source;
        bindings_.load(observed_);
        const auto utf8 = path_.generic_u8string();
        const std::string uri(utf8.begin(), utf8.end());
        app_.setFleckSourceUri(uri.c_str());
    }

    void checkForChanges()
    {
        // Missing/temporarily locked files during an editor's atomic save are retried.
        if (auto source = read(); source && *source != observed_) {
            app_.queueFleckRefresh(*source, bindings_.properties().nativeHandle());
            observed_ = std::move(*source);
        }
    }

private:
    std::optional<std::string> read() const
    {
        std::ifstream file(path_, std::ios::binary);
        if (!file) { return {}; }
        std::string source(1024 * 1024 + 1, '\0');
        file.read(source.data(), static_cast<std::streamsize>(source.size()));
        if (file.bad()) { return {}; }
        source.resize(static_cast<size_t>(file.gcount()));
        if (source.size() > 1024 * 1024) { return {}; }
        return source;
    }
    sunfleck::App& app_;
    sunfleck::FleckPage<CounterModel>& bindings_;
    std::filesystem::path path_;
    std::string observed_;
};
