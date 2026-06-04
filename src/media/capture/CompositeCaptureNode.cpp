#include "media/capture/CompositeCaptureNode.h"

namespace tri::media {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> CompositeCaptureNode::init(device::CameraManager* manager) {
    if (manager == nullptr) return Result<void>::error(ErrorCode::InvalidArgument, "camera manager is null");
    manager_ = manager;
    return Result<void>::success();
}

Result<void> CompositeCaptureNode::start() {
    if (manager_ == nullptr) return Result<void>::error(ErrorCode::NotInitialized, "composite capture node not initialized");
    return manager_->startCamera(device::CameraId::CompositeLowThermal);
}

Result<VideoFrame> CompositeCaptureNode::capture(int timeoutMs) {
    if (manager_ == nullptr) return Result<VideoFrame>::error(ErrorCode::NotInitialized, "composite capture node not initialized");
    auto* camera = manager_->compositeCamera();
    if (camera == nullptr) return Result<VideoFrame>::error(ErrorCode::CompositeCameraOffline, "composite camera is not available");
    auto frame = camera->readFrame(timeoutMs);
    if (!frame) return Result<VideoFrame>::error(frame.status().code(), frame.status().describe());
    return Result<VideoFrame>::ok(fromCameraFrame(frame.value()));
}

Result<void> CompositeCaptureNode::stop() {
    if (manager_ == nullptr) return Result<void>::success();
    return manager_->stopCamera(device::CameraId::CompositeLowThermal);
}

} // namespace tri::media
