#include "mode/ModePolicy.h"

#include "device_control/CompositeSensorControlTypes.h"
#include "foundation/error/ErrorCode.h"

namespace tri::mode {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> ModePolicy::init(const foundation::ModeConfig& config) {
    config_ = config;
    initialized_ = true;
    return Result<void>::success();
}

Result<void> ModePolicy::validateModeEnabled(WorkMode mode) const {
    if (!initialized_) return Result<void>::error(ErrorCode::NotInitialized, "mode policy is not initialized");
    if (mode == WorkMode::Unknown) return Result<void>::error(ErrorCode::InvalidArgument, "unknown work mode");

    const auto key = configKey(mode);
    auto it = config_.enabledModes.find(key);
    if (it != config_.enabledModes.end() && !it->second) {
        return Result<void>::error(ErrorCode::UnsupportedWorkMode, "work mode disabled by config: " + key);
    }
    return Result<void>::success();
}

Result<device_control::CompositeSensorOutputMode>
ModePolicy::compositeOutputFor(WorkMode mode) const {
    if (!requiresCompositeSensorSwitch(mode)) {
        return Result<device_control::CompositeSensorOutputMode>::ok(
            device_control::CompositeSensorOutputMode::Unknown);
    }

    const auto key = configKey(mode);
    auto it = config_.compositeOutputs.find(key);
    if (it != config_.compositeOutputs.end() && !it->second.empty()) {
        auto modeFromConfig = device_control::compositeSensorOutputModeFromString(it->second);
        if (modeFromConfig == device_control::CompositeSensorOutputMode::Unknown) {
            return Result<device_control::CompositeSensorOutputMode>::error(
                ErrorCode::ConfigError,
                "invalid composite_output for mode " + key + ": " + it->second);
        }
        return Result<device_control::CompositeSensorOutputMode>::ok(modeFromConfig);
    }

    auto fallback = defaultCompositeOutput(mode);
    if (fallback == device_control::CompositeSensorOutputMode::Unknown) {
        return Result<device_control::CompositeSensorOutputMode>::error(
            ErrorCode::UnsupportedWorkMode,
            "work mode does not map to composite sensor output: " + toString(mode));
    }
    return Result<device_control::CompositeSensorOutputMode>::ok(fallback);
}

device_control::CompositeSensorOutputMode ModePolicy::defaultCompositeOutput(WorkMode mode) noexcept {
    using device_control::CompositeSensorOutputMode;
    switch (mode) {
        case WorkMode::LowlightOnly:
        case WorkMode::VisibleLowlightFusion:
            return CompositeSensorOutputMode::LowlightOnly;
        case WorkMode::ThermalOnly:
        case WorkMode::VisibleThermalFusion:
            return CompositeSensorOutputMode::ThermalOnly;
        case WorkMode::LowlightThermalComposite:
        case WorkMode::VisibleCompositeFusion:
            return CompositeSensorOutputMode::LowlightThermalComposite;
        case WorkMode::VisibleOnly:
        case WorkMode::Unknown:
            return CompositeSensorOutputMode::Unknown;
    }
    return CompositeSensorOutputMode::Unknown;
}

} // namespace tri::mode
