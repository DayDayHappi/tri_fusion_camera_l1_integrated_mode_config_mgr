#pragma once
#include <memory>
#include "hardware/mpp/MppDecoder.h"
#include "media/decode/MjpegDecoder.h"
#include "media/decode/MppMjpegDecoder.h"
namespace tri::media {
class DecoderFactory final {
public:
    static std::unique_ptr<Decoder> createPassthroughMjpeg() { return std::make_unique<MjpegDecoder>(); }
    static std::unique_ptr<Decoder> createMppMjpeg(hardware::mpp::MppDecoder* decoder) { return std::make_unique<MppMjpegDecoder>(decoder); }
};
} // namespace tri::media
