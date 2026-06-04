#include "service/ModeService.h"

#include "foundation/error/ErrorCode.h"

namespace tri::service {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> ModeService::init(tri::mode::ModeManager* modeManager) {
    if (modeManager == nullptr) {
        return Result<void>::error(ErrorCode::InvalidArgument, "mode manager is null");
    }
    modeManager_ = modeManager;
    return Result<void>::success();
}

Result<void> ModeService::setWorkMode(tri::mode::WorkMode mode) {
    if (modeManager_ == nullptr) {
        return Result<void>::error(ErrorCode::NotInitialized, "mode service is not initialized");
    }
    if (mode == tri::mode::WorkMode::Unknown) {
        return Result<void>::error(ErrorCode::UnsupportedWorkMode, "unknown work mode");
    }
    return modeManager_->switchTo(mode);
}

Result<void> ModeService::setWorkMode(const std::string& mode) {
    return setWorkMode(tri::mode::workModeFromString(mode));
}

Result<tri::mode::WorkMode> ModeService::getWorkMode() const {
    if (modeManager_ == nullptr) {
        return Result<tri::mode::WorkMode>::error(ErrorCode::NotInitialized, "mode service is not initialized");
    }
    return Result<tri::mode::WorkMode>::ok(modeManager_->currentMode());
}

Result<std::vector<tri::mode::WorkMode>> ModeService::getSupportedModes() const {
    std::vector<tri::mode::WorkMode> modes{
        tri::mode::WorkMode::VisibleOnly,
        tri::mode::WorkMode::LowlightOnly,
        tri::mode::WorkMode::ThermalOnly,
        tri::mode::WorkMode::LowlightThermalComposite,
        tri::mode::WorkMode::VisibleLowlightFusion,
        tri::mode::WorkMode::VisibleThermalFusion,
        tri::mode::WorkMode::VisibleCompositeFusion,
    };
    return Result<std::vector<tri::mode::WorkMode>>::ok(std::move(modes));
}

Result<tri::mode::ModeSwitchPlan> ModeService::planFor(tri::mode::WorkMode mode) const {
    if (modeManager_ == nullptr) {
        return Result<tri::mode::ModeSwitchPlan>::error(ErrorCode::NotInitialized, "mode service is not initialized");
    }
    return modeManager_->planFor(mode);
}

Result<tri::mode::ModeSwitchPlan> ModeService::planFor(const std::string& mode) const {
    if (modeManager_ == nullptr) {
        return Result<tri::mode::ModeSwitchPlan>::error(ErrorCode::NotInitialized, "mode service is not initialized");
    }
    return modeManager_->planFor(mode);
}

} // namespace tri::service
