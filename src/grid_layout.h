#pragma once

#include "numeric_literal.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace grid_layout {
inline bool parse_z_index(std::string_view value, int32_t& result) {
    auto end = value.data()+value.size();
    auto parsed = std::from_chars(value.data(),end,result);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}
enum class Unit { pixels, automatic, star };
struct Track {
    Unit unit = Unit::star;
    double value = 1;
    std::string group;
    bool operator==(const Track&) const = default;
};
struct Cell {
    unsigned row = 0, column = 0, rows = 1, columns = 1;
    bool operator==(const Cell&) const = default;
};

inline bool valid_group(std::string_view name) {
    if (name.empty() || name.size() > 64) return false;
    const auto letter = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; };
    return letter(name.front()) && std::all_of(name.begin(), name.end(), [&](char c) { return letter(c) || (c >= '0' && c <= '9'); });
}

inline bool parse_tracks(std::string_view text, std::vector<Track>& result) {
    std::vector<Track> tracks;
    while (true) {
        const auto comma = text.find(',');
        auto token = text.substr(0, comma);
        while (!token.empty() && token.front() == ' ') token.remove_prefix(1);
        while (!token.empty() && token.back() == ' ') token.remove_suffix(1);
        if (token.empty() || tracks.size() == 256) return false;
        Track track;
        const auto suffix = token.find('@');
        if (suffix != token.npos) {
            const auto name = token.substr(suffix + 1);
            if (!valid_group(name)) return false;
            track.group = name;
            token = token.substr(0, suffix);
            if (token.empty()) return false;
        }
        if (token == "Auto") track.unit = Unit::automatic;
        else {
            const bool star = token.back() == '*';
            track.unit = star ? Unit::star : Unit::pixels;
            if (star) token.remove_suffix(1);
            if (!token.empty()) {
                if (!numeric_literal::real_literal(token, track.value) ||
                    track.value < 0 || track.value > 10000000 || (star && track.value == 0)) return false;
            }
        }
        tracks.push_back(track);
        if (comma == text.npos) break;
        text.remove_prefix(comma + 1);
    }
    result.swap(tracks);
    return true;
}

struct Settings {
    std::vector<Track> rows, columns;
    Cell cell;
};
inline bool attribute(std::string_view name) {
    return name == "RowDefinitions" || name == "ColumnDefinitions" || name == "Grid.Row" ||
        name == "Grid.Column" || name == "Grid.RowSpan" || name == "Grid.ColumnSpan";
}
inline bool assign(Settings& settings, bool grid, std::string_view name, std::string_view value) {
    if (name == "RowDefinitions" || name == "ColumnDefinitions")
        return grid && parse_tracks(value, name == "RowDefinitions" ? settings.rows : settings.columns);
    unsigned number = 0;
    auto end = value.data()+value.size();
    auto parsed = std::from_chars(value.data(),end,number);
    if (parsed.ec != std::errc{} || parsed.ptr != end) return false;
    if (name == "Grid.Row" || name == "Grid.Column") {
        if (number >= 256) return false;
        (name == "Grid.Row" ? settings.cell.row : settings.cell.column) = number;
    } else {
        if (number == 0 || number > 256) return false;
        (name == "Grid.RowSpan" ? settings.cell.rows : settings.cell.columns) = number;
    }
    return true;
}
struct Demand { unsigned index, span; double size; };
struct Axis {
    std::vector<double> starts, ends;
    std::pair<double,double> range(unsigned index, unsigned span) const {
        const size_t first = std::min<size_t>(index, starts.size()-1);
        const size_t last = first + std::min<size_t>(span, starts.size()-first) - 1;
        return {starts[first], ends[last]};
    }
};

inline Axis resolve(std::span<const Track> tracks, std::vector<Demand> demands,
                    double extent, double gap, double scale) {
    const Track implicit{};
    if (tracks.empty()) tracks = std::span(&implicit, 1);
    std::vector<double> sizes(tracks.size());
    double weight = 0;
    for (size_t i = 0; i < tracks.size(); ++i) {
        if (tracks[i].unit == Unit::pixels) sizes[i] = tracks[i].value * scale;
        if (tracks[i].unit == Unit::star) weight += tracks[i].value;
    }
    std::stable_sort(demands.begin(), demands.end(), [](const Demand& a, const Demand& b) {
        return a.span < b.span;
    });
    for (auto item : demands) {
        const size_t first = std::min<size_t>(item.index, sizes.size()-1);
        const size_t count = std::min<size_t>(item.span, sizes.size()-first);
        double used = gap * (count-1);
        size_t flexible = 0;
        const auto grows = [&](size_t i) {
            return tracks[i].unit == Unit::automatic || (!std::isfinite(extent) && tracks[i].unit == Unit::star);
        };
        for (size_t i = first; i < first+count; ++i) {
            used += sizes[i];
            if (grows(i)) ++flexible;
        }
        if (flexible && item.size > used) {
            const double extra = (item.size-used)/flexible;
            for (size_t i = first; i < first+count; ++i) if (grows(i)) sizes[i] += extra;
        }
    }
    if (std::isfinite(extent) && weight > 0) {
        double remaining = extent - gap*(sizes.size()-1);
        for (double size : sizes) remaining -= size;
        for (size_t i = 0; i < sizes.size(); ++i)
            if (tracks[i].unit == Unit::star) sizes[i] = std::max(0.0, remaining)*tracks[i].value/weight;
    }
    Axis result;
    double position = 0;
    for (double size : sizes) {
        result.starts.push_back(position);
        result.ends.push_back(position+size);
        position += size+gap;
    }
    return result;
}
}
