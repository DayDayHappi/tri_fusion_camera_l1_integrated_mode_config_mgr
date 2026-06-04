#include "protocol/onvif/OnvifService.h"
namespace tri::protocol::onvif {
tri::foundation::Result<void> OnvifService::init(tri::command::CommandBus* bus, tri::protocol::ActiveProtocolGuard* guard) { return adapter_.init(bus, guard); }
tri::foundation::Result<tri::protocol::ProtocolResponse> OnvifService::handle(const OnvifRequest& request) const { return adapter_.handle(request); }
} // namespace tri::protocol::onvif
