#pragma once
#include "foundation/error/Result.h"
#include "media/frame/VideoFrame.h"
namespace tri::media {
class Decoder {
public:
    virtual ~Decoder() = default;
    virtual foundation::Result<void> init() = 0;
    virtual foundation::Result<VideoFrame> decode(const VideoFrame& encoded) = 0;
    virtual void release() = 0;
};
} // namespace tri::media
