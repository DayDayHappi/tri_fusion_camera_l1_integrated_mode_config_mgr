#pragma once

#include "foundation/error/ErrorCode.h"

namespace tri::device_control {

inline tri::foundation::ErrorCode compositeSensorModeSwitchError() noexcept {
    return tri::foundation::ErrorCode::CompositeModeSwitchFailed;
}

} // namespace tri::device_control
