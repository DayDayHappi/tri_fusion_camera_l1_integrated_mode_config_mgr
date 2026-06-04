#pragma once

#include "foundation/error/Result.h"
#include "media/frame/EncodedFrame.h"
#include "media/stream/MainStream.h"

#include <optional>

namespace tri::service {

class StreamService final {
public:
    tri::foundation::Result<void> init(tri::media::MainStream* mainStream);

    tri::foundation::Result<void> startMainStream();
    tri::foundation::Result<void> stopMainStream();
    tri::foundation::Result<void> clearMainStream();

    tri::foundation::Result<tri::media::StreamState> state() const;
    tri::foundation::Result<tri::media::StreamProfile> profile() const;
    tri::foundation::Result<tri::media::StreamStats> stats() const;
    tri::foundation::Result<tri::media::EncodedFrame> latestFrame() const;
    tri::foundation::Result<tri::media::EncodedFrame> waitFrame(int timeoutMs);

    bool initialized() const noexcept { return mainStream_ != nullptr; }

private:
    tri::media::MainStream* mainStream_{nullptr};
};

} // namespace tri::service
