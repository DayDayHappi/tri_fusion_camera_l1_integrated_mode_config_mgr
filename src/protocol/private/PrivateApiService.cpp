#include "protocol/private/PrivateApiService.h"
#include "protocol/private/PrivateProtocolParser.h"
namespace tri::protocol::private_api {
tri::foundation::Result<void> PrivateApiService::init(tri::command::CommandBus* bus, tri::protocol::ActiveProtocolGuard* guard) {
    return adapter_.init(bus, guard);
}
tri::foundation::Result<PrivateResponse> PrivateApiService::handleRequest(const PrivateRequest& request) const {
    return adapter_.handle(request);
}
tri::foundation::Result<PrivateResponse> PrivateApiService::handleLine(const std::string& line) const {
    return handleRequest(PrivateProtocolParser::parseLine(line));
}
} // namespace tri::protocol::private_api
