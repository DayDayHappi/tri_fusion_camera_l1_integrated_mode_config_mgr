#include "media/decode/MppMjpegDecoder.h"

namespace tri::media {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> MppMjpegDecoder::init() {
    if (decoder_ == nullptr) return Result<void>::error(ErrorCode::Unsupported, "MppMjpegDecoder requires a concrete hardware::mpp::MppDecoder");
    return decoder_->init(hardware::mpp::MppCodec::Mjpeg);
}

Result<VideoFrame> MppMjpegDecoder::decode(const VideoFrame& encoded) {
    if (decoder_ == nullptr) return Result<VideoFrame>::error(ErrorCode::Unsupported, "MppMjpegDecoder is not bound");
    hardware::mpp::MppPacket packet;
    packet.codec = hardware::mpp::MppCodec::Mjpeg;
    packet.data = encoded.data;
    packet.ptsMs = encoded.ptsMs;
    auto ret = decoder_->decode(packet);
    if (!ret) return Result<VideoFrame>::error(ret.status().code(), ret.status().describe());
    auto out = fromMppFrame(ret.value(), encoded.source, encoded.ptsMs);
    out.sequence = encoded.sequence;
    out.metadata = encoded.metadata;
    return Result<VideoFrame>::ok(std::move(out));
}

void MppMjpegDecoder::release() {
    if (decoder_ != nullptr) decoder_->release();
}

} // namespace tri::media
