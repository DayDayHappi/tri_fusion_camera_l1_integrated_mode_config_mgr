#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include "foundation/error/Result.h"

namespace tri::foundation {

class Worker final {
public:
    using Task = std::function<void(std::atomic<bool>& stopFlag)>;
    Worker() = default;
    ~Worker();
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    Result<void> start(std::string name, Task task);
    void stop();
    bool running() const noexcept;
private:
    std::string name_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopFlag_{false};
    std::thread thread_;
};

} // namespace tri::foundation
