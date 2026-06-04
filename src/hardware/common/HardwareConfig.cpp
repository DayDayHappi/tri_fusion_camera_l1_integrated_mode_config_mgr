#include "hardware/common/HardwareConfig.h"
#include "foundation/error/ErrorCode.h"
#include <linux/videodev2.h>
#include <algorithm>

namespace tri::hardware {
namespace {
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}
}

foundation::Result<v4l2::CaptureFormat> makeCaptureFormat(const foundation::CameraEndpointConfig& cfg) {
    if (cfg.width <= 0 || cfg.height <= 0 || cfg.fps <= 0) {
        return foundation::Result<v4l2::CaptureFormat>::error(foundation::ErrorCode::InvalidArgument,
            "invalid camera endpoint width/height/fps");
    }
    auto fmt = lower(cfg.format);
    std::uint32_t pix = 0;
    if (fmt == "mjpeg" || fmt == "mjpg") pix = V4L2_PIX_FMT_MJPEG;
    else if (fmt == "yuyv" || fmt == "yuyv422") pix = V4L2_PIX_FMT_YUYV;
    else if (fmt == "nv12") pix = V4L2_PIX_FMT_NV12;
    else {
        return foundation::Result<v4l2::CaptureFormat>::error(foundation::ErrorCode::Unsupported,
            "unsupported pixel format from config: " + cfg.format);
    }
    return foundation::Result<v4l2::CaptureFormat>::ok(v4l2::CaptureFormat{
        static_cast<std::uint32_t>(cfg.width), static_cast<std::uint32_t>(cfg.height), pix, static_cast<std::uint32_t>(cfg.fps)});
}

foundation::Result<serial::SerialPortConfig> makeSerialPortConfig(const foundation::SerialConfig& cfg) {
    if (!cfg.enable) {
        return foundation::Result<serial::SerialPortConfig>::error(foundation::ErrorCode::InvalidArgument,
            "serial config is disabled");
    }
    if (cfg.dev.empty()) {
        return foundation::Result<serial::SerialPortConfig>::error(foundation::ErrorCode::InvalidArgument,
            "serial dev is empty");
    }
    serial::SerialPortConfig out;
    out.device = cfg.dev;
    out.baudrate = cfg.baudrate;
    out.dataBits = cfg.databits;
    out.stopBits = cfg.stopbits;
    out.timeoutMs = cfg.timeoutMs;
    const auto p = lower(cfg.parity);
    if (p == "none" || p == "n") out.parity = serial::SerialParity::None;
    else if (p == "odd" || p == "o") out.parity = serial::SerialParity::Odd;
    else if (p == "even" || p == "e") out.parity = serial::SerialParity::Even;
    else return foundation::Result<serial::SerialPortConfig>::error(foundation::ErrorCode::Unsupported, "unsupported parity: " + cfg.parity);
    return foundation::Result<serial::SerialPortConfig>::ok(out);
}

} // namespace tri::hardware
