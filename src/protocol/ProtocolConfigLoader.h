#pragma once

#include "foundation/error/Result.h"
#include "protocol/ProtocolConfigTypes.h"

#include <string>

namespace tri::protocol {

class ProtocolConfigLoader final {
public:
    static tri::foundation::Result<ProtocolEndpointConfigSet> loadAll(const std::string& configDir);

    static tri::foundation::Result<Gb28181Config> loadGb28181(const std::string& path);
    static tri::foundation::Result<OnvifConfig> loadOnvif(const std::string& path);
    static tri::foundation::Result<PrivateApiConfig> loadPrivateApi(const std::string& path);
    static tri::foundation::Result<RtspConfig> loadRtsp(const std::string& path);

    static tri::foundation::Result<void> validate(const ProtocolEndpointConfigSet& config);
    static tri::foundation::Result<void> validateGb28181(const Gb28181Config& config);
    static tri::foundation::Result<void> validateOnvif(const OnvifConfig& config);
    static tri::foundation::Result<void> validatePrivateApi(const PrivateApiConfig& config);
    static tri::foundation::Result<void> validateRtsp(const RtspConfig& config);

    // 为后续 Web 配置预留：Web 修改内存对象后，可调用保存接口落盘。
    static tri::foundation::Result<void> saveGb28181(const std::string& path, const Gb28181Config& config);
    static tri::foundation::Result<void> saveOnvif(const std::string& path, const OnvifConfig& config);
    static tri::foundation::Result<void> savePrivateApi(const std::string& path, const PrivateApiConfig& config);
    static tri::foundation::Result<void> saveRtsp(const std::string& path, const RtspConfig& config);
};

} // namespace tri::protocol
