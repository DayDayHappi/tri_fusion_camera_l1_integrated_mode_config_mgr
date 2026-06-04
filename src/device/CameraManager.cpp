#include "device/CameraManager.h"
#include "foundation/log/Logger.h"

namespace tri::device {
using tri::foundation::ErrorCode;
using tri::foundation::Event;
using tri::foundation::EventType;
using tri::foundation::Result;

CameraManager::CameraManager()
    : visible_(std::make_unique<VisibleCameraDevice>()),
      composite_(std::make_unique<CompositeLowThermalCameraDevice>()) {}

CameraManager::~CameraManager() { closeAll(); }

Result<void> CameraManager::init(const tri::foundation::CameraConfig& config,
                                 tri::foundation::EventBus* eventBus) {
    config_ = config;
    eventBus_ = eventBus;

    auto r1 = visible_->init(config.visible);
    if (!r1) return r1;
    auto r2 = composite_->init(config.compositeLowThermal);
    if (!r2) return r2;

    initialized_ = true;
    TRI_LOG_INFO(tri::foundation::LogCategory::System) << "camera manager initialized";
    return Result<void>::success();
}

CameraDevice* CameraManager::select(CameraId id) noexcept {
    switch (id) {
        case CameraId::Visible: return visible_.get();
        case CameraId::CompositeLowThermal: return composite_.get();
    }
    return nullptr;
}

const CameraDevice* CameraManager::select(CameraId id) const noexcept {
    switch (id) {
        case CameraId::Visible: return visible_.get();
        case CameraId::CompositeLowThermal: return composite_.get();
    }
    return nullptr;
}

CameraDevice* CameraManager::camera(CameraId id) noexcept { return select(id); }
const CameraDevice* CameraManager::camera(CameraId id) const noexcept { return select(id); }

Result<void> CameraManager::openAllEnabled() {
    if (!initialized_) return Result<void>::error(ErrorCode::NotInitialized, "camera manager is not initialized");

    if (visible_->status().enabled) {
        auto ret = visible_->open();
        publishCameraEvent(visible_->status(), ret.ok());
        if (!ret) return ret;
    }
    if (composite_->status().enabled) {
        auto ret = composite_->open();
        publishCameraEvent(composite_->status(), ret.ok());
        if (!ret) return ret;
    }
    return Result<void>::success();
}

Result<void> CameraManager::startCamera(CameraId id) {
    if (!initialized_) return Result<void>::error(ErrorCode::NotInitialized, "camera manager is not initialized");
    auto* dev = select(id);
    if (dev == nullptr) return Result<void>::error(ErrorCode::InvalidArgument, "unknown camera id");
    auto ret = dev->start();
    publishCameraEvent(dev->status(), ret.ok());
    return ret;
}

Result<void> CameraManager::stopCamera(CameraId id) {
    auto* dev = select(id);
    if (dev == nullptr) return Result<void>::error(ErrorCode::InvalidArgument, "unknown camera id");
    auto ret = dev->stop();
    publishCameraEvent(dev->status(), false);
    return ret;
}

void CameraManager::stopAll() {
    if (visible_) (void)visible_->stop();
    if (composite_) (void)composite_->stop();
}

void CameraManager::closeAll() {
    if (visible_) visible_->close();
    if (composite_) composite_->close();
}

std::vector<CameraStatus> CameraManager::queryStatus() const {
    std::vector<CameraStatus> out;
    if (visible_) out.push_back(visible_->status());
    if (composite_) out.push_back(composite_->status());
    return out;
}

void CameraManager::publishCameraEvent(const CameraStatus& status, bool online) {
    if (eventBus_ == nullptr) return;
    Event ev;
    ev.type = online ? EventType::CameraOnline : EventType::CameraOffline;
    ev.name = online ? "camera_online" : "camera_offline";
    ev.fields["camera_id"] = toString(status.id);
    ev.fields["camera_kind"] = toString(status.kind);
    ev.fields["state"] = toString(status.state);
    ev.fields["online"] = online ? "true" : "false";
    ev.fields["streaming"] = status.streaming ? "true" : "false";
    if (!status.lastErrorMessage.empty()) ev.fields["last_error"] = status.lastErrorMessage;
    eventBus_->publish(ev);
}

} // namespace tri::device
