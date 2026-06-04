#pragma once

#include "device/CameraManager.h"
#include "foundation/error/Result.h"
#include "media/frame/VideoFrame.h"

namespace tri::media {

class CaptureNode {
public:
    virtual ~CaptureNode() = default;
    virtual foundation::Result<void> init(device::CameraManager* manager) = 0;
    virtual foundation::Result<void> start() = 0;
    virtual foundation::Result<VideoFrame> capture(int timeoutMs) = 0;
    virtual foundation::Result<void> stop() = 0;
    virtual device::CameraId cameraId() const noexcept = 0;
};

} // namespace tri::media
