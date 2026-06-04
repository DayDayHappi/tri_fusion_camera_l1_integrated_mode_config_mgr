#pragma once

#include <deque>
#include <mutex>
#include <optional>
#include "media/frame/SyncedFrameGroup.h"

namespace tri::media {

class FrameSync final {
public:
    explicit FrameSync(std::int64_t maxDeltaMs = 40, std::size_t capacity = 8);
    void pushVisible(VideoFrame frame);
    void pushComposite(VideoFrame frame);
    std::optional<SyncedFrameGroup> trySync();
    void clear();

private:
    void trim(std::deque<VideoFrame>& q);
    std::int64_t maxDeltaMs_{40};
    std::size_t capacity_{8};
    std::mutex mutex_;
    std::deque<VideoFrame> visible_;
    std::deque<VideoFrame> composite_;
};

} // namespace tri::media
