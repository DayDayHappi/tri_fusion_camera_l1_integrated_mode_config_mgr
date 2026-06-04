#include "protocol/gb28181/Gb28181Service.h"

#include "foundation/error/ErrorCode.h"

#include <utility>

namespace tri::protocol::gb28181 {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> Gb28181Service::init(const tri::protocol::Gb28181Config& config,
                                  tri::command::CommandBus* bus,
                                  tri::service::StreamService* streamService,
                                  tri::protocol::ActiveProtocolGuard* guard) {
    return init(makeGbDeviceProfile(config), bus, streamService, guard);
}

Result<void> Gb28181Service::init(GbDeviceProfile profile,
                                  tri::command::CommandBus* bus,
                                  tri::service::StreamService* streamService,
                                  tri::protocol::ActiveProtocolGuard* guard) {
    if (bus == nullptr) return Result<void>::error(ErrorCode::InvalidArgument, "command bus is null");
    if (streamService == nullptr) return Result<void>::error(ErrorCode::InvalidArgument, "stream service is null");

    profile_ = std::move(profile);
    auto s = sip_.init(profile_); if (!s) return s;
    auto r = reg_.init(profile_); if (!r) return r;
    auto c = control_.init(bus, guard); if (!c) return c;
    auto sender = sender_.init(streamService); if (!sender) return sender;
    auto inv = invite_.init(&sender_); if (!inv) return inv;
    initialized_ = true;
    return Result<void>::success();
}

Result<void> Gb28181Service::start() {
    if (!initialized_) return Result<void>::error(ErrorCode::NotInitialized, "GB28181 service is not initialized");
    auto s = sip_.start(); if (!s) return s;
    auto r = reg_.registerDevice(); if (!r) return r;
    return keepalive_.start(profile_.keepaliveIntervalSec);
}

void Gb28181Service::stop() {
    keepalive_.stop();
    invite_.bye();
    sip_.stop();
}

Result<tri::protocol::ProtocolResponse> Gb28181Service::handleControl(const GbControlRequest& request) const {
    return control_.handle(request);
}

Result<void> Gb28181Service::handleInvite(GbInviteContext context) {
    if (context.ssrc.empty()) context.ssrc = profile_.ssrc;
    return invite_.handleInvite(std::move(context));
}

} // namespace tri::protocol::gb28181
