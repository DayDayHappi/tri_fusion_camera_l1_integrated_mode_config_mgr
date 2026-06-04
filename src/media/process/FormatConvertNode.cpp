#include "media/process/FormatConvertNode.h"
#include "hardware/rga/RgaTypes.h"
#include "hardware/rga/RgaConverter.h"
#include <string>
#include <utility>

namespace tri::media {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<VideoFrame> FormatConvertNode::process(const VideoFrame& frame) const {
    if (frame.format == target_) return Result<VideoFrame>::ok(frame);

    if (target_ == VideoPixelFormat::Nv12 && frame.format == VideoPixelFormat::Yuyv422) {
        return convertYuyv422ToNv12(frame);
    }

    if (isEncodedFormat(frame.format)) {
        return Result<VideoFrame>::error(ErrorCode::Unsupported,
            std::string("cannot format-convert encoded input ") + toString(frame.format) +
            "; decode it before FormatConvertNode");
    }

    return Result<VideoFrame>::error(ErrorCode::Unsupported,
        std::string("unsupported format conversion: ") + toString(frame.format) +
        " -> " + toString(target_));
}

Result<VideoFrame> FormatConvertNode::convertYuyv422ToNv12(const VideoFrame& frame) const {
    if (frame.width == 0 || frame.height == 0) {
        return Result<VideoFrame>::error(ErrorCode::InvalidArgument, "invalid YUYV frame size");
    }

    const std::size_t srcExpected =
        static_cast<std::size_t>(frame.width) * frame.height * 2;

    if (frame.data.size() < srcExpected) {
        return Result<VideoFrame>::error(ErrorCode::InvalidArgument, "YUYV frame data too small");
    }

    VideoFrame out;
    out.width = frame.width;
    out.height = frame.height;
    out.ptsMs = frame.ptsMs;
    out.sequence = frame.sequence;
    out.format = VideoPixelFormat::Nv12;
    out.data.resize(static_cast<std::size_t>(frame.width) * frame.height * 3 / 2);

    tri::hardware::rga::RgaImageDesc src{};
    src.virAddr = const_cast<std::uint8_t*>(frame.data.data());
    src.width = frame.width;
    src.height = frame.height;
    src.horStride = frame.width;
    src.verStride = frame.height;
    src.format = tri::hardware::rga::RgaPixelFormat::Yuyv422;
    src.sizeBytes = frame.data.size();

    tri::hardware::rga::RgaImageDesc dst{};
    dst.virAddr = out.data.data();
    dst.width = out.width;
    dst.height = out.height;
    dst.horStride = out.width;
    dst.verStride = out.height;
    dst.format = tri::hardware::rga::RgaPixelFormat::Nv12;
    dst.sizeBytes = out.data.size();

    tri::hardware::rga::RgaConverter converter;
    auto ret = converter.convert(src, dst);
    if (!ret) {
        return Result<VideoFrame>::error(ret.status().code(), ret.status().message());
    }

    return Result<VideoFrame>::ok(std::move(out));
}

} // namespace tri::media
