#pragma once

#include "foundation/config/ConfigTypes.h"
#include "foundation/error/Result.h"
#include "mode/ModeSwitchExecutor.h"
#include "mode/ModeSwitchPlanner.h"
#include "mode/ModeTypes.h"
#include "mode/WorkMode.h"

#include <mutex>

namespace tri::mode {

class ModeManager final {
public:
    ModeManager() = default;
    ~ModeManager();

    ModeManager(const ModeManager&) = delete;
    ModeManager& operator=(const ModeManager&) = delete;

    foundation::Result<void> init(const foundation::ModeConfig& modeConfig,
                                  const foundation::MediaConfig& mediaConfig,
                                  ModeRuntimeContext runtime);

    foundation::Result<void> switchTo(WorkMode mode);
    foundation::Result<void> switchTo(const std::string& mode);
    foundation::Result<ModeSwitchPlan> planFor(WorkMode mode) const;
    foundation::Result<ModeSwitchPlan> planFor(const std::string& mode) const;

    void stop();

    const ModeState& state() const noexcept { return state_; }
    WorkMode currentMode() const noexcept { return state_.currentMode; }
    const ModeSwitchPlan& lastPlan() const noexcept { return lastPlan_; }

private:
    void markStage(ModeSwitchStage stage) noexcept;
    void markError(const foundation::Status& status);

    mutable std::mutex mutex_;
    ModeSwitchPlanner planner_;
    ModeSwitchExecutor executor_;
    ModeState state_{};
    ModeSwitchPlan lastPlan_{};
};

} // namespace tri::mode
