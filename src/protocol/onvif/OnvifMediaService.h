#pragma once
#include "foundation/config/ConfigTypes.h"
#include "foundation/error/Result.h"
#include "protocol/onvif/OnvifTypes.h"
#include <string>
namespace tri::protocol::onvif {
class OnvifMediaService final {
public:
    tri::foundation::Result<void> init(const tri::foundation::MediaConfig& mediaConfig, std::string host);
    tri::foundation::Result<OnvifStreamUri> getStreamUri() const;
private:
    tri::foundation::MediaConfig mediaConfig_{};
    std::string host_;
    bool initialized_{false};
};
} // namespace tri::protocol::onvif
