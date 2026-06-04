#pragma once

#include <cstdint>
#include <string>

namespace tri::mode {

enum class WorkMode : std::uint8_t {
    Unknown = 0,
    VisibleOnly,
    LowlightOnly,
    ThermalOnly,
    LowlightThermalComposite,
    VisibleLowlightFusion,
    VisibleThermalFusion,
    VisibleCompositeFusion,
};

std::string toString(WorkMode mode);
std::string configKey(WorkMode mode);
WorkMode workModeFromString(const std::string& text);

bool requiresVisibleCamera(WorkMode mode) noexcept;
bool requiresCompositeCamera(WorkMode mode) noexcept;
bool requiresCompositeSensorSwitch(WorkMode mode) noexcept;
bool isFusionMode(WorkMode mode) noexcept;

} // namespace tri::mode
