#include "mode/ModeSwitchPlanner.h"

#include "foundation/error/ErrorCode.h"
#include "media/pipeline/PipelineBuilder.h"

namespace tri::mode {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> ModeSwitchPlanner::init(const foundation::ModeConfig& modeConfig,
                                     const foundation::MediaConfig& mediaConfig) {
    auto ret = policy_.init(modeConfig);
    if (!ret) {
        return ret;
    }

    mediaConfig_ = mediaConfig;
    initialized_ = true;

    return Result<void>::success();
}

Result<ModeSwitchPlan> ModeSwitchPlanner::buildPlan(const std::string& targetMode) const {
    return buildPlan(workModeFromString(targetMode));
}

Result<ModeSwitchPlan> ModeSwitchPlanner::buildPlan(WorkMode targetMode) const {
    if (!initialized_) {
        return Result<ModeSwitchPlan>::error(ErrorCode::NotInitialized,
                                             "mode switch planner is not initialized");
    }

    auto valid = policy_.validateModeEnabled(targetMode);
    if (!valid) {
        return Result<ModeSwitchPlan>::error(valid.status().code(),
                                             valid.status().describe());
    }

    ModeSwitchPlan plan;
    plan.targetMode = targetMode;
    plan.requireVisible = requiresVisibleCamera(targetMode);
    plan.requireComposite = requiresCompositeCamera(targetMode);
    plan.requireCompositeSensorSwitch = requiresCompositeSensorSwitch(targetMode);
    plan.requireFusion = isFusionMode(targetMode);
    plan.fusionMode = fusionModeFor(targetMode);

    if (plan.requireVisible) {
        plan.requiredCameras.push_back(device::CameraId::Visible);
    }

    if (plan.requireComposite) {
        plan.requiredCameras.push_back(device::CameraId::CompositeLowThermal);
    }

    if (plan.requireCompositeSensorSwitch) {
        auto composite = policy_.compositeOutputFor(targetMode);
        if (!composite) {
            return Result<ModeSwitchPlan>::error(composite.status().code(),
                                                 composite.status().describe());
        }

        plan.compositeOutput = composite.value();
    }

    if (targetMode == WorkMode::VisibleOnly) {
        plan.pipeline = media::PipelineBuilder::visibleOnly(mediaConfig_);
    } else if (isFusionMode(targetMode)) {
        plan.pipeline = media::PipelineBuilder::visibleCompositeFusionPlaceholder(mediaConfig_);
    } else {
        // LOWLIGHT_ONLY / THERMAL_ONLY / LOWLIGHT_THERMAL_COMPOSITE
        //
        // 这三个模式都来自复合相机 /dev/tri_composite_video。
        // 当前复合相机输出是 YUYV422，不能走 passthrough。
        //
        // 正确链路：
        //   CompositeCaptureNode
        //   -> FormatConvertNode(NV12)
        //   -> MppH264Encoder
        //   -> MainStream
        plan.pipeline = media::PipelineBuilder::compositeOnlyAuto(mediaConfig_);
    }

    plan.pipelineNodes = describePipelineNodes(plan);
    return Result<ModeSwitchPlan>::ok(std::move(plan));
}

algorithm::FusionMode ModeSwitchPlanner::fusionModeFor(WorkMode mode) noexcept {
    switch (mode) {
        case WorkMode::VisibleLowlightFusion:
            return algorithm::FusionMode::VisibleLowlight;
        case WorkMode::VisibleThermalFusion:
            return algorithm::FusionMode::VisibleThermal;
        case WorkMode::VisibleCompositeFusion:
            return algorithm::FusionMode::VisibleComposite;
        default:
            return algorithm::FusionMode::Unknown;
    }
}

std::vector<std::string> ModeSwitchPlanner::describePipelineNodes(const ModeSwitchPlan& plan) {
    std::vector<std::string> nodes;

    if (plan.requireVisible) {
        nodes.push_back("VisibleCaptureNode");
    }

    if (plan.requireComposite) {
        nodes.push_back("CompositeCaptureNode");
    }

    if (plan.pipeline.useDecoder) {
        nodes.push_back("MjpegDecoder");
    }

    if (plan.requireFusion) {
        nodes.push_back("FrameSync");
        nodes.push_back("ImageRegistrationNode");

        switch (plan.fusionMode) {
            case algorithm::FusionMode::VisibleLowlight:
                nodes.push_back("VisibleLowlightFusion");
                break;
            case algorithm::FusionMode::VisibleThermal:
                nodes.push_back("VisibleThermalFusion");
                break;
            case algorithm::FusionMode::VisibleComposite:
                nodes.push_back("VisibleCompositeFusion");
                break;
            case algorithm::FusionMode::Unknown:
                nodes.push_back("UnknownFusion");
                break;
        }
    }

    if (plan.pipeline.useFormatConvert) {
        nodes.push_back("FormatConvertNode(NV12)");
    }

    if (plan.pipeline.useEncoder) {
        nodes.push_back("MppH264Encoder");
    } else if (plan.pipeline.passthroughEncoded) {
        nodes.push_back("EncodedPassthrough");
    }

    nodes.push_back("MainStream");

    return nodes;
}

} // namespace tri::mode
