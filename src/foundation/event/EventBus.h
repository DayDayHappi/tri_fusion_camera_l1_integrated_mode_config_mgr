#pragma once

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "foundation/error/Result.h"
#include "foundation/event/EventSubscriber.h"
#include "foundation/thread/ThreadPool.h"

namespace tri::foundation {

class EventBus final {
public:
    EventBus() = default;
    ~EventBus();

    Result<void> init(bool async = false, std::size_t workers = 1);
    void shutdown();

    SubscriptionId subscribe(EventType type, EventCallback callback);
    void unsubscribe(SubscriptionId id);
    void publish(const Event& event);

private:
    struct Subscriber {
        SubscriptionId id{0};
        EventCallback callback;
    };

    std::atomic<SubscriptionId> nextId_{1};
    std::mutex mutex_;
    bool async_{false};
    bool initialized_{false};
    ThreadPool pool_;
    std::unordered_map<EventType, std::vector<Subscriber>> subscribers_;
};

} // namespace tri::foundation
