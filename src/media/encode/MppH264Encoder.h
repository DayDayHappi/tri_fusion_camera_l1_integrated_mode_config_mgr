#pragma once

#include "hardware/mpp/MppEncoder.h"
#include "media/encode/Encoder.h"

namespace tri::media {

class MppH264Encoder final : public Encoder {
public:
    explicit MppH264Encoder(hardware::mpp::MppEncoder* encoder = nullptr)
        : encoder_(encoder) {}

    void setEncoder(hardware::mpp::MppEncoder* encoder) noexcept {
        encoder_ = encoder;
    }

    foundation::Result<void> init(const StreamProfile& profile) override;
    foundation::Result<EncodedFrame> encode(const VideoFrame& frame) override;
    void release() override;

private:
    hardware::mpp::MppEncoder* encoder_{nullptr};
    std::uint64_t sequence_{0};
};

} // namespace tri::media
