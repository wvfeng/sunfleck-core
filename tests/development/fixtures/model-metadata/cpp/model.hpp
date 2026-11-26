#pragma once
#include <sunfleck/property_model.hpp>

struct CounterModel {
    int64_t count = 0;
    unsigned reads = 0;
    void registerProperties(sunfleck::PropertyModel& properties)
    {
        properties.property("Counter.Value", [&] { ++reads; return count; }, [&](int64_t value) { count = value; });
        properties.property("Status.Ready", [] { return true; });
    }
};
