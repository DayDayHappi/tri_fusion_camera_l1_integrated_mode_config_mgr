#pragma once

#include "device/CameraCapability.h"
#include "device/CameraStatus.h"
#include "device/CameraTypes.h"
#include "foundation/config/ConfigTypes.h"
#include "foundation/error/Result.h"

namespace tri::device {

class CameraDevice {
public:
    virtual ~CameraDevice() = default;

    CameraDevice(const CameraDevice&) = delete;
    CameraDevice& operator=(const CameraDevice&) = delete;

    virtual CameraId id() const noexcept = 0;
    virtual CameraKind kind() const noexcept = 0;
    virtual const std::string& name() const noexcept = 0;

    virtual foundation::Result<void> init(const foundation::CameraEndpointConfig& config) = 0;
    virtual foundation::Result<void> open() = 0;
    virtual foundation::Result<void> start() = 0;
    virtual foundation::Result<CameraFrame> readFrame(int timeoutMs) = 0;
    virtual foundation::Result<void> stop() = 0;
    virtual void close() = 0;

    virtual const CameraCapability& capability() const noexcept = 0;
    virtual const CameraStatus& status() const noexcept = 0;
    virtual bool isStreaming() const noexcept = 0;

protected:
    CameraDevice() = default;
};

} // namespace tri::device
