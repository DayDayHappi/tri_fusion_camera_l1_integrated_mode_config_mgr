#include "mode/ModeTypes.h"

namespace tri::mode {

std::string toString(ModeSwitchStage stage) {
    switch (stage) {
        case ModeSwitchStage::Idle: return "IDLE";
        case ModeSwitchStage::Planning: return "PLANNING";
        case ModeSwitchStage::StoppingPipeline: return "STOPPING_PIPELINE";
        case ModeSwitchStage::SwitchingCompositeSensor: return "SWITCHING_COMPOSITE_SENSOR";
        case ModeSwitchStage::StartingPipeline: return "STARTING_PIPELINE";
        case ModeSwitchStage::Running: return "RUNNING";
        case ModeSwitchStage::Failed: return "FAILED";
    }
    return "UNKNOWN";
}

} // namespace tri::mode
