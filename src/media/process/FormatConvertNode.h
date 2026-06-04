#pragma once

#include "foundation/error/Result.h"
#include "media/frame/VideoFrame.h"

namespace tri::media {

class FormatConvertNode final {
public:
    explicit FormatConvertNode(VideoPixelFormat target = VideoPixelFormat::Nv12) : target_(target) {}

    void setTarget(VideoPixelFormat target) noexcept { target_ = target; }
    VideoPixelFormat target() const noexcept { return target_; }

    foundation::Result<VideoFrame> process(const VideoFrame& frame) const;

private:
    foundation::Result<VideoFrame> convertYuyv422ToNv12(const VideoFrame& frame) const;

    VideoPixelFormat target_{VideoPixelFormat::Nv12};
};

} // namespace tri::media
