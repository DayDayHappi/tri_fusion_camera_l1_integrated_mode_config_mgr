#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <thread>
#include <vector>
#include "foundation/error/Result.h"
#include "foundation/thread/BlockingQueue.h"

namespace tri::foundation {

class ThreadPool final {
public:
    ThreadPool() = default;
    ~ThreadPool();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    Result<void> start(std::size_t workerCount, std::string namePrefix = "tf-worker");
    void stop();
    bool running() const noexcept;

    template <typename F>
    auto submit(F&& f) -> std::future<decltype(f())> {
        using Ret = decltype(f());
        auto task = std::make_shared<std::packaged_task<Ret()>>(std::forward<F>(f));
        auto fut = task->get_future();
        tasks_.push([task]() { (*task)(); });
        return fut;
    }

private:
    std::atomic<bool> running_{false};
    BlockingQueue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
};

} // namespace tri::foundation
