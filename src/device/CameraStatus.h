#pragma once

#include "device/CameraTypes.h"
#include "foundation/error/ErrorCode.h"

#include <cstdint>
#include <string>

namespace tri::device {

struct CameraStatus {
    CameraId id{CameraId::Visible};
    CameraKind kind{CameraKind::VisibleUvc};
    CameraLifecycleState state{CameraLifecycleState::Uninitialized};
    bool enabled{false};
    bool configured{false};
    bool opened{false};
    bool streaming{false};
    bool online{false};
    bool metadataAvailable{false};
    std::uint64_t framesRead{0};
    std::uint64_t metadataPacketsRead{0};
    std::uint64_t droppedFrames{0};
    foundation::ErrorCode lastErrorCode{foundation::ErrorCode::Ok};
    std::string lastErrorMessage;
};

} // namespace tri::device
