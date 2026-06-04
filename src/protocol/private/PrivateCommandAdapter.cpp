#include "protocol/private/PrivateCommandAdapter.h"
#include "foundation/error/ErrorCode.h"
#include "protocol/private/PrivateProtocolParser.h"
namespace tri::protocol::private_api {
using tri::foundation::ErrorCode;
using tri::foundation::Result;
Result<void> PrivateCommandAdapter::init(tri::command::CommandBus* bus, tri::protocol::ActiveProtocolGuard* guard) {
    return adapter_.init(tri::protocol::ProtocolKind::PrivateApi, bus, guard);
}
Result<PrivateResponse> PrivateCommandAdapter::handle(const PrivateRequest& request) const {
    if (request.method == "GET" && request.path == "/api/v1/status") return adapter_.queryStatus();
    if (request.method == "GET" && request.path == "/api/v1/mode") return adapter_.getWorkMode();
    if (request.method == "POST" && request.path == "/api/v1/mode") {
        auto mode = PrivateProtocolParser::getParam(request, "mode");
        if (mode.empty()) mode = request.body;
        if (mode.empty()) return Result<PrivateResponse>::error(ErrorCode::InvalidCommand, "POST /api/v1/mode requires mode");
        return adapter_.setWorkMode(mode);
    }
    if (request.method == "POST" && request.path == "/api/v1/stream/start") return adapter_.startStream();
    if (request.method == "POST" && request.path == "/api/v1/stream/stop") return adapter_.stopStream();
    return Result<PrivateResponse>::error(ErrorCode::NotFound, "unknown private API path: " + request.method + " " + request.path);
}
} // namespace tri::protocol::private_api
