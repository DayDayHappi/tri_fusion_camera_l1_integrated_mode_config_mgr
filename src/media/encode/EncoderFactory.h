#pragma once
#include <memory>
#include "hardware/mpp/MppEncoder.h"
#include "media/encode/MppH264Encoder.h"
namespace tri::media {
class EncoderFactory final {
public:
    static std::unique_ptr<Encoder> createMppH264(hardware::mpp::MppEncoder* encoder) { return std::make_unique<MppH264Encoder>(encoder); }
};
} // namespace tri::media
