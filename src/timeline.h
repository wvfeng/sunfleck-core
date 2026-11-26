#pragma once
#include "property_value.h"
#include <chrono>
#include <vector>

namespace animation {
using Clock = std::chrono::steady_clock;
using Milliseconds = std::chrono::duration<double, std::milli>;
enum class Easing { linear, ease_in, ease_out, ease_in_out };

struct KeyFrame {
    double position;
    BindingValue value;
    Easing easing = Easing::linear;
};

inline double ease(Easing easing, double t) {
    switch (easing) {
    case Easing::ease_in: return t*t;
    case Easing::ease_out: return t*(2-t);
    case Easing::ease_in_out: return t < 0.5 ? 2*t*t : 1-2*(1-t)*(1-t);
    default: return t;
    }
}

inline BindingValue interpolate(const BindingValue& from, const BindingValue& to, double t) {
    BindingValue result = from;
    const auto kind = from.value.kind;
    const auto mix = [t](double a, double b) { return std::lerp(a,b,t); };
    if (kind == UI_VALUE_COLOR) {
        uint32_t color = 0;
        for (unsigned shift = 0; shift < 32; shift += 8) {
            const auto a = (uint32_t(from.value.integer) >> shift) & 255;
            const auto b = (uint32_t(to.value.integer) >> shift) & 255;
            color |= uint32_t(std::lround(mix(a,b))) << shift;
        }
        result.value.integer = color;
    } else if (const auto count = property_values::dimensions(kind)) {
        for (unsigned i = 0; i < count; ++i) result.geometry[i] = mix(from.geometry[i],to.geometry[i]);
    } else if (kind == UI_VALUE_INT) {
        result.value.integer = std::llround(mix(double(from.value.integer),double(to.value.integer)));
    } else {
        result.value.number = mix(from.value.number,to.value.number);
    }
    return result;
}

struct Timeline {
    std::vector<KeyFrame> frames;
    double duration_ms = 1;
    double delay_ms = 0;
    uint32_t repeat = 1; // Zero repeats indefinitely.
    bool auto_reverse = false;

    struct Sample { BindingValue value; bool completed; };
    Sample sample(double elapsed_ms) const {
        const double active = std::max(0., elapsed_ms-delay_ms);
        const double cycle = duration_ms * (auto_reverse ? 2 : 1);
        const bool completed = repeat && active >= cycle*repeat;
        double position = completed ? (auto_reverse ? 0. : 1.) : std::fmod(active,cycle)/duration_ms;
        if (position > 1) position = 2-position;
        auto next = std::upper_bound(frames.begin(),frames.end(),position,
            [](double value,const KeyFrame& frame) { return value < frame.position; });
        if (next == frames.end()) return {frames.back().value,completed};
        if (next == frames.begin()) return {next->value,completed};
        const auto& prev = *(next-1);
        const double fraction = (position-prev.position)/(next->position-prev.position);
        return {interpolate(prev.value,next->value,ease(next->easing,fraction)),completed};
    }
};
struct Track {
    ui_property property{};
    Timeline timeline;
    bool hold = false;
};
}
