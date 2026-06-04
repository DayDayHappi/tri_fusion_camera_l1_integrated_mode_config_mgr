#pragma once

#include "algorithm/FusionEngine.h"
#include "device/CameraManager.h"
#include "device_control/CompositeSensorController.h"
#include "foundation/error/Result.h"
#include "foundation/event/EventBus.h"
#include "hardware/mpp/MppDecoder.h"
#include "hardware/mpp/MppEncoder.h"
#include "media/pipeline/MediaPipeline.h"
#include "media/stream/MainStream.h"
#include "mode/ModeTypes.h"

#include <memory>

namespace tri::mode {

struct ModeRuntimeContext {
    device::CameraManager* cameraManager{nullptr};
    device_control::CompositeSensorController* compositeController{nullptr};
    media::MainStream* mainStream{nullptr};
    foundation::EventBus* eventBus{nullptr};
    hardware::mpp::MppDecoder* mjpegDecoder{nullptr};
    hardware::mpp::MppEncoder* encoder{nullptr};
    algorithm::FusionEngine* fusionEngine{nullptr};
};

class ModeSwitchExecutor final {
public:
    ModeSwitchExecutor() = default;
    ~ModeSwitchExecutor();

    ModeSwitchExecutor(const ModeSwitchExecutor&) = delete;
    ModeSwitchExecutor& operator=(const ModeSwitchExecutor&) = delete;

    foundation::Result<void> init(ModeRuntimeContext context);
    foundation::Result<void> execute(const ModeSwitchPlan& plan);
    void stop();

    media::MediaPipeline* currentPipeline() noexcept { return currentPipeline_.get(); }
    const media::MediaPipeline* currentPipeline() const noexcept { return currentPipeline_.get(); }
    const ModeRuntimeContext& context() const noexcept { return context_; }
    bool initialized() const noexcept { return initialized_; }

private:
    foundation::Result<void> validateContext(const ModeSwitchPlan& plan) const;
    foundation::Result<void> prepareFusion(const ModeSwitchPlan& plan);
    foundation::Result<void> switchCompositeSensor(const ModeSwitchPlan& plan);
    foundation::Result<void> startPipeline(const ModeSwitchPlan& plan);

    ModeRuntimeContext context_{};
    std::unique_ptr<media::MediaPipeline> currentPipeline_;
    bool initialized_{false};
};

} // namespace tri::mode
