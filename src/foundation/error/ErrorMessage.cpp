#include "foundation/error/ErrorMessage.h"

namespace tri::foundation {

bool isOk(ErrorCode code) noexcept { return code == ErrorCode::Ok; }

std::string errorMessage(ErrorCode code) {
    switch (code) {
        case ErrorCode::Ok: return "OK";
        case ErrorCode::InvalidArgument: return "invalid argument";
        case ErrorCode::NotInitialized: return "not initialized";
        case ErrorCode::AlreadyInitialized: return "already initialized";
        case ErrorCode::NotFound: return "not found";
        case ErrorCode::Timeout: return "timeout";
        case ErrorCode::Busy: return "busy";
        case ErrorCode::IoError: return "I/O error";
        case ErrorCode::PermissionDenied: return "permission denied";
        case ErrorCode::ParseError: return "parse error";
        case ErrorCode::ConfigError: return "configuration error";
        case ErrorCode::InternalError: return "internal error";
        case ErrorCode::Unsupported: return "unsupported";
        case ErrorCode::InvalidCommand: return "invalid command";
        case ErrorCode::UnsupportedWorkMode: return "unsupported work mode";
        case ErrorCode::ProtocolNotActive: return "protocol not active";
        case ErrorCode::ProtocolBusy: return "protocol busy";
        case ErrorCode::VisibleCameraOffline: return "visible camera offline";
        case ErrorCode::CompositeCameraOffline: return "composite camera offline";
        case ErrorCode::CompositeCameraOpenFailed: return "composite camera open failed";
        case ErrorCode::CompositeMetadataOpenFailed: return "composite metadata open failed";
        case ErrorCode::SerialOpenFailed: return "serial open failed";
        case ErrorCode::SerialWriteFailed: return "serial write failed";
        case ErrorCode::SerialReadTimeout: return "serial read timeout";
        case ErrorCode::SerialAckInvalid: return "serial ACK invalid";
        case ErrorCode::CompositeModeSwitchFailed: return "composite sensor mode switch failed";
        case ErrorCode::PipelineBuildFailed: return "pipeline build failed";
        case ErrorCode::PipelineStartFailed: return "pipeline start failed";
        case ErrorCode::MjpegDecodeFailed: return "MJPEG decode failed";
        case ErrorCode::FrameSyncTimeout: return "frame sync timeout";
        case ErrorCode::ImageRegistrationFailed: return "image registration failed";
        case ErrorCode::FusionProcessFailed: return "fusion process failed";
        case ErrorCode::EncoderInitFailed: return "encoder init failed";
        case ErrorCode::MainStreamNotReady: return "main stream not ready";
    }
    return "unknown error";
}

} // namespace tri::foundation
