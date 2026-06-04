#include "hardware/v4l2/V4L2Device.h"
#include "foundation/log/Logger.h"
#include "hardware/v4l2/V4L2Error.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace tri::hardware::v4l2 {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

namespace {
std::string sysErr(const std::string& prefix) { return prefix + ": " + std::strerror(errno); }
}

V4L2Device::~V4L2Device() { closeDevice(); }

Result<void> V4L2Device::openDevice(const std::string& path, bool nonBlock) {
    if (isOpen()) return Result<void>::error(ErrorCode::AlreadyInitialized, "V4L2 device already open: " + path_);
    if (path.empty()) return Result<void>::error(ErrorCode::InvalidArgument, "empty V4L2 device path");

    int flags = O_RDWR | O_CLOEXEC;
    if (nonBlock) flags |= O_NONBLOCK;
    fd_ = ::open(path.c_str(), flags);
    if (fd_ < 0) return Result<void>::error(openFailedCodeForPath(path), sysErr("open " + path));
    path_ = path;
    TRI_LOG_INFO(tri::foundation::LogCategory::V4L2) << "opened " << path;
    return Result<void>::success();
}

void V4L2Device::closeDevice() {
    if (streaming_) (void)stopStream();
    for (auto& b : buffers_) {
        if (b.start && b.length > 0) ::munmap(b.start, b.length);
    }
    buffers_.clear();
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    path_.clear();
}

Result<void> V4L2Device::xioctl(unsigned long request, void* arg, ErrorCode code) const {
    if (fd_ < 0) return Result<void>::error(ErrorCode::NotInitialized, "V4L2 device not open");
    int r = 0;
    do { r = ::ioctl(fd_, request, arg); } while (r == -1 && errno == EINTR);
    if (r == -1) return Result<void>::error(code, sysErr("ioctl failed"));
    return Result<void>::success();
}

Result<V4L2CapabilityInfo> V4L2Device::queryCapability() const {
    v4l2_capability cap{};
    auto ret = xioctl(VIDIOC_QUERYCAP, &cap);
    if (!ret) return Result<V4L2CapabilityInfo>::error(ret.status().code(), ret.status().describe());
    V4L2CapabilityInfo info;
    info.driver = reinterpret_cast<const char*>(cap.driver);
    info.card = reinterpret_cast<const char*>(cap.card);
    info.busInfo = reinterpret_cast<const char*>(cap.bus_info);
    info.capabilities = cap.capabilities;
    info.deviceCaps = cap.device_caps;
    return Result<V4L2CapabilityInfo>::ok(info);
}

Result<std::vector<V4L2FormatDesc>> V4L2Device::enumFormats() const {
    if (fd_ < 0) return Result<std::vector<V4L2FormatDesc>>::error(ErrorCode::NotInitialized, "V4L2 device not open");
    std::vector<V4L2FormatDesc> out;
    v4l2_fmtdesc desc{};
    desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    for (desc.index = 0;; ++desc.index) {
        if (::ioctl(fd_, VIDIOC_ENUM_FMT, &desc) == -1) {
            if (errno == EINVAL) break;
            return Result<std::vector<V4L2FormatDesc>>::error(ErrorCode::IoError, sysErr("VIDIOC_ENUM_FMT"));
        }
        out.push_back({desc.pixelformat, reinterpret_cast<const char*>(desc.description)});
    }
    return Result<std::vector<V4L2FormatDesc>>::ok(out);
}

Result<void> V4L2Device::setFormat(const CaptureFormat& fmt) {
    if (fmt.width == 0 || fmt.height == 0 || fmt.fps == 0) {
        return Result<void>::error(ErrorCode::InvalidArgument, "invalid V4L2 capture format");
    }
    v4l2_format f{};
    f.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    f.fmt.pix.width = fmt.width;
    f.fmt.pix.height = fmt.height;
    f.fmt.pix.pixelformat = fmt.pixelformat;
    f.fmt.pix.field = V4L2_FIELD_ANY;
    auto ret = xioctl(VIDIOC_S_FMT, &f);
    if (!ret) return ret;
    if (f.fmt.pix.pixelformat != fmt.pixelformat) {
        return Result<void>::error(ErrorCode::Unsupported, "driver changed pixel format to " + fourccToString(f.fmt.pix.pixelformat));
    }
    return setFrameRate(fmt.fps);
}

