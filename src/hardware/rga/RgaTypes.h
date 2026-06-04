#pragma once

#include <cstdint>
#include <cstddef>

namespace tri::hardware::rga {

enum class RgaPixelFormat {
    Unknown,
    Yuyv422,
    Nv12,
    Rgb888,
    Bgr888,
    Rgba8888,
};

struct RgaImageDesc {
    void* virAddr{nullptr};
    int fd{-1};

    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t horStride{0};
    std::uint32_t verStride{0};

    RgaPixelFormat format{RgaPixelFormat::Unknown};
    std::size_t sizeBytes{0};
};

int toRgaFormat(RgaPixelFormat format);
std::size_t imageSizeBytes(RgaPixelFormat format, std::uint32_t width, std::uint32_t height);

} // namespace tri::hardware::rga
