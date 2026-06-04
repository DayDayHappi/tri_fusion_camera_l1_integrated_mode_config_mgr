#pragma once

#include "device_control/CompositeSensorControlTypes.h"
#include <cstdint>
#include <string>

namespace tri::device_control {

struct CompositeSensorState {
    bool initialized{false};
    bool serialOnline{false};
    CompositeSensorOutputMode currentMode{CompositeSensorOutputMode::Unknown};
    std::int64_t lastSwitchTimeMs{0};
    std::uint32_t switchCount{0};
    std::uint32_t errorCount{0};
    std::string lastError;
};

} // namespace tri::device_control
