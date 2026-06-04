#include "foundation/thread/ThreadPool.h"
#include "foundation/thread/Thread.h"

namespace tri::foundation {

ThreadPool::~ThreadPool() { stop(); }

Result<void> ThreadPool::start(std::size_t workerCount, std::string namePrefix) {
    if (running_) return Result<void>::error(ErrorCode::Busy, "thread pool already running");
    if (workerCount == 0) return Result<void>::error(ErrorCode::InvalidArgument, "worker count is zero");
    running_ = true;
    for (std::size_t i = 0; i < workerCount; ++i) {
        workers_.emplace_back([this, name = namePrefix + std::to_string(i)] {
            setCurrentThreadName(name);
            while (running_) {
                auto task = tasks_.pop();
                if (!task) break;
                (*task)();
            }
        });
    }
    return Result<void>::success();
}

void ThreadPool::stop() {
    if (!running_) return;
    running_ = false;
    tasks_.close();
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
    workers_.clear();
}

bool ThreadPool::running() const noexcept { return running_; }

} // namespace tri::foundation
