#pragma once

#include "foundation/config/ConfigTypes.h"
#include "foundation/error/Result.h"
#include "protocol/ActiveProtocolGuard.h"
#include "protocol/ProtocolCommandAdapter.h"
#include "protocol/ProtocolConfigTypes.h"
#include "protocol/ProtocolTypes.h"
#include "service/ProtocolService.h"

#include <unordered_map>

namespace tri::protocol {

class ProtocolManager final {
public:
    tri::foundation::Result<void> init(const tri::foundation::ProtocolConfig& config,
                                       ProtocolRuntimeContext runtime);
    tri::foundation::Result<void> init(const tri::foundation::ProtocolConfig& config,
                                       const ProtocolEndpointConfigSet& endpointConfig,
                                       ProtocolRuntimeContext runtime);
    tri::foundation::Result<void> loadEndpointConfig(const std::string& configDir);

    tri::foundation::Result<void> startActiveProtocol();
    void stopAll();

    tri::foundation::Result<ProtocolResponse> submit(ProtocolKind kind,
                                                     tri::command::CommandType type,
                                                     tri::command::CommandParams params = {});

    ProtocolKind activeKind() const noexcept { return activeKind_; }
    ProtocolEndpointState state(ProtocolKind kind) const;
    bool initialized() const noexcept { return initialized_; }

    const ProtocolEndpointConfigSet& endpointConfig() const noexcept { return endpointConfig_; }

private:
    tri::foundation::Result<void> ensureReady() const;
    tri::foundation::Result<void> validateActiveEndpoint() const;

    tri::foundation::ProtocolConfig config_{};
    ProtocolEndpointConfigSet endpointConfig_{};
    ProtocolRuntimeContext runtime_{};
    ActiveProtocolGuard guard_{};
    ProtocolKind activeKind_{ProtocolKind::Unknown};
    std::unordered_map<ProtocolKind, ProtocolEndpointState> states_;
    bool initialized_{false};
};

} // namespace tri::protocol
