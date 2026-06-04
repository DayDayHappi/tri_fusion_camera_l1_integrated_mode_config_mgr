#pragma once
#include <cstdint>
#include <vector>

namespace tri::hardware::mpp {

enum class MppCodec { Mjpeg, H264, H265 };
enum class MppPixelFormat { Unknown, Nv12, Rgb888 };

struct MppFrame {
    std::uint32_t width{0};
    std::uint32_t height{0};
    MppPixelFormat format{MppPixelFormat::Unknown};
    std::vector<std::uint8_t> data;
};

struct MppPacket {
    MppCodec codec{MppCodec::H264};
    std::vector<std::uint8_t> data;
    std::int64_t ptsMs{0};
    bool keyFrame{false};
};

} // namespace tri::hardware::mpp
