#include "protocol/onvif/OnvifCommandAdapter.h"
#include "foundation/error/ErrorCode.h"
namespace tri::protocol::onvif {
using tri::foundation::ErrorCode;
using tri::foundation::Result;
Result<void> OnvifCommandAdapter::init(tri::command::CommandBus* bus, tri::protocol::ActiveProtocolGuard* guard) {
    return adapter_.init(tri::protocol::ProtocolKind::Onvif, bus, guard);
}
Result<tri::protocol::ProtocolResponse> OnvifCommandAdapter::handle(const OnvifRequest& request) const {
    if (request.action == "GetStatus") return adapter_.queryStatus();
    if (request.action == "GetWorkMode") return adapter_.getWorkMode();
    if (request.action == "SetWorkMode") {
        auto it = request.fields.find("mode");
        if (it == request.fields.end() || it->second.empty()) {
            return Result<tri::protocol::ProtocolResponse>::error(ErrorCode::InvalidCommand, "SetWorkMode requires field: mode");
        }
        return adapter_.setWorkMode(it->second);
    }
    if (request.action == "StartStream") return adapter_.startStream();
    if (request.action == "StopStream") return adapter_.stopStream();
    return Result<tri::protocol::ProtocolResponse>::error(ErrorCode::Unsupported, "unsupported ONVIF action: " + request.action);
}
} // namespace tri::protocol::onvif
