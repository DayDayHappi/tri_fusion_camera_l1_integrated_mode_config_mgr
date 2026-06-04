#include "media/frame/FrameQueue.h"
#include <chrono>

namespace tri::media {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

FrameQueue::FrameQueue(std::size_t capacity) : capacity_(capacity == 0 ? 1 : capacity) {}

Result<void> FrameQueue::push(VideoFrame frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) return Result<void>::error(ErrorCode::Busy, "frame queue is closed");
    if (queue_.size() >= capacity_) {
        queue_.pop_front();
        ++dropped_;
    }
    queue_.push_back(std::move(frame));
    notEmpty_.notify_one();
    return Result<void>::success();
}

std::optional<VideoFrame> FrameQueue::pop(int timeoutMs) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (timeoutMs < 0) {
        notEmpty_.wait(lock, [&] { return closed_ || !queue_.empty(); });
    } else {
        notEmpty_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] { return closed_ || !queue_.empty(); });
    }
    if (queue_.empty()) return std::nullopt;
    auto frame = std::move(queue_.front());
    queue_.pop_front();
    return frame;
}

void FrameQueue::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
    notEmpty_.notify_all();
}

void FrameQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
}

std::size_t FrameQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

} // namespace tri::media
