#include "protocol/gb28181/GbRegister.h"

#include "foundation/error/ErrorCode.h"
#include "foundation/log/Logger.h"

#include <utility>

namespace tri::protocol::gb28181 {
using tri::foundation::ErrorCode;
using tri::foundation::LogCategory;
using tri::foundation::Result;

Result<void> GbRegister::init(GbDeviceProfile profile) {
    if (profile.deviceId.empty()) return Result<void>::error(ErrorCode::InvalidArgument, "GB device id is empty");
    if (profile.serverId.empty()) return Result<void>::error(ErrorCode::InvalidArgument, "GB server id is empty");
    if (profile.serverIp.empty()) return Result<void>::error(ErrorCode::InvalidArgument, "GB server ip is empty");
    profile_ = std::move(profile);
    return Result<void>::success();
}

Result<void> GbRegister::registerDevice() {
    if (profile_.deviceId.empty()) return Result<void>::error(ErrorCode::NotInitialized, "GB register is not initialized");
    TRI_LOG_INFO(LogCategory::System)
        << "GB28181 register device=" << profile_.deviceId
        << ", domain=" << profile_.domain
        << ", server_id=" << profile_.serverId
        << ", server_domain=" << profile_.serverDomain
        << ", server=" << profile_.serverIp << ":" << profile_.serverPort
        << ", expires=" << profile_.registerExpiresSec;
    registered_ = true;
    return Result<void>::success();
}

} // namespace tri::protocol::gb28181
