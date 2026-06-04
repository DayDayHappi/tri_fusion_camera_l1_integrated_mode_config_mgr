#pragma once

#include "device_control/CompositeSensorControlTypes.h"
#include "foundation/config/ConfigTypes.h"
#include "foundation/error/Result.h"
#include "mode/WorkMode.h"

namespace tri::mode {

class ModePolicy final {
public:
    foundation::Result<void> init(const foundation::ModeConfig& config);

    foundation::Result<void> validateModeEnabled(WorkMode mode) const;
    foundation::Result<device_control::CompositeSensorOutputMode>
    compositeOutputFor(WorkMode mode) const;

    bool initialized() const noexcept { return initialized_; }

private:
    static device_control::CompositeSensorOutputMode defaultCompositeOutput(WorkMode mode) noexcept;

    foundation::ModeConfig config_{};
    bool initialized_{false};
};

} // namespace tri::mode