Result<CaptureFormat> V4L2Device::getFormat() const {
    v4l2_format f{};
    f.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    auto ret = xioctl(VIDIOC_G_FMT, &f);
    if (!ret) return Result<CaptureFormat>::error(ret.status().code(), ret.status().describe());
    return Result<CaptureFormat>::ok(CaptureFormat{f.fmt.pix.width, f.fmt.pix.height, f.fmt.pix.pixelformat, 0});
}

Result<void> V4L2Device::setFrameRate(std::uint32_t fps) {
    if (fps == 0) return Result<void>::error(ErrorCode::InvalidArgument, "fps is zero");
    v4l2_streamparm parm{};
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = fps;
    return xioctl(VIDIOC_S_PARM, &parm);
}

Result<void> V4L2Device::requestBuffers(std::uint32_t count) {
    if (count < 2) return Result<void>::error(ErrorCode::InvalidArgument, "V4L2 buffer count must be >= 2");
    v4l2_requestbuffers req{};
    req.count = count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    auto ret = xioctl(VIDIOC_REQBUFS, &req);
    if (!ret) return ret;
    if (req.count < 2) return Result<void>::error(ErrorCode::InternalError, "insufficient V4L2 buffers returned by driver");

    buffers_.resize(req.count);
    for (std::uint32_t i = 0; i < req.count; ++i) {
        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        ret = xioctl(VIDIOC_QUERYBUF, &buf);
        if (!ret) return ret;
        void* start = ::mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, buf.m.offset);
        if (start == MAP_FAILED) return Result<void>::error(ErrorCode::IoError, sysErr("mmap"));
        buffers_[i] = {start, buf.length};
        ret = enqueueBuffer(i);
        if (!ret) return ret;
    }
    TRI_LOG_INFO(tri::foundation::LogCategory::V4L2) << "requested " << buffers_.size() << " mmap buffers for " << path_;
    return Result<void>::success();
}

Result<void> V4L2Device::startStream() {
    if (streaming_) return Result<void>::success();
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    auto ret = xioctl(VIDIOC_STREAMON, &type);
    if (!ret) return ret;
    streaming_ = true;
    return Result<void>::success();
}

Result<V4L2BufferView> V4L2Device::dequeueBuffer(int timeoutMs) {
    if (!streaming_) return Result<V4L2BufferView>::error(ErrorCode::NotInitialized, "V4L2 stream not started");
    pollfd pfd{fd_, POLLIN, 0};
    int pr = ::poll(&pfd, 1, timeoutMs);
    if (pr == 0) return Result<V4L2BufferView>::error(ErrorCode::Timeout, "V4L2 dequeue timeout");
    if (pr < 0) return Result<V4L2BufferView>::error(ErrorCode::IoError, sysErr("poll"));

    v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (::ioctl(fd_, VIDIOC_DQBUF, &buf) == -1) {
        return Result<V4L2BufferView>::error(ErrorCode::IoError, sysErr("VIDIOC_DQBUF"));
    }
    if (buf.index >= buffers_.size()) {
        return Result<V4L2BufferView>::error(ErrorCode::InternalError, "invalid V4L2 buffer index");
    }
    return Result<V4L2BufferView>::ok(V4L2BufferView{buf.index, buffers_[buf.index].start, buf.bytesused, static_cast<std::uint32_t>(buffers_[buf.index].length), buf.timestamp});
}

Result<void> V4L2Device::enqueueBuffer(std::uint32_t index) {
    if (index >= buffers_.size()) return Result<void>::error(ErrorCode::InvalidArgument, "invalid V4L2 buffer index");
    v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;
    return xioctl(VIDIOC_QBUF, &buf);
}

Result<void> V4L2Device::stopStream() {
    if (!streaming_) return Result<void>::success();
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    auto ret = xioctl(VIDIOC_STREAMOFF, &type);
    streaming_ = false;
    return ret;
}

} // namespace tri::hardware::v4l2
