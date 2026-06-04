#pragma once

#include "protocol/ActiveProtocolGuard.h"
#include "protocol/ProtocolConfigTypes.h"
#include "protocol/gb28181/GbControlAdapter.h"
#include "protocol/gb28181/GbInviteHandler.h"
#include "protocol/gb28181/GbKeepalive.h"
#include "protocol/gb28181/GbRegister.h"
#include "protocol/gb28181/SipClient.h"
#include "service/StreamService.h"

namespace tri::protocol::gb28181 {

class Gb28181Service final {
public:
    tri::foundation::Result<void> init(const tri::protocol::Gb28181Config& config,
                                       tri::command::CommandBus* bus,
                                       tri::service::StreamService* streamService,
                                       tri::protocol::ActiveProtocolGuard* guard = nullptr);

    tri::foundation::Result<void> init(GbDeviceProfile profile,
                                       tri::command::CommandBus* bus,
                                       tri::service::StreamService* streamService,
                                       tri::protocol::ActiveProtocolGuard* guard = nullptr);
    tri::foundation::Result<void> start();
    void stop();
    tri::foundation::Result<tri::protocol::ProtocolResponse> handleControl(const GbControlRequest& request) const;
    tri::foundation::Result<void> handleInvite(GbInviteContext context);

    const GbDeviceProfile& profile() const noexcept { return profile_; }
    bool initialized() const noexcept { return initialized_; }

private:
    GbDeviceProfile profile_{};
    SipClient sip_{};
    GbRegister reg_{};
    GbKeepalive keepalive_{};
    GbControlAdapter control_{};
    GbRtpSender sender_{};
    GbInviteHandler invite_{};
    bool initialized_{false};
};

} // namespace tri::protocol::gb28181
