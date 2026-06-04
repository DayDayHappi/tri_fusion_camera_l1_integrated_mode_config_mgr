#pragma once
#include "media/capture/CaptureNode.h"
namespace tri::media {
class CompositeCaptureNode final : public CaptureNode {
public:
    foundation::Result<void> init(device::CameraManager* manager) override;
    foundation::Result<void> start() override;
    foundation::Result<VideoFrame> capture(int timeoutMs) override;
    foundation::Result<void> stop() override;
    device::CameraId cameraId() const noexcept override { return device::CameraId::CompositeLowThermal; }
private:
    device::CameraManager* manager_{nullptr};
};
} // namespace tri::media
