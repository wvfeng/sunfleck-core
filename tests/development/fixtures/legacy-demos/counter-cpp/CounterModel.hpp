#pragma once
#include <sunfleck/property_model.hpp>
#include <chrono>
#include <thread>

class CounterModel {
public:
    explicit CounterModel(sunfleck::App& app) : app_(app) {}

    const std::string Caption = "同步和异步各等待 100 毫秒；异步等待期间界面仍可操作。";
    int SyncCount() const { return syncCount_; }
    int AsyncCount() const { return asyncCount_; }

    void Increment()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ++syncCount_;
        changed_.notify("SyncCount");
    }

    void IncrementAsync(sunfleck::CancellationToken token)
    {
        for (int elapsed = 0; elapsed < 100; elapsed += 10) {
            token.throwIfCancelled();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        app_.invokeAsync([this] {
            ++asyncCount_;
            changed_.notify("AsyncCount");
        }, token).get();
    }

    auto subscribePropertyChanged(sunfleck::PropertyChangedHandler handler)
    {
        return changed_.subscribe(std::move(handler));
    }

    CounterModel(const CounterModel&) = delete;
    CounterModel& operator=(const CounterModel&) = delete;

private:
    sunfleck::App& app_;
    int syncCount_ = 0, asyncCount_ = 0;
    sunfleck::PropertyChangedEvent changed_;
};
