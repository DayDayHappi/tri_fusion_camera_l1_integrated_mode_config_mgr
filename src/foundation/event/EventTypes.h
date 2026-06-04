#pragma once

#include <cstdint>

namespace tri::foundation {

enum class EventType : std::uint32_t {
    Unknown = 0,
    SystemStarted,
    SystemStopping,
    ConfigReloaded,
    CameraOnline,
    CameraOffline,
    ModeChanged,
    PipelineStarted,
    PipelineStopped,
    ProtocolError,
    FaultDetected,
};

} // namespace tri::foundation
