#include "service/ProtocolService.h"

#include "foundation/error/ErrorCode.h"

namespace tri::service {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> ProtocolService::init(const tri::foundation::ProtocolConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    activeOwner_.clear();
    initialized_ = true;
    return Result<void>::success();
}

Result<void> ProtocolService::acquire(tri::command::CommandSource source) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) return Result<void>::error(ErrorCode::NotInitialized, "protocol service is not initialized");

    const auto name = protocolName(source);
    if (name.empty()) return Result<void>::error(ErrorCode::InvalidArgument, "invalid protocol source");

    if (!config_.allowMultiOnline && !activeOwner_.empty() && activeOwner_ != name) {
        return Result<void>::error(ErrorCode::ProtocolBusy,
                                   "active protocol is " + activeOwner_ + ", reject " + name);
    }

    if (!config_.active.empty() && config_.active != name) {
        return Result<void>::error(ErrorCode::ProtocolNotActive,
                                   "protocol is not active: " + name);
    }

    activeOwner_ = name;
    return Result<void>::success();
}

void ProtocolService::release(tri::command::CommandSource source) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto name = protocolName(source);
    if (activeOwner_ == name) activeOwner_.clear();
}

bool ProtocolService::isActive(tri::command::CommandSource source) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto name = protocolName(source);
    if (!initialized_ || name.empty()) return false;
    if (!config_.active.empty() && config_.active != name) return false;
    if (!config_.allowMultiOnline && !activeOwner_.empty() && activeOwner_ != name) return false;
    return true;
}

std::unordered_map<std::string, std::string> ProtocolService::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_map<std::string, std::string> out;
    out["active"] = config_.active;
    out["owner"] = activeOwner_;
    out["allow_multi_online"] = config_.allowMultiOnline ? "true" : "false";
    out["reserve_multi_online_architecture"] = config_.reserveMultiOnlineArchitecture ? "true" : "false";
    return out;
}

std::string ProtocolService::protocolName(tri::command::CommandSource source) {
    switch (source) {
        case tri::command::CommandSource::Gb28181: return "gb28181";
        case tri::command::CommandSource::Onvif: return "onvif";
        case tri::command::CommandSource::PrivateApi: return "private";
        case tri::command::CommandSource::Rtsp: return "rtsp";
        case tri::command::CommandSource::LocalCli: return "local";
        case tri::command::CommandSource::Internal: return "internal";
        case tri::command::CommandSource::Unknown: return {};
    }
    return {};
}

} // namespace tri::service
