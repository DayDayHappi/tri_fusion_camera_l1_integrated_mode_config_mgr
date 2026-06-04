#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include "foundation/config/ConfigTypes.h"
#include "foundation/error/Result.h"
#include "media/frame/EncodedFrame.h"
#include "media/stream/StreamTypes.h"

namespace tri::media {

class MainStream final {
public:
    foundation::Result<void> init(const foundation::MediaConfig& config);
    void start();
    void stop();
    void clear();
    foundation::Result<void> push(EncodedFrame frame);
    std::optional<EncodedFrame> waitFrame(int timeoutMs);
    std::optional<EncodedFrame> latestFrame() const;
    StreamState state() const noexcept { return state_; }
    const StreamProfile& profile() const noexcept { return profile_; }
    const StreamStats& stats() const noexcept { return stats_; }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<EncodedFrame> frames_;
    std::size_t capacity_{30};
    StreamProfile profile_{};
    StreamStats stats_{};
    StreamState state_{StreamState::Stopped};
};

} // namespace tri::media
