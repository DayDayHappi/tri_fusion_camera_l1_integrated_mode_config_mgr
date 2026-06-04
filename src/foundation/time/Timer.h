#pragma once

#include <chrono>
#include <functional>
#include <thread>
#include <atomic>
#include "foundation/error/Result.h"

namespace tri::foundation {

class ScopedTimer final {
public:
    using Callback = std::function<void(std::chrono::milliseconds)>;
    explicit ScopedTimer(Callback callback);
    ~ScopedTimer();
private:
    Callback callback_;
    std::chrono::steady_clock::time_point start_;
};

class PeriodicTimer final {
public:
    PeriodicTimer() = default;
    ~PeriodicTimer();
    PeriodicTimer(const PeriodicTimer&) = delete;
    PeriodicTimer& operator=(const PeriodicTimer&) = delete;

    Result<void> start(std::chrono::milliseconds interval, std::function<void()> callback);
    void stop();
    bool running() const noexcept;
private:
    std::atomic<bool> running_{false};
    std::thread thread_;
};

} // namespace tri::foundation
