#pragma once
#include "foundation/error/Result.h"
#include "protocol/gb28181/Gb28181Types.h"
namespace tri::protocol::gb28181 { class GbRegister final { public: tri::foundation::Result<void> init(GbDeviceProfile profile); tri::foundation::Result<void> registerDevice(); bool registered() const noexcept { return registered_; } private: GbDeviceProfile profile_{}; bool registered_{false}; }; }
