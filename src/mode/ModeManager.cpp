#include "mode/ModeManager.h"

#include "foundation/error/ErrorCode.h"
#include "foundation/event/Event.h"
#include "foundation/log/Logger.h"
#include "foundation/time/Clock.h"

namespace tri::mode {
using tri::foundation::ErrorCode;
using tri::foundation::Event;
using tri::foundation::EventType;
using tri::foundation::LogCategory;
using tri::foundation::Result;
using tri::foundation::Status;

ModeManager::~ModeManager() { stop(); }

Result<void> ModeManager::init(const foundation::ModeConfig& modeConfig,
                               const foundation::MediaConfig& mediaConfig,
                               ModeRuntimeContext runtime) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_.initialized) {
        return Result<void>::error(ErrorCode::AlreadyInitialized, "mode manager already initialized");
    }

    auto planRet = planner_.init(modeConfig, mediaConfig);
    if (!planRet) return planRet;

    auto execRet = executor_.init(runtime);
    if (!execRet) return execRet;

    state_ = ModeState{};
    state_.initialized = true;
    state_.stage = ModeSwitchStage::Idle;
    return Result<void>::success();
}

Result<ModeSwitchPlan> ModeManager::planFor(WorkMode mode) const {
    return planner_.buildPlan(mode);
}

Result<ModeSwitchPlan> ModeManager::planFor(const std::string& mode) const {
    return planner_.buildPlan(mode);
}

Result<void> ModeManager::switchTo(const std::string& mode) {
    return switchTo(workModeFromString(mode));
}

Result<void> ModeManager::switchTo(WorkMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!state_.initialized) {
        return Result<void>::error(ErrorCode::NotInitialized, "mode manager is not initialized");
    }
    if (state_.switching) {
        return Result<void>::error(ErrorCode::Busy, "mode manager is switching");
    }

    state_.switching = true;
    state_.lastSwitchBeginMs = tri::foundation::Clock::nowMs();
    markStage(ModeSwitchStage::Planning);

    auto planRet = planner_.buildPlan(mode);
    if (!planRet) {
        markError(planRet.status());
        state_.switching = false;
        return Result<void>::error(planRet.status().code(), planRet.status().describe());
    }

    lastPlan_ = planRet.value();

    markStage(ModeSwitchStage::StoppingPipeline);
    markStage(ModeSwitchStage::SwitchingCompositeSensor);
    auto execRet = executor_.execute(lastPlan_);
    if (!execRet) {
        markError(execRet.status());
        state_.switching = false;
        return execRet;
    }

    markStage(ModeSwitchStage::Running);
    state_.previousMode = state_.currentMode;
    state_.currentMode = mode;
    state_.switching = false;
    state_.lastSwitchEndMs = tri::foundation::Clock::nowMs();
    state_.lastError.clear();
    ++state_.switchCount;

    TRI_LOG_INFO(LogCategory::System) << "work mode switched to " << toString(mode);

    if (executor_.context().eventBus != nullptr) {
        Event ev;
        ev.type = EventType::ModeChanged;
        ev.name = "work_mode_changed";
        ev.fields["work_mode"] = toString(mode);
        ev.fields["pipeline_type"] = media::toString(lastPlan_.pipeline.type);
        executor_.context().eventBus->publish(ev);
    }

    return Result<void>::success();
}

void ModeManager::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    executor_.stop();
    state_.stage = ModeSwitchStage::Idle;
    state_.switching = false;
}

void ModeManager::markStage(ModeSwitchStage stage) noexcept {
    state_.stage = stage;
}

void ModeManager::markError(const Status& status) {
    state_.stage = ModeSwitchStage::Failed;
    state_.lastError = status.describe();
    ++state_.errorCount;
    TRI_LOG_ERROR(LogCategory::System) << "mode manager error: " << state_.lastError;
}

} // namespace tri::mode
