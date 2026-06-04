#pragma once

#include "device/CameraManager.h"
#include "foundation/error/Result.h"

#include <vector>

namespace tri::service {

class DeviceService final {
public:
    tri::foundation::Result<void> init(tri::device::CameraManager* cameraManager);
    tri::foundation::Result<std::vector<tri::device::CameraStatus>> queryCameraStatus() const;
    tri::foundation::Result<void> openAllEnabled();
    tri::foundation::Result<void> stopAll();

    bool initialized() const noexcept { return cameraManager_ != nullptr; }

private:
    tri::device::CameraManager* cameraManager_{nullptr};
};

} // namespace tri::service
