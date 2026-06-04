#pragma once

#include "foundation/config/ConfigTypes.h"
#include "foundation/error/Result.h"

namespace tri::foundation {

class ConfigValidator final {
public:
    static Result<void> validate(const SystemConfig& config);
    static Result<void> validateCamera(const CameraConfig& config);
    static Result<void> validateSerial(const SerialConfig& config);
    static Result<void> validateMedia(const MediaConfig& config);
    static Result<void> validateProtocol(const ProtocolConfig& config);
};

} // namespace tri::foundation
