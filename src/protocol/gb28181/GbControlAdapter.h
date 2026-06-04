#pragma once
#include "protocol/ProtocolCommandAdapter.h"
#include "protocol/gb28181/Gb28181Types.h"
namespace tri::protocol::gb28181 {
class GbControlAdapter final {
public:
    tri::foundation::Result<void> init(tri::command::CommandBus* bus, tri::protocol::ActiveProtocolGuard* guard = nullptr);
    tri::foundation::Result<tri::protocol::ProtocolResponse> handle(const GbControlRequest& request) const;
private:
    tri::protocol::ProtocolCommandAdapter adapter_;
};
} // namespace tri::protocol::gb28181
