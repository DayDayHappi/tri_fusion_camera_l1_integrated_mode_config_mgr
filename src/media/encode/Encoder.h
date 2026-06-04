#pragma once
#include "foundation/error/Result.h"
#include "media/frame/EncodedFrame.h"
#include "media/frame/VideoFrame.h"
#include "media/stream/StreamTypes.h"
namespace tri::media {
class Encoder {
public:
    virtual ~Encoder() = default;
    virtual foundation::Result<void> init(const StreamProfile& profile) = 0;
    virtual foundation::Result<EncodedFrame> encode(const VideoFrame& frame) = 0;
    virtual void release() = 0;
};
} // namespace tri::media
