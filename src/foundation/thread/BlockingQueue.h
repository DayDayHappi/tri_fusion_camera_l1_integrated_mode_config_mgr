#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

namespace tri::foundation {

template <typename T>
class BlockingQueue final {
public:
    explicit BlockingQueue(std::size_t capacity = 0) : capacity_(capacity) {}

    void push(T value) {
        std::unique_lock<std::mutex> lock(mutex_);
        notFull_.wait(lock, [&] { return closed_ || capacity_ == 0 || queue_.size() < capacity_; });
        if (closed_) return;
        queue_.push_back(std::move(value));
        notEmpty_.notify_one();
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        notEmpty_.wait(lock, [&] { return closed_ || !queue_.empty(); });
        if (queue_.empty()) return std::nullopt;
        T value = std::move(queue_.front());
        queue_.pop_front();
        notFull_.notify_one();
        return value;
    }

    bool tryPop(T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        value = std::move(queue_.front());
        queue_.pop_front();
        notFull_.notify_one();
        return true;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        notEmpty_.notify_all();
        notFull_.notify_all();
    }

    bool closed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    std::size_t capacity_{0};
    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
    std::deque<T> queue_;
    bool closed_{false};
};

} // namespace tri::foundation
