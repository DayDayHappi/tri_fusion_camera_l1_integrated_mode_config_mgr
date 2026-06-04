#pragma once

#include "foundation/error/Result.h"
#include "mode/ModeManager.h"
#include "mode/WorkMode.h"

#include <vector>

namespace tri::service {

class ModeService final {
public:
    ModeService() = default;

    tri::foundation::Result<void> init(tri::mode::ModeManager* modeManager);
    tri::foundation::Result<void> setWorkMode(tri::mode::WorkMode mode);
    tri::foundation::Result<void> setWorkMode(const std::string& mode);
    tri::foundation::Result<tri::mode::WorkMode> getWorkMode() const;
    tri::foundation::Result<std::vector<tri::mode::WorkMode>> getSupportedModes() const;
    tri::foundation::Result<tri::mode::ModeSwitchPlan> planFor(tri::mode::WorkMode mode) const;
    tri::foundation::Result<tri::mode::ModeSwitchPlan> planFor(const std::string& mode) const;

    bool initialized() const noexcept { return modeManager_ != nullptr; }

private:
    tri::mode::ModeManager* modeManager_{nullptr};
};

} // namespace tri::service
