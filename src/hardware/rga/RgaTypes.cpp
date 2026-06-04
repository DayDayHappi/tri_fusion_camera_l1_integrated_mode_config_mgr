#include "hardware/rga/RgaTypes.h"

#include <im2d.h>

namespace tri::hardware::rga {

int toRgaFormat(RgaPixelFormat format) {
    switch (format) {
        case RgaPixelFormat::Yuyv422:
            return RK_FORMAT_YUYV_422;
        case RgaPixelFormat::Nv12:
            return RK_FORMAT_YCbCr_420_SP;
        case RgaPixelFormat::Rgb888:
            return RK_FORMAT_RGB_888;
        case RgaPixelFormat::Bgr888:
            return RK_FORMAT_BGR_888;
        case RgaPixelFormat::Rgba8888:
            return RK_FORMAT_RGBA_8888;
        default:
            return 0;
    }
}

std::size_t imageSizeBytes(RgaPixelFormat format, std::uint32_t width, std::uint32_t height) {
    switch (format) {
        case RgaPixelFormat::Yuyv422:
            return static_cast<std::size_t>(width) * height * 2;
        case RgaPixelFormat::Nv12:
            return static_cast<std::size_t>(width) * height * 3 / 2;
        case RgaPixelFormat::Rgb888:
        case RgaPixelFormat::Bgr888:
            return static_cast<std::size_t>(width) * height * 3;
        case RgaPixelFormat::Rgba8888:
            return static_cast<std::size_t>(width) * height * 4;
        default:
            return 0;
    }
}

} // namespace tri::hardware::rga
