#pragma once
#include <utility>
#include "protocol/gb28181/Gb28181Types.h"
#include <vector>
namespace tri::protocol::gb28181 { class GbCatalog final { public: explicit GbCatalog(GbDeviceProfile profile = {}) : profile_(std::move(profile)) {} std::vector<GbDeviceProfile> list() const { return {profile_}; } void setProfile(GbDeviceProfile profile) { profile_ = std::move(profile); } private: GbDeviceProfile profile_{}; }; }
