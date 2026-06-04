#include "media/frame/FrameSync.h"
#include <cstdlib>

namespace tri::media {

FrameSync::FrameSync(std::int64_t maxDeltaMs, std::size_t capacity)
    : maxDeltaMs_(maxDeltaMs), capacity_(capacity == 0 ? 1 : capacity) {}

void FrameSync::trim(std::deque<VideoFrame>& q) {
    while (q.size() > capacity_) q.pop_front();
}

void FrameSync::pushVisible(VideoFrame frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    visible_.push_back(std::move(frame));
    trim(visible_);
}

void FrameSync::pushComposite(VideoFrame frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    composite_.push_back(std::move(frame));
    trim(composite_);
}

std::optional<SyncedFrameGroup> FrameSync::trySync() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (visible_.empty() || composite_.empty()) return std::nullopt;

    std::size_t bestIndex = 0;
    auto bestDelta = std::llabs(visible_.front().ptsMs - composite_.front().ptsMs);
    for (std::size_t i = 1; i < composite_.size(); ++i) {
        auto delta = std::llabs(visible_.front().ptsMs - composite_[i].ptsMs);
        if (delta < bestDelta) {
            bestDelta = delta;
            bestIndex = i;
        }
    }
    if (bestDelta > maxDeltaMs_) {
        if (visible_.front().ptsMs < composite_.front().ptsMs) visible_.pop_front();
        else composite_.pop_front();
        return std::nullopt;
    }

    SyncedFrameGroup out;
    out.visible = std::move(visible_.front());
    out.composite = std::move(composite_[bestIndex]);
    out.deltaMs = bestDelta;
    visible_.pop_front();
    composite_.erase(composite_.begin(), composite_.begin() + static_cast<long>(bestIndex) + 1);
    return out;
}

void FrameSync::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    visible_.clear();
    composite_.clear();
}

} // namespace tri::media
