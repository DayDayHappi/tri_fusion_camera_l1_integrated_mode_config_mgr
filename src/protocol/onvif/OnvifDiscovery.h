#pragma once
#include "foundation/error/Result.h"
#include <string>
namespace tri::protocol::onvif {
class OnvifDiscovery final { public: tri::foundation::Result<void> start(const std::string& endpoint); void stop(); bool running() const noexcept { return running_; } private: bool running_{false}; std::string endpoint_; };
} // namespace tri::protocol::onvif
