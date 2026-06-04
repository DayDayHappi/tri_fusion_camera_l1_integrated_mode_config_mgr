#include "protocol/gb28181/SipClient.h"

#include "foundation/error/ErrorCode.h"
#include "foundation/log/Logger.h"

#include <utility>

namespace tri::protocol::gb28181 {
using tri::foundation::ErrorCode;
using tri::foundation::LogCategory;
using tri::foundation::Result;

Result<void> SipClient::init(const GbDeviceProfile& profile) {
    if (profile.deviceId.empty()) return Result<void>::error(ErrorCode::InvalidArgument, "SIP local device id is empty");
    if (profile.domain.empty()) return Result<void>::error(ErrorCode::InvalidArgument, "SIP local domain is empty");
    if (profile.ip.empty()) return Result<void>::error(ErrorCode::InvalidArgument, "SIP local ip is empty");
    if (profile.port == 0) return Result<void>::error(ErrorCode::InvalidArgument, "SIP local port is invalid");
    if (profile.serverId.empty()) return Result<void>::error(ErrorCode::InvalidArgument, "SIP server id is empty");
    if (profile.serverIp.empty()) return Result<void>::error(ErrorCode::InvalidArgument, "SIP server ip is empty");
    if (profile.serverPort == 0) return Result<void>::error(ErrorCode::InvalidArgument, "SIP server port is invalid");
    profile_ = profile;
    configured_ = true;
    return Result<void>::success();
}

Result<void> SipClient::init(std::string localId) {
    GbDeviceProfile p;
    p.deviceId = std::move(localId);
    return init(p);
}

Result<void> SipClient::start() {
    if (!configured_) return Result<void>::error(ErrorCode::NotInitialized, "SIP client is not initialized");
    TRI_LOG_INFO(LogCategory::System)
        << "GB28181 SIP client start: local=" << profile_.deviceId
        << "@" << profile_.ip << ":" << profile_.port
        << ", server=" << profile_.serverId
        << "@" << profile_.serverIp << ":" << profile_.serverPort;
    running_ = true;
    return Result<void>::success();
}

void SipClient::stop() { running_ = false; }

} // namespace tri::protocol::gb28181
