#pragma once

#include <cstdint>

namespace tri::foundation {

enum class ErrorCode : std::int32_t {
    Ok = 0,

    // Common / L0
    InvalidArgument = 1000,
    NotInitialized,
    AlreadyInitialized,
    NotFound,
    Timeout,
    Busy,
    IoError,
    PermissionDenied,
    ParseError,
    ConfigError,
    InternalError,
    Unsupported,

    // Project-wide reserved error codes, centralized here for upper layers.
    InvalidCommand = 2000,
    UnsupportedWorkMode,
    ProtocolNotActive,
    ProtocolBusy,
    VisibleCameraOffline,
    CompositeCameraOffline,
    CompositeCameraOpenFailed,
    CompositeMetadataOpenFailed,
    SerialOpenFailed,
    SerialWriteFailed,
    SerialReadTimeout,
    SerialAckInvalid,
    CompositeModeSwitchFailed,
    PipelineBuildFailed,
    PipelineStartFailed,
    MjpegDecodeFailed,
    FrameSyncTimeout,
    ImageRegistrationFailed,
    FusionProcessFailed,
    EncoderInitFailed,
    MainStreamNotReady,
};

bool isOk(ErrorCode code) noexcept;

} // namespace tri::foundation
