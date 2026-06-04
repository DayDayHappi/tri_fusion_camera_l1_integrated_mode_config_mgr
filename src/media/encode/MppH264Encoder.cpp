#include "media/encode/MppH264Encoder.h"

#include <cstddef>
#include <string>
#include <utility>

namespace tri::media {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> MppH264Encoder::init(const StreamProfile& profile) {
    if (encoder_ == nullptr) {
        return Result<void>::error(ErrorCode::Unsupported,
                                   "MppH264Encoder requires hardware::mpp::MppEncoder");
    }

    hardware::mpp::MppEncoderConfig cfg;
    cfg.codec = hardware::mpp::MppCodec::H264;
    cfg.inputFormat = hardware::mpp::MppPixelFormat::Nv12;

    cfg.width = static_cast<std::uint32_t>(profile.width);
    cfg.height = static_cast<std::uint32_t>(profile.height);
    cfg.horStride = static_cast<std::uint32_t>(profile.width);
    cfg.verStride = static_cast<std::uint32_t>(profile.height);

    cfg.fps = static_cast<std::uint32_t>(profile.fps);
    cfg.bitrateKbps = static_cast<std::uint32_t>(profile.bitrateKbps);
    cfg.gop = static_cast<std::uint32_t>(profile.gop);
    cfg.lowLatency = true;

    return encoder_->init(cfg);
}

Result<EncodedFrame> MppH264Encoder::encode(const VideoFrame& frame) {
    if (encoder_ == nullptr) {
        return Result<EncodedFrame>::error(ErrorCode::Unsupported,
                                           "MppH264Encoder is not bound to hardware::mpp::MppEncoder");
    }

    if (frame.format != VideoPixelFormat::Nv12) {
        return Result<EncodedFrame>::error(
            ErrorCode::Unsupported,
            std::string("MppH264Encoder requires NV12 input, got ") + toString(frame.format)
        );
    }

    if (frame.width == 0 || frame.height == 0 || frame.data.empty()) {
        return Result<EncodedFrame>::error(ErrorCode::InvalidArgument,
                                           "MppH264Encoder input frame is invalid");
    }

    const std::size_t expected =
        static_cast<std::size_t>(frame.width) *
        static_cast<std::size_t>(frame.height) * 3 / 2;

    if (frame.data.size() < expected) {
        return Result<EncodedFrame>::error(ErrorCode::InvalidArgument,
                                           "MppH264Encoder NV12 frame data is too small");
    }

    auto packet = encoder_->encodeNv12(frame.data.data(),
                                       frame.data.size(),
                                       static_cast<std::uint64_t>(frame.ptsMs));

    if (!packet) {
        return Result<EncodedFrame>::error(packet.status().code(),
                                           packet.status().describe());
    }

    EncodedFrame out;
    out.codec = hardware::mpp::MppCodec::H264;
    out.data = std::move(packet.value().data);
    out.ptsMs = frame.ptsMs;
    out.keyFrame = packet.value().keyFrame;
    out.sequence = frame.sequence != 0 ? frame.sequence : ++sequence_;

    return Result<EncodedFrame>::ok(std::move(out));
}

void MppH264Encoder::release() {
    if (encoder_ != nullptr) {
        encoder_->release();
    }
}

} // namespace tri::media
