#include "foundation/event/EventBus.h"
#include <algorithm>

namespace tri::foundation {

EventBus::~EventBus() { shutdown(); }

Result<void> EventBus::init(bool async, std::size_t workers) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return Result<void>::error(ErrorCode::Busy, "event bus already initialized");
    async_ = async;
    if (async_) {
        auto ret = pool_.start(workers, "tf-event-");
        if (!ret) return ret;
    }
    initialized_ = true;
    return Result<void>::success();
}

void EventBus::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_.clear();
        initialized_ = false;
    }
    pool_.stop();
}

SubscriptionId EventBus::subscribe(EventType type, EventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto id = nextId_++;
    subscribers_[type].push_back(Subscriber{id, std::move(callback)});
    return id;
}

void EventBus::unsubscribe(SubscriptionId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [_, vec] : subscribers_) {
        vec.erase(std::remove_if(vec.begin(), vec.end(), [&](const Subscriber& s) { return s.id == id; }), vec.end());
    }
}

void EventBus::publish(const Event& event) {
    std::vector<EventCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscribers_.find(event.type);
        if (it != subscribers_.end()) {
            for (const auto& sub : it->second) callbacks.push_back(sub.callback);
        }
    }
    for (auto& cb : callbacks) {
        if (async_) {
            pool_.submit([cb, event] { cb(event); });
        } else {
            cb(event);
        }
    }
}

} // namespace tri::foundation
