#pragma once
#include "media/decode/Decoder.h"
namespace tri::media {
class MjpegDecoder : public Decoder {
public:
    foundation::Result<void> init() override { return foundation::Result<void>::success(); }
    foundation::Result<VideoFrame> decode(const VideoFrame& encoded) override;
    void release() override {}
};
} // namespace tri::media
