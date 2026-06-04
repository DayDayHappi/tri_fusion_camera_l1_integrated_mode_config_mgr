#pragma once

#include "command/Command.h"
#include "command/CommandBus.h"
#include "foundation/error/Result.h"
#include "protocol/ActiveProtocolGuard.h"
#include "protocol/ProtocolTypes.h"

#include <string>
#include <unordered_map>

namespace tri::protocol {

class ProtocolCommandAdapter final {
public:
    tri::foundation::Result<void> init(ProtocolKind kind,
                                       tri::command::CommandBus* bus,
                                       ActiveProtocolGuard* guard = nullptr);

    tri::foundation::Result<ProtocolResponse> submit(tri::command::CommandType type,
                                                     tri::command::CommandParams params = {}) const;
    tri::foundation::Result<ProtocolResponse> setWorkMode(const std::string& mode) const;
    tri::foundation::Result<ProtocolResponse> getWorkMode() const;
    tri::foundation::Result<ProtocolResponse> startStream() const;
    tri::foundation::Result<ProtocolResponse> stopStream() const;
    tri::foundation::Result<ProtocolResponse> queryStatus() const;

    ProtocolKind kind() const noexcept { return kind_; }
    tri::command::CommandSource source() const noexcept { return source_; }

private:
    ProtocolKind kind_{ProtocolKind::Unknown};
    tri::command::CommandSource source_{tri::command::CommandSource::Unknown};
    tri::command::CommandBus* bus_{nullptr};
    ActiveProtocolGuard* guard_{nullptr};
};

} // namespace tri::protocol
