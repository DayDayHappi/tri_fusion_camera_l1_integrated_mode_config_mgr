#include "foundation/time/Timer.h"

namespace tri::foundation {

ScopedTimer::ScopedTimer(Callback callback)
    : callback_(std::move(callback)), start_(std::chrono::steady_clock::now()) {}

ScopedTimer::~ScopedTimer() {
    if (callback_) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_);
        callback_(elapsed);
    }
}

PeriodicTimer::~PeriodicTimer() { stop(); }

Result<void> PeriodicTimer::start(std::chrono::milliseconds interval, std::function<void()> callback) {
    if (running_) return Result<void>::error(ErrorCode::Busy, "timer already running");
    if (!callback) return Result<void>::error(ErrorCode::InvalidArgument, "timer callback is empty");
    running_ = true;
    thread_ = std::thread([this, interval, callback = std::move(callback)]() mutable {
        while (running_) {
            auto begin = std::chrono::steady_clock::now();
            callback();
            auto cost = std::chrono::steady_clock::now() - begin;
            if (cost < interval) std::this_thread::sleep_for(interval - cost);
        }
    });
    return Result<void>::success();
}

void PeriodicTimer::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

bool PeriodicTimer::running() const noexcept { return running_; }

} // namespace tri::foundation
