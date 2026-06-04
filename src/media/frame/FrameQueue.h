#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include "foundation/error/Result.h"
#include "media/frame/VideoFrame.h"

namespace tri::media {

class FrameQueue final {
public:
    explicit FrameQueue(std::size_t capacity = 8);
    foundation::Result<void> push(VideoFrame frame);
    std::optional<VideoFrame> pop(int timeoutMs);
    void close();
    void clear();
    std::size_t size() const;
    std::uint64_t dropped() const noexcept { return dropped_; }

private:
    std::size_t capacity_{8};
    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::deque<VideoFrame> queue_;
    bool closed_{false};
    std::uint64_t dropped_{0};
};

} // namespace tri::media
