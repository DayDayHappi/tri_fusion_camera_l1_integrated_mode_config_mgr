#pragma once

#include "foundation/config/ConfigTypes.h"
#include "foundation/error/Result.h"
#include "mode/ModePolicy.h"
#include "mode/ModeTypes.h"
#include "mode/WorkMode.h"

namespace tri::mode {

class ModeSwitchPlanner final {
public:
    foundation::Result<void> init(const foundation::ModeConfig& modeConfig,
                                  const foundation::MediaConfig& mediaConfig);

    foundation::Result<ModeSwitchPlan> buildPlan(WorkMode targetMode) const;
    foundation::Result<ModeSwitchPlan> buildPlan(const std::string& targetMode) const;

    bool initialized() const noexcept { return initialized_; }

private:
    static algorithm::FusionMode fusionModeFor(WorkMode mode) noexcept;
    static std::vector<std::string> describePipelineNodes(const ModeSwitchPlan& plan);

    ModePolicy policy_;
    foundation::MediaConfig mediaConfig_{};
    bool initialized_{false};
};

} // namespace tri::mode
