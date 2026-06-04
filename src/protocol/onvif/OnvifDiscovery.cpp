#include "protocol/onvif/OnvifDiscovery.h"
#include "foundation/error/ErrorCode.h"
namespace tri::protocol::onvif {
tri::foundation::Result<void> OnvifDiscovery::start(const std::string& endpoint) { if (endpoint.empty()) return tri::foundation::Result<void>::error(tri::foundation::ErrorCode::InvalidArgument, "ONVIF discovery endpoint is empty"); endpoint_ = endpoint; running_ = true; return tri::foundation::Result<void>::success(); }
void OnvifDiscovery::stop() { running_ = false; }
} // namespace tri::protocol::onvif
