#include "service/DeviceService.h"

#include "foundation/error/ErrorCode.h"

namespace tri::service {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> DeviceService::init(tri::device::CameraManager* cameraManager) {
    if (cameraManager == nullptr) {
        return Result<void>::error(ErrorCode::InvalidArgument, "camera manager is null");
    }
    cameraManager_ = cameraManager;
    return Result<void>::success();
}

Result<std::vector<tri::device::CameraStatus>> DeviceService::queryCameraStatus() const {
    if (cameraManager_ == nullptr) {
        return Result<std::vector<tri::device::CameraStatus>>::error(ErrorCode::NotInitialized, "device service is not initialized");
    }
    return Result<std::vector<tri::device::CameraStatus>>::ok(cameraManager_->queryStatus());
}

Result<void> DeviceService::openAllEnabled() {
    if (cameraManager_ == nullptr) {
        return Result<void>::error(ErrorCode::NotInitialized, "device service is not initialized");
    }
    return cameraManager_->openAllEnabled();
}

Result<void> DeviceService::stopAll() {
    if (cameraManager_ == nullptr) {
        return Result<void>::error(ErrorCode::NotInitialized, "device service is not initialized");
    }
    cameraManager_->stopAll();
    return Result<void>::success();
}

} // namespace tri::service
