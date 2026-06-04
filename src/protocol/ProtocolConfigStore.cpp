#include "protocol/ProtocolConfigStore.h"

#include "foundation/error/ErrorCode.h"
#include "protocol/ProtocolConfigLoader.h"

namespace tri::protocol {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

std::string ProtocolConfigStore::pathOf(const std::string& filename) const {
    if (configDir_.empty() || configDir_ == ".") return "./" + filename;
    if (configDir_.back() == '/') return configDir_ + filename;
    return configDir_ + "/" + filename;
}

Result<void> ProtocolConfigStore::load(const std::string& configDir) {
    auto cfg = ProtocolConfigLoader::loadAll(configDir);
    if (!cfg) return Result<void>::error(cfg.status().code(), cfg.status().describe());
    std::lock_guard<std::mutex> lock(mutex_);
    configDir_ = configDir;
    config_ = cfg.value();
    loaded_ = true;
    return Result<void>::success();
}

Result<void> ProtocolConfigStore::save() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!loaded_) return Result<void>::error(ErrorCode::NotInitialized, "protocol config store is not loaded");
    auto r1 = ProtocolConfigLoader::saveGb28181(pathOf("gb28181.yaml"), config_.gb28181); if (!r1) return r1;
    auto r2 = ProtocolConfigLoader::saveOnvif(pathOf("onvif.yaml"), config_.onvif); if (!r2) return r2;
    auto r3 = ProtocolConfigLoader::savePrivateApi(pathOf("private_api.yaml"), config_.privateApi); if (!r3) return r3;
    return ProtocolConfigLoader::saveRtsp(pathOf("rtsp.yaml"), config_.rtsp);
}

ProtocolEndpointConfigSet ProtocolConfigStore::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

Result<void> ProtocolConfigStore::updateGb28181(const Gb28181Config& config, bool persist) {
    auto v = ProtocolConfigLoader::validateGb28181(config);
    if (!v) return v;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.gb28181 = config;
    }
    return persist ? ProtocolConfigLoader::saveGb28181(pathOf("gb28181.yaml"), config) : Result<void>::success();
}

Result<void> ProtocolConfigStore::updateGb28181Server(const Gb28181ServerConfig& server, bool persist) {
    Gb28181Config next;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        next = config_.gb28181;
        next.server = server;
    }
    return updateGb28181(next, persist);
}

Result<void> ProtocolConfigStore::updateOnvif(const OnvifConfig& config, bool persist) {
    auto v = ProtocolConfigLoader::validateOnvif(config);
    if (!v) return v;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.onvif = config;
    }
    return persist ? ProtocolConfigLoader::saveOnvif(pathOf("onvif.yaml"), config) : Result<void>::success();
}

Result<void> ProtocolConfigStore::updatePrivateApi(const PrivateApiConfig& config, bool persist) {
    auto v = ProtocolConfigLoader::validatePrivateApi(config);
    if (!v) return v;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.privateApi = config;
    }
    return persist ? ProtocolConfigLoader::savePrivateApi(pathOf("private_api.yaml"), config) : Result<void>::success();
}

Result<void> ProtocolConfigStore::updateRtsp(const RtspConfig& config, bool persist) {
    auto v = ProtocolConfigLoader::validateRtsp(config);
    if (!v) return v;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.rtsp = config;
    }
    return persist ? ProtocolConfigLoader::saveRtsp(pathOf("rtsp.yaml"), config) : Result<void>::success();
}

std::unordered_map<std::string, std::string> ProtocolConfigStore::flattenGb28181() const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto& c = config_.gb28181;
    return {
        {"gb28181.enable", c.enable ? "true" : "false"},
        {"gb28181.local.device_id", c.local.deviceId},
        {"gb28181.local.domain", c.local.domain},
        {"gb28181.local.ip", c.local.ip},
        {"gb28181.local.port", std::to_string(c.local.port)},
        {"gb28181.local.password", c.local.password},
        {"gb28181.server.id", c.server.id},
        {"gb28181.server.domain", c.server.domain},
        {"gb28181.server.ip", c.server.ip},
        {"gb28181.server.port", std::to_string(c.server.port)},
        {"gb28181.server.password", c.server.password},
        {"gb28181.channel.id", c.channel.id},
        {"gb28181.channel.name", c.channel.name},
        {"gb28181.register.expires_sec", std::to_string(c.reg.expiresSec)},
        {"gb28181.keepalive.interval_sec", std::to_string(c.keepalive.intervalSec)},
    };
}

} // namespace tri::protocol
