#include "protocol/gb28181/GbControlAdapter.h"
#include "foundation/error/ErrorCode.h"
namespace tri::protocol::gb28181 {
using tri::foundation::ErrorCode;
using tri::foundation::Result;
Result<void> GbControlAdapter::init(tri::command::CommandBus* bus, tri::protocol::ActiveProtocolGuard* guard) {
    return adapter_.init(tri::protocol::ProtocolKind::Gb28181, bus, guard);
}
Result<tri::protocol::ProtocolResponse> GbControlAdapter::handle(const GbControlRequest& request) const {
    if (request.action == "DeviceStatus" || request.action == "QueryStatus") return adapter_.queryStatus();
    if (request.action == "GetWorkMode") return adapter_.getWorkMode();
    if (request.action == "SetWorkMode" || request.action == "DeviceControl") {
        auto it = request.fields.find("mode");
        if (it == request.fields.end() || it->second.empty()) return Result<tri::protocol::ProtocolResponse>::error(ErrorCode::InvalidCommand, "GB28181 control requires field: mode");
        return adapter_.setWorkMode(it->second);
    }
    if (request.action == "StartStream") return adapter_.startStream();
    if (request.action == "StopStream") return adapter_.stopStream();
    return Result<tri::protocol::ProtocolResponse>::error(ErrorCode::Unsupported, "unsupported GB28181 control action: " + request.action);
}
} // namespace tri::protocol::gb28181
