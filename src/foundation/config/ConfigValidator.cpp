#include "foundation/config/ConfigValidator.h"
#include "foundation/utils/NetUtils.h"

namespace tri::foundation {

Result<void> ConfigValidator::validate(const SystemConfig& config) {
    auto r1 = validateCamera(config.camera); if (!r1) return r1;
    auto r2 = validateSerial(config.serial); if (!r2) return r2;
    auto r3 = validateMedia(config.media); if (!r3) return r3;
    auto r4 = validateProtocol(config.protocol); if (!r4) return r4;
    return Result<void>::success();
}

Result<void> ConfigValidator::validateCamera(const CameraConfig& config) {
    if (config.compositeLowThermal.enable && config.compositeLowThermal.videoNode.empty()) {
        return Result<void>::error(ErrorCode::ConfigError, "composite camera video_node is empty");
    }
    if (config.compositeLowThermal.videoNode == config.compositeLowThermal.metadataNode) {
        return Result<void>::error(ErrorCode::ConfigError, "composite video_node and metadata_node must be different");
    }
    if (config.compositeLowThermal.width <= 0 || config.compositeLowThermal.height <= 0 || config.compositeLowThermal.fps <= 0) {
        return Result<void>::error(ErrorCode::ConfigError, "invalid composite camera resolution or fps");
    }
    return Result<void>::success();
}

Result<void> ConfigValidator::validateSerial(const SerialConfig& config) {
    if (config.enable && config.dev.empty()) return Result<void>::error(ErrorCode::ConfigError, "serial dev is empty");
    if (config.baudrate <= 0) return Result<void>::error(ErrorCode::ConfigError, "invalid baudrate");
    if (config.timeoutMs <= 0) return Result<void>::error(ErrorCode::ConfigError, "invalid serial timeout_ms");
    return Result<void>::success();
}

Result<void> ConfigValidator::validateMedia(const MediaConfig& config) {
    if (config.width <= 0 || config.height <= 0 || config.fps <= 0) return Result<void>::error(ErrorCode::ConfigError, "invalid main stream resolution or fps");
    if (config.rtspEnable && !net::isValidPort(config.rtspPort)) return Result<void>::error(ErrorCode::ConfigError, "invalid rtsp port");
    return Result<void>::success();
}

Result<void> ConfigValidator::validateProtocol(const ProtocolConfig& config) {
    if (config.active.empty()) return Result<void>::error(ErrorCode::ConfigError, "active protocol is empty");
    if (!config.allowMultiOnline && config.priority.empty()) return Result<void>::error(ErrorCode::ConfigError, "protocol priority is empty");
    return Result<void>::success();
}

} // namespace tri::foundation
