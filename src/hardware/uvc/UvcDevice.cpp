#include "hardware/uvc/UvcDevice.h"
#include "foundation/log/Logger.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace tri::hardware::uvc {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

namespace {
std::uint32_t effectiveCaps(const v4l2_capability& cap) {
    return (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;
}
}

Result<UvcNodeInfo> UvcDevice::inspectNode(const std::string& path) {
    if (path.empty()) return Result<UvcNodeInfo>::error(ErrorCode::InvalidArgument, "empty UVC node path");
    int fd = ::open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return Result<UvcNodeInfo>::error(ErrorCode::IoError, std::string("open ") + path + ": " + std::strerror(errno));
    v4l2_capability cap{};
    if (::ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        auto msg = std::string("VIDIOC_QUERYCAP ") + path + ": " + std::strerror(errno);
        ::close(fd);
        return Result<UvcNodeInfo>::error(ErrorCode::IoError, msg);
    }
    ::close(fd);

    UvcNodeInfo info;
    info.path = path;
    info.driver = reinterpret_cast<const char*>(cap.driver);
    info.card = reinterpret_cast<const char*>(cap.card);
    info.busInfo = reinterpret_cast<const char*>(cap.bus_info);
    info.capabilities = cap.capabilities;
    info.deviceCaps = cap.device_caps;
    const auto caps = effectiveCaps(cap);
    if (caps & V4L2_CAP_META_CAPTURE) info.kind = UvcNodeKind::MetadataCapture;
    else if (caps & V4L2_CAP_VIDEO_CAPTURE) info.kind = UvcNodeKind::VideoCapture;
    else info.kind = UvcNodeKind::Unknown;

    TRI_LOG_INFO(tri::foundation::LogCategory::Uvc) << "inspected " << path << ", card=" << info.card;
    return Result<UvcNodeInfo>::ok(info);
}

bool UvcDevice::isVideoCaptureNode(const UvcNodeInfo& info) noexcept { return info.kind == UvcNodeKind::VideoCapture; }
bool UvcDevice::isMetadataNode(const UvcNodeInfo& info) noexcept { return info.kind == UvcNodeKind::MetadataCapture; }

} // namespace tri::hardware::uvc
