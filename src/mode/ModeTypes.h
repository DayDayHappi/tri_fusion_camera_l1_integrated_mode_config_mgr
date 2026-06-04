#pragma once

#include "algorithm/FusionTypes.h"
#include "device/CameraTypes.h"
#include "device_control/CompositeSensorControlTypes.h"
#include "media/pipeline/PipelineTypes.h"
#include "mode/WorkMode.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tri::mode {

enum class ModeSwitchStage : std::uint8_t {
    Idle = 0,
    Planning,
    StoppingPipeline,
    SwitchingCompositeSensor,
    StartingPipeline,
    Running,
    Failed,
};

struct ModeSwitchPlan {
    WorkMode targetMode{WorkMode::Unknown};
    std::vector<device::CameraId> requiredCameras;

    bool requireVisible{false};
    bool requireComposite{false};
    bool requireCompositeSensorSwitch{false};
    device_control::CompositeSensorOutputMode compositeOutput{
        device_control::CompositeSensorOutputMode::Unknown
    };

    bool requireFusion{false};
    algorithm::FusionMode fusionMode{algorithm::FusionMode::Unknown};

    bool restartPipeline{true};
    int stableWaitMs{200};

    media::PipelineConfig pipeline{};
    std::vector<std::string> pipelineNodes;
};

struct ModeState {
    bool initialized{false};
    bool switching{false};
    WorkMode currentMode{WorkMode::Unknown};
    WorkMode previousMode{WorkMode::Unknown};
    ModeSwitchStage stage{ModeSwitchStage::Idle};
    std::uint64_t switchCount{0};
    std::uint64_t errorCount{0};
    std::int64_t lastSwitchBeginMs{0};
    std::int64_t lastSwitchEndMs{0};
    std::string lastError;
};

std::string toString(ModeSwitchStage stage);

} // namespace tri::mode
