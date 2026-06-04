#pragma once
#include "protocol/ProtocolCommandAdapter.h"
#include "protocol/onvif/OnvifTypes.h"
namespace tri::protocol::onvif {
class OnvifCommandAdapter final {
public:
    tri::foundation::Result<void> init(tri::command::CommandBus* bus, tri::protocol::ActiveProtocolGuard* guard = nullptr);
    tri::foundation::Result<tri::protocol::ProtocolResponse> handle(const OnvifRequest& request) const;
private:
    tri::protocol::ProtocolCommandAdapter adapter_;
};
} // namespace tri::protocol::onvif
