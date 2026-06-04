#include "mode/ModeSwitchExecutor.h"

#include "foundation/error/ErrorCode.h"
#include "foundation/log/Logger.h"
#include "media/pipeline/PipelineBuilder.h"

#include <cstdlib>

namespace tri::mode {
using tri::foundation::ErrorCode;
using tri::foundation::LogCategory;
using tri::foundation::Result;

ModeSwitchExecutor::~ModeSwitchExecutor() { stop(); }

Result<void> ModeSwitchExecutor::init(ModeRuntimeContext context) {
    context_ = context;
    initialized_ = true;
    return Result<void>::success();
}

void ModeSwitchExecutor::stop() {
    if (currentPipeline_) {
        currentPipeline_->stop();
        currentPipeline_.reset();
    }
}

Result<void> ModeSwitchExecutor::execute(const ModeSwitchPlan& plan) {
    if (!initialized_) return Result<void>::error(ErrorCode::NotInitialized, "mode switch executor is not initialized");

    auto valid = validateContext(plan);
    if (!valid) return valid;

    stop();

    auto fusion = prepareFusion(plan);
    if (!fusion) return fusion;

    auto sensor = switchCompositeSensor(plan);
    if (!sensor) return sensor;

    return startPipeline(plan);
}

Result<void> ModeSwitchExecutor::validateContext(const ModeSwitchPlan& plan) const {
    if (context_.cameraManager == nullptr) {
        return Result<void>::error(ErrorCode::InvalidArgument, "mode runtime camera manager is null");
    }
    if (context_.mainStream == nullptr) {
        return Result<void>::error(ErrorCode::InvalidArgument, "mode runtime main stream is null");
    }
    if (plan.requireCompositeSensorSwitch && context_.compositeController == nullptr) {
        return Result<void>::error(ErrorCode::InvalidArgument, "mode runtime composite sensor controller is null");
    }
    if (plan.requireFusion && context_.fusionEngine == nullptr) {
        return Result<void>::error(ErrorCode::InvalidArgument, "mode runtime fusion engine is null");
    }
    return Result<void>::success();
}

Result<void> ModeSwitchExecutor::prepareFusion(const ModeSwitchPlan& plan) {
    if (!plan.requireFusion) return Result<void>::success();

    if (!context_.fusionEngine->initialized()) {
        auto init = context_.fusionEngine->init();
        if (!init) return init;
        auto reg = context_.fusionEngine->registerDefaultAlgorithms();
        if (!reg) return reg;
    }

    return context_.fusionEngine->setMode(plan.fusionMode);
}

Result<void> ModeSwitchExecutor::switchCompositeSensor(const ModeSwitchPlan& plan) {
    if (!plan.requireCompositeSensorSwitch) return Result<void>::success();

    const char* skipSensorSwitch = std::getenv("TRI_FUSION_SKIP_SENSOR_SWITCH");
    if (skipSensorSwitch != nullptr && skipSensorSwitch[0] == '1') {
        TRI_LOG_INFO(LogCategory::System)
            << "skip composite sensor output switch by env TRI_FUSION_SKIP_SENSOR_SWITCH=1, target="
            << device_control::toString(plan.compositeOutput);
        return Result<void>::success();
    }

    TRI_LOG_INFO(LogCategory::System) << "switch composite sensor output to "
                                      << device_control::toString(plan.compositeOutput);

    auto ret = context_.compositeController->setOutputMode(plan.compositeOutput);
    if (!ret) return ret;

    return context_.compositeController->waitStable(plan.stableWaitMs);
}

Result<void> ModeSwitchExecutor::startPipeline(const ModeSwitchPlan& plan) {
    auto open = context_.cameraManager->openAllEnabled();
    if (!open) return open;

    media::PipelineContext ctx;
    ctx.cameraManager = context_.cameraManager;
    ctx.mainStream = context_.mainStream;
    ctx.eventBus = context_.eventBus;
    ctx.mjpegDecoder = context_.mjpegDecoder;
    ctx.encoder = context_.encoder;

    auto built = media::PipelineBuilder::build(plan.pipeline, ctx);
    if (!built) {
        return Result<void>::error(built.status().code(), built.status().describe());
    }

    currentPipeline_ = built.take();
    auto started = currentPipeline_->start();
    if (!started) {
        currentPipeline_.reset();
        return started;
    }

    return Result<void>::success();
}

} // namespace tri::mode
