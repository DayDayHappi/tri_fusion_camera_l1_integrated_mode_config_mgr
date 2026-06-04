#pragma once
#include <utility>
#include "protocol/gb28181/Gb28181Types.h"
namespace tri::protocol::gb28181 { class GbDeviceInfo final { public: const GbDeviceProfile& profile() const noexcept { return profile_; } void setProfile(GbDeviceProfile profile) { profile_ = std::move(profile); } private: GbDeviceProfile profile_{}; }; }
