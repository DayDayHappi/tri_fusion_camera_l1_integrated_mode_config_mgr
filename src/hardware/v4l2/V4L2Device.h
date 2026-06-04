#pragma once

#include "foundation/error/Result.h"
#include "hardware/v4l2/V4L2Buffer.h"
#include "hardware/v4l2/V4L2Format.h"
#include "hardware/v4l2/V4L2Types.h"

#include <string>
#include <vector>

namespace tri::hardware::v4l2 {

class V4L2Device final {
public:
    V4L2Device() = default;
    ~V4L2Device();

    V4L2Device(const V4L2Device&) = delete;
    V4L2Device& operator=(const V4L2Device&) = delete;

    foundation::Result<void> openDevice(const std::string& path, bool nonBlock = false);
    void closeDevice();

    bool isOpen() const noexcept { return fd_ >= 0; }
    int fd() const noexcept { return fd_; }
    const std::string& path() const noexcept { return path_; }
    bool streaming() const noexcept { return streaming_; }

    foundation::Result<V4L2CapabilityInfo> queryCapability() const;
    foundation::Result<std::vector<V4L2FormatDesc>> enumFormats() const;
    foundation::Result<void> setFormat(const CaptureFormat& fmt);
    foundation::Result<CaptureFormat> getFormat() const;
    foundation::Result<void> setFrameRate(std::uint32_t fps);

    foundation::Result<void> requestBuffers(std::uint32_t count);
    foundation::Result<void> startStream();
    foundation::Result<V4L2BufferView> dequeueBuffer(int timeoutMs);
    foundation::Result<void> enqueueBuffer(std::uint32_t index);
    foundation::Result<void> stopStream();

private:
    foundation::Result<void> xioctl(unsigned long request, void* arg, foundation::ErrorCode code = foundation::ErrorCode::IoError) const;

    int fd_{-1};
    std::string path_;
    std::vector<MappedBuffer> buffers_;
    bool streaming_{false};
};

} // namespace tri::hardware::v4l2
