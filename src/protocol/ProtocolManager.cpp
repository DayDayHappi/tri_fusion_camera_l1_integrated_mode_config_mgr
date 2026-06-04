#include "protocol/ProtocolManager.h"

#include "foundation/error/ErrorCode.h"
#include "foundation/log/Logger.h"
#include "protocol/ProtocolConfigLoader.h"

namespace tri::protocol {
using tri::foundation::ErrorCode;
using tri::foundation::LogCategory;
using tri::foundation::Result;

Result<void> ProtocolManager::init(const tri::foundation::ProtocolConfig& config,
                                   ProtocolRuntimeContext runtime) {
    return init(config, ProtocolEndpointConfigSet{}, runtime);
}

Result<void> ProtocolManager::init(const tri::foundation::ProtocolConfig& config,
                                   const ProtocolEndpointConfigSet& endpointConfig,
                                   ProtocolRuntimeContext runtime) {
    if (runtime.commandBus == nullptr) return Result<void>::error(ErrorCode::InvalidArgument, "command bus is null");
    if (runtime.protocolService == nullptr) return Result<void>::error(ErrorCode::InvalidArgument, "protocol service is null");
    if (runtime.streamService == nullptr) return Result<void>::error(ErrorCode::InvalidArgument, "stream service is null");

    config_ = config;
    endpointConfig_ = endpointConfig;
    runtime_ = runtime;
    auto g = guard_.init(runtime_.protocolService);
    if (!g) return g;

    activeKind_ = protocolKindFromString(config_.active);
    if (activeKind_ == ProtocolKind::Unknown) {
        return Result<void>::error(ErrorCode::ConfigError, "unknown active protocol: " + config_.active);
    }

    auto active = validateActiveEndpoint();
    if (!active) return active;

    states_.clear();
    states_[ProtocolKind::Gb28181] = ProtocolEndpointState::Stopped;
    states_[ProtocolKind::Onvif] = ProtocolEndpointState::Stopped;
    states_[ProtocolKind::PrivateApi] = ProtocolEndpointState::Stopped;
    states_[ProtocolKind::Rtsp] = ProtocolEndpointState::Stopped;
    initialized_ = true;
    return Result<void>::success();
}

Result<void> ProtocolManager::loadEndpointConfig(const std::string& configDir) {
    auto cfg = ProtocolConfigLoader::loadAll(configDir);
    if (!cfg) return Result<void>::error(cfg.status().code(), cfg.status().describe());
    endpointConfig_ = cfg.value();
    return validateActiveEndpoint();
}

Result<void> ProtocolManager::validateActiveEndpoint() const {
    switch (activeKind_) {
        case ProtocolKind::Gb28181:
            if (!endpointConfig_.gb28181.enable) return Result<void>::error(ErrorCode::ConfigError, "gb28181 endpoint is disabled");
            return ProtocolConfigLoader::validateGb28181(endpointConfig_.gb28181);
        case ProtocolKind::Onvif:
            if (!endpointConfig_.onvif.enable) return Result<void>::error(ErrorCode::ConfigError, "onvif endpoint is disabled");
            return ProtocolConfigLoader::validateOnvif(endpointConfig_.onvif);
        case ProtocolKind::PrivateApi:
            if (!endpointConfig_.privateApi.enable) return Result<void>::error(ErrorCode::ConfigError, "private api endpoint is disabled");
            return ProtocolConfigLoader::validatePrivateApi(endpointConfig_.privateApi);
        case ProtocolKind::Rtsp:
            if (!endpointConfig_.rtsp.enable) return Result<void>::error(ErrorCode::ConfigError, "rtsp endpoint is disabled");
            return ProtocolConfigLoader::validateRtsp(endpointConfig_.rtsp);
        case ProtocolKind::Unknown:
            return Result<void>::error(ErrorCode::ConfigError, "active protocol is unknown");
    }
    return Result<void>::success();
}

Result<void> ProtocolManager::ensureReady() const {
    if (!initialized_) return Result<void>::error(ErrorCode::NotInitialized, "protocol manager is not initialized");
    return Result<void>::success();
}

Result<void> ProtocolManager::startActiveProtocol() {
    auto ready = ensureReady();
    if (!ready) return ready;

    auto acq = guard_.acquire(activeKind_);
    if (!acq) {
        states_[activeKind_] = ProtocolEndpointState::Error;
        return acq;
    }

    switch (activeKind_) {
        case ProtocolKind::Gb28181:
            TRI_LOG_INFO(LogCategory::System)
                << "active protocol started: gb28181, server_id=" << endpointConfig_.gb28181.server.id
                << ", domain=" << endpointConfig_.gb28181.server.domain
                << ", server=" << endpointConfig_.gb28181.server.ip << ":" << endpointConfig_.gb28181.server.port;
            break;
        case ProtocolKind::Onvif:
            TRI_LOG_INFO(LogCategory::System)
                << "active protocol started: onvif, listen=" << endpointConfig_.onvif.listenIp
                << ":" << endpointConfig_.onvif.listenPort;
            break;
        case ProtocolKind::PrivateApi:
            TRI_LOG_INFO(LogCategory::System)
                << "active protocol started: private, listen=" << endpointConfig_.privateApi.listenIp
                << ":" << endpointConfig_.privateApi.listenPort;
            break;
        case ProtocolKind::Rtsp:
            TRI_LOG_INFO(LogCategory::System)
                << "active protocol started: rtsp, bind=" << endpointConfig_.rtsp.bindIp
                << ":" << endpointConfig_.rtsp.port << endpointConfig_.rtsp.path;
            break;
        case ProtocolKind::Unknown:
            break;
    }

    states_[activeKind_] = ProtocolEndpointState::Running;
    return Result<void>::success();
}

void ProtocolManager::stopAll() {
    if (!initialized_) return;
    for (auto& kv : states_) kv.second = ProtocolEndpointState::Stopped;
    if (activeKind_ != ProtocolKind::Unknown) guard_.release(activeKind_);
}

Result<ProtocolResponse> ProtocolManager::submit(ProtocolKind kind,
                                                 tri::command::CommandType type,
                                                 tri::command::CommandParams params) {
    auto ready = ensureReady();
    if (!ready) return Result<ProtocolResponse>::error(ready.status().code(), ready.status().describe());

    ProtocolCommandAdapter adapter;
    auto initRet = adapter.init(kind, runtime_.commandBus, &guard_);
    if (!initRet) return Result<ProtocolResponse>::error(initRet.status().code(), initRet.status().describe());
    return adapter.submit(type, std::move(params));
}

ProtocolEndpointState ProtocolManager::state(ProtocolKind kind) const {
    auto it = states_.find(kind);
    return it == states_.end() ? ProtocolEndpointState::Stopped : it->second;
}

} // namespace tri::protocol
