#include "protocol/gb28181/GbKeepalive.h"
#include "foundation/error/ErrorCode.h"
namespace tri::protocol::gb28181 { tri::foundation::Result<void> GbKeepalive::start(int intervalSeconds) { if (intervalSeconds <= 0) return tri::foundation::Result<void>::error(tri::foundation::ErrorCode::InvalidArgument, "keepalive interval must be positive"); intervalSeconds_ = intervalSeconds; running_ = true; return tri::foundation::Result<void>::success(); } void GbKeepalive::stop() { running_ = false; } }
