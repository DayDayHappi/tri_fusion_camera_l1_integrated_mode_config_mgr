#pragma once
#include "protocol/ActiveProtocolGuard.h"
#include "protocol/private/PrivateCommandAdapter.h"
namespace tri::protocol::private_api {
class PrivateApiService final {
public:
    tri::foundation::Result<void> init(tri::command::CommandBus* bus, tri::protocol::ActiveProtocolGuard* guard = nullptr);
    tri::foundation::Result<PrivateResponse> handleRequest(const PrivateRequest& request) const;
    tri::foundation::Result<PrivateResponse> handleLine(const std::string& line) const;
private:
    PrivateCommandAdapter adapter_;
};
} // namespace tri::protocol::private_api
