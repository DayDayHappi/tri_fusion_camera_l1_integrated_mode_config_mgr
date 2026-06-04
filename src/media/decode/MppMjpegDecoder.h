#pragma once
#include "hardware/mpp/MppDecoder.h"
#include "media/decode/Decoder.h"
namespace tri::media {
class MppMjpegDecoder final : public Decoder {
public:
    explicit MppMjpegDecoder(hardware::mpp::MppDecoder* decoder = nullptr) : decoder_(decoder) {}
    void setDecoder(hardware::mpp::MppDecoder* decoder) noexcept { decoder_ = decoder; }
    foundation::Result<void> init() override;
    foundation::Result<VideoFrame> decode(const VideoFrame& encoded) override;
    void release() override;
private:
    hardware::mpp::MppDecoder* decoder_{nullptr};
};
} // namespace tri::media
