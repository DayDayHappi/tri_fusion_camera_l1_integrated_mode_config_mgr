#pragma once

#include "device/CameraDevice.h"
#include "hardware/uvc/UvcMetadataReader.h"
#include "hardware/v4l2/V4L2Device.h"

namespace tri::device {

class CompositeLowThermalCameraDevice final : public CameraDevice {
public:
    CompositeLowThermalCameraDevice() = default;
    ~CompositeLowThermalCameraDevice() override;

    CameraId id() const noexcept override { return CameraId::CompositeLowThermal; }
    CameraKind kind() const noexcept override { return CameraKind::CompositeLowThermalUvc; }
    const std::string& name() const noexcept override { return config_.name; }

    foundation::Result<void> init(const foundation::CameraEndpointConfig& config) override;
    foundation::Result<void> open() override;
    foundation::Result<void> start() override;
    foundation::Result<CameraFrame> readFrame(int timeoutMs) override;
    foundation::Result<void> stop() override;
    void close() override;

    const CameraCapability& capability() const noexcept override { return capability_; }
    const CameraStatus& status() const noexcept override { return status_; }
    bool isStreaming() const noexcept override { return v4l2_.streaming(); }

private:
    foundation::Result<void> rememberError(foundation::ErrorCode code, const std::string& message);
    void clearError();
    void tryOpenMetadataNode();
    void tryAttachMetadata(CameraFrame& frame);

    foundation::CameraEndpointConfig config_{};
    hardware::v4l2::CaptureFormat captureFormat_{};
    hardware::v4l2::V4L2Device v4l2_;
    hardware::uvc::UvcMetadataReader metadataReader_;
    CameraCapability capability_{};
    CameraStatus status_{};
    std::uint64_t sequence_{0};
};

} // namespace tri::device
