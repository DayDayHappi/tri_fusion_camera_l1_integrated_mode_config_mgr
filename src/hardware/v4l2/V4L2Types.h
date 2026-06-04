#pragma once

#include <cstdint>
#include <string>
#include <sys/time.h>
#include <linux/videodev2.h>

namespace tri::hardware::v4l2 {

struct CaptureFormat {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t pixelformat{V4L2_PIX_FMT_MJPEG};
    std::uint32_t fps{30};
};

struct V4L2CapabilityInfo {
    std::string driver;
    std::string card;
    std::string busInfo;
    std::uint32_t capabilities{0};
    std::uint32_t deviceCaps{0};
};

struct V4L2FormatDesc {
    std::uint32_t pixelformat{0};
    std::string description;
};

struct V4L2BufferView {
    std::uint32_t index{0};
    void* data{nullptr};
    std::uint32_t bytesUsed{0};
    std::uint32_t length{0};
    timeval timestamp{};
};

std::string fourccToString(std::uint32_t fourcc);
bool hasCapability(std::uint32_t caps, std::uint32_t flag);

} // namespace tri::hardware::v4l2
