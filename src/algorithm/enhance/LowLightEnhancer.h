#pragma once

#include "foundation/error/Result.h"
#include "media/frame/VideoFrame.h"

namespace tri::algorithm::enhance {

class LowLightEnhancer final {
public:
    foundation::Result<void> init() { initialized_ = true; return foundation::Result<void>::success(); }
    foundation::Result<media::VideoFrame> process(const media::VideoFrame& frame) const {
        return foundation::Result<media::VideoFrame>::ok(frame);
    }
    void release() noexcept { initialized_ = false; }
    bool initialized() const noexcept { return initialized_; }
private:
    bool initialized_{false};
};

} // namespace tri::algorithm::enhance
