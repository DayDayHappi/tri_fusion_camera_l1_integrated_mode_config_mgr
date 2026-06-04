#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <linux/videodev2.h>
#include "device/CameraTypes.h"
#include "hardware/mpp/MppTypes.h"

namespace tri::media {

enum class VideoPixelFormat {
    Unknown = 0,
    Mjpeg,
    Nv12,
    Rgb888,
    Yuyv422,
};

inline VideoPixelFormat fromV4L2PixelFormat(std::uint32_t fourcc) noexcept {
    switch (fourcc) {
        case V4L2_PIX_FMT_MJPEG: return VideoPixelFormat::Mjpeg;
        case V4L2_PIX_FMT_YUYV:  return VideoPixelFormat::Yuyv422;
        case V4L2_PIX_FMT_NV12:  return VideoPixelFormat::Nv12;
        default: return VideoPixelFormat::Unknown;
    }
}

inline const char* toString(VideoPixelFormat format) noexcept {
    switch (format) {
        case VideoPixelFormat::Mjpeg: return "MJPG";
        case VideoPixelFormat::Nv12: return "NV12";
        case VideoPixelFormat::Rgb888: return "RGB888";
        case VideoPixelFormat::Yuyv422: return "YUYV422";
        case VideoPixelFormat::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

inline bool isEncodedFormat(VideoPixelFormat format) noexcept {
    return format == VideoPixelFormat::Mjpeg;
}

inline bool isRawFormat(VideoPixelFormat format) noexcept {
    return format == VideoPixelFormat::Nv12 ||
           format == VideoPixelFormat::Rgb888 ||
           format == VideoPixelFormat::Yuyv422;
}

struct VideoFrame {
    device::CameraId source{device::CameraId::Visible};
    std::uint64_t sequence{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    VideoPixelFormat format{VideoPixelFormat::Unknown};
    std::uint32_t v4l2PixelFormat{0};
    std::vector<std::uint8_t> data;
    std::int64_t ptsMs{0};
    std::vector<std::uint8_t> metadata;
};

inline VideoFrame fromCameraFrame(const device::CameraFrame& frame) {
    VideoFrame out;
    out.source = frame.cameraId;
    out.sequence = frame.sequence;
    out.width = frame.width;
    out.height = frame.height;
    out.v4l2PixelFormat = frame.pixelformat;
    out.format = fromV4L2PixelFormat(frame.pixelformat);
    out.data = frame.bytes;
    out.ptsMs = frame.receiveTimestampMs;
    out.metadata = frame.metadata;
    return out;
}

inline hardware::mpp::MppPixelFormat toMppPixelFormat(VideoPixelFormat format) noexcept {
    switch (format) {
        case VideoPixelFormat::Nv12: return hardware::mpp::MppPixelFormat::Nv12;
        case VideoPixelFormat::Rgb888: return hardware::mpp::MppPixelFormat::Rgb888;
        default: return hardware::mpp::MppPixelFormat::Unknown;
    }
}

inline hardware::mpp::MppFrame toMppFrame(const VideoFrame& frame,
                                          hardware::mpp::MppPixelFormat fallback = hardware::mpp::MppPixelFormat::Nv12) {
    hardware::mpp::MppFrame out;
    out.width = frame.width;
    out.height = frame.height;
    const auto mapped = toMppPixelFormat(frame.format);
    out.format = mapped == hardware::mpp::MppPixelFormat::Unknown ? fallback : mapped;
    out.data = frame.data;
    return out;
}

inline VideoFrame fromMppFrame(const hardware::mpp::MppFrame& frame,
                               device::CameraId source,
                               std::int64_t ptsMs = 0) {
    VideoFrame out;
    out.source = source;
    out.width = frame.width;
    out.height = frame.height;
    out.format = frame.format == hardware::mpp::MppPixelFormat::Nv12 ? VideoPixelFormat::Nv12 :
                 frame.format == hardware::mpp::MppPixelFormat::Rgb888 ? VideoPixelFormat::Rgb888 :
                 VideoPixelFormat::Unknown;
    out.data = frame.data;
    out.ptsMs = ptsMs;
    return out;
}

} // namespace tri::media
