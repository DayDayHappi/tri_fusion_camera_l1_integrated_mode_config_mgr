#pragma once

#include "service/ServiceTypes.h"
#include "service/ProtocolService.h"
#include "foundation/error/Result.h"

namespace tri::service {

class StatusService final {
public:
    tri::foundation::Result<void> init(ServiceRuntime runtime,
                                       ProtocolService* protocolService = nullptr);

    tri::foundation::Result<SystemStatus> querySystemStatus() const;
    tri::foundation::Result<std::unordered_map<std::string, std::string>> queryFlatStatus() const;

private:
    ServiceRuntime runtime_{};
    ProtocolService* protocolService_{nullptr};
    bool initialized_{false};
};

} // namespace tri::service
