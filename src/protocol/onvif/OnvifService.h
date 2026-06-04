#pragma once
#include "protocol/onvif/OnvifCommandAdapter.h"
#include "protocol/onvif/OnvifDeviceService.h"
#include "protocol/onvif/OnvifDiscovery.h"
#include "protocol/onvif/OnvifMediaService.h"
namespace tri::protocol::onvif {
class OnvifService final {
public:
    tri::foundation::Result<void> init(tri::command::CommandBus* bus, tri::protocol::ActiveProtocolGuard* guard = nullptr);
    tri::foundation::Result<tri::protocol::ProtocolResponse> handle(const OnvifRequest& request) const;
private:
    OnvifCommandAdapter adapter_;
    OnvifDeviceService device_;
};
} // namespace tri::protocol::onvif
