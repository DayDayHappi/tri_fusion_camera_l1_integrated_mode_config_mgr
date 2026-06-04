#pragma once
#include "protocol/ProtocolCommandAdapter.h"
#include "protocol/private/PrivateProtocolTypes.h"
namespace tri::protocol::private_api {
class PrivateCommandAdapter final {
public:
    tri::foundation::Result<void> init(tri::command::CommandBus* bus, tri::protocol::ActiveProtocolGuard* guard = nullptr);
    tri::foundation::Result<PrivateResponse> handle(const PrivateRequest& request) const;
private:
    tri::protocol::ProtocolCommandAdapter adapter_;
};
} // namespace tri::protocol::private_api
