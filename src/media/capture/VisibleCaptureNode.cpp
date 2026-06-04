#include "media/capture/VisibleCaptureNode.h"

namespace tri::media {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> VisibleCaptureNode::init(device::CameraManager* manager) {
    if (manager == nullptr) return Result<void>::error(ErrorCode::InvalidArgument, "camera manager is null");
    manager_ = manager;
    return Result<void>::success();
}

Result<void> VisibleCaptureNode::start() {
    if (manager_ == nullptr) return Result<void>::error(ErrorCode::NotInitialized, "visible capture node not initialized");
    return manager_->startCamera(device::CameraId::Visible);
}

Result<VideoFrame> VisibleCaptureNode::capture(int timeoutMs) {
    if (manager_ == nullptr) return Result<VideoFrame>::error(ErrorCode::NotInitialized, "visible capture node not initialized");
    auto* camera = manager_->visibleCamera();
    if (camera == nullptr) return Result<VideoFrame>::error(ErrorCode::VisibleCameraOffline, "visible camera is not available");
    auto frame = camera->readFrame(timeoutMs);
    if (!frame) return Result<VideoFrame>::error(frame.status().code(), frame.status().describe());
    return Result<VideoFrame>::ok(fromCameraFrame(frame.value()));
}

Result<void> VisibleCaptureNode::stop() {
    if (manager_ == nullptr) return Result<void>::success();
    return manager_->stopCamera(device::CameraId::Visible);
}

} // namespace tri::media
