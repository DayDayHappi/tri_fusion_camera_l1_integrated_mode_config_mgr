#pragma once

#include "foundation/error/Result.h"
#include "protocol/ProtocolConfigTypes.h"

#include <mutex>
#include <string>
#include <unordered_map>

namespace tri::protocol {

class ProtocolConfigStore final {
public:
    tri::foundation::Result<void> load(const std::string& configDir);
    tri::foundation::Result<void> save() const;

    ProtocolEndpointConfigSet snapshot() const;

    tri::foundation::Result<void> updateGb28181(const Gb28181Config& config, bool persist = false);
    tri::foundation::Result<void> updateGb28181Server(const Gb28181ServerConfig& server, bool persist = false);
    tri::foundation::Result<void> updateOnvif(const OnvifConfig& config, bool persist = false);
    tri::foundation::Result<void> updatePrivateApi(const PrivateApiConfig& config, bool persist = false);
    tri::foundation::Result<void> updateRtsp(const RtspConfig& config, bool persist = false);

    std::unordered_map<std::string, std::string> flattenGb28181() const;

private:
    std::string pathOf(const std::string& filename) const;

    mutable std::mutex mutex_;
    std::string configDir_;
    ProtocolEndpointConfigSet config_{};
    bool loaded_{false};
};

} // namespace tri::protocol
