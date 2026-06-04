#pragma once

#include "algorithm/registration/RegistrationTypes.h"
#include "foundation/error/Result.h"

#include <string>

namespace tri::algorithm::registration {

class CalibrationManager final {
public:
    foundation::Result<void> load(const std::string& path);
    foundation::Result<void> setProfile(CalibrationProfile profile);
    const CalibrationProfile& profile() const noexcept { return profile_; }
    bool hasValidProfile() const noexcept { return profile_.valid; }
    void clear() noexcept { profile_ = CalibrationProfile{}; }

private:
    CalibrationProfile profile_{};
};

} // namespace tri::algorithm::registration
