#pragma once

#include "foundation/error/Result.h"
#include "protocol/gb28181/Gb28181Types.h"

#include <string>

namespace tri::protocol::gb28181 {

class SipClient final {
public:
    tri::foundation::Result<void> init(const GbDeviceProfile& profile);
    tri::foundation::Result<void> init(std::string localId);
    tri::foundation::Result<void> start();
    void stop();

    bool running() const noexcept { return running_; }
    const GbDeviceProfile& profile() const noexcept { return profile_; }

private:
    GbDeviceProfile profile_{};
    bool configured_{false};
    bool running_{false};
};

} // namespace tri::protocol::gb28181
