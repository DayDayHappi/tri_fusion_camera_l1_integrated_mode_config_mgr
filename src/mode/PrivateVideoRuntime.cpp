#include "mode/PrivateVideoRuntime.h"

#include "protocol/private/PrivateControlServer.h"

#include <iostream>
#include <sstream>

namespace tri::mode {
namespace {

using tri::protocol::private_api::PrivateWorkMode;
using tri::protocol::private_api::toCommandName;
using tri::protocol::private_api::toString;

bool isAppFusionMode(PrivateWorkMode mode) {
    return mode == PrivateWorkMode::VisibleLowlightFusion ||
           mode == PrivateWorkMode::VisibleThermalFusion ||
           mode == PrivateWorkMode::VisibleCompositeFusion;
}

} // namespace

PrivateVideoRuntime::PrivateVideoRuntime(tri::media::gstreamer::GStreamerPipelineConfigManager& configManager)
    : configManager_(configManager),
      currentMode_(PrivateWorkMode::LowlightThermalComposite) {}

PrivateVideoRuntime::~PrivateVideoRuntime() {
    stop();
}

bool PrivateVideoRuntime::start(PrivateWorkMode mode) {
    lastError_.clear();
    if (isRunning()) {
        lastError_ = "video runtime is already running";
        return false;
    }

    if (isAppFusionMode(mode)) {
        return startAppFusionMode(mode);
    }
    return startGstLaunchMode(mode);
}

bool PrivateVideoRuntime::switchTo(PrivateWorkMode mode) {
    if (!stop()) return false;
    return start(mode);
}

bool PrivateVideoRuntime::stop() {
    bool ok = true;

    if (usingAppFusion_) {
        if (!appFusionPipeline_.stop()) {
            lastError_ = "failed to stop app fusion pipeline: " + appFusionPipeline_.lastError();
            ok = false;
        }
    }

    if (gstPipeline_) {
        if (!gstPipeline_->stop()) {
            lastError_ = "failed to stop gst-launch pipeline: " + gstPipeline_->lastError();
            ok = false;
        }
        gstPipeline_.reset();
    }

    usingAppFusion_ = false;
    activeDescription_.clear();
    return ok;
}

bool PrivateVideoRuntime::isRunning() const {
    if (usingAppFusion_) return appFusionPipeline_.isRunning();
    if (gstPipeline_) return gstPipeline_->isRunning();
    return false;
}

PrivateWorkMode PrivateVideoRuntime::currentMode() const {
    return currentMode_;
}

std::string PrivateVideoRuntime::lastError() const {
    if (usingAppFusion_ && !appFusionPipeline_.lastError().empty()) return appFusionPipeline_.lastError();
    if (gstPipeline_ && !gstPipeline_->lastError().empty()) return gstPipeline_->lastError();
    return lastError_;
}

std::string PrivateVideoRuntime::activeDescription() const {
    if (usingAppFusion_) return appFusionPipeline_.description();
    return activeDescription_;
}

bool PrivateVideoRuntime::startGstLaunchMode(PrivateWorkMode mode) {
    const std::string commandName = toCommandName(mode);
    const auto cfg = configManager_.configForModeCommand(commandName);
    gstPipeline_ = std::make_unique<tri::media::gstreamer::GStreamerPipeline>(cfg);

    std::cerr << "[VIDEO][RUNTIME] start gst-launch mode=" << toString(mode)
              << " command=" << commandName
              << " source=" << cfg.sourceName
              << " device=" << cfg.device
              << " codec=" << cfg.inputCodec
              << " format=" << cfg.rawFormat
              << " size=" << cfg.width << "x" << cfg.height
              << " fps=" << cfg.fps
              << " udp=" << cfg.udpHost << ":" << cfg.udpPort << "\n";
    std::cerr << "[VIDEO][RUNTIME] command_line=" << gstPipeline_->commandLine() << "\n";

    if (!gstPipeline_->start()) {
        lastError_ = "failed to start gst-launch pipeline: " + gstPipeline_->lastError();
        gstPipeline_.reset();
        return false;
    }

    usingAppFusion_ = false;
    currentMode_ = mode;
    hasMode_ = true;
    activeDescription_ = gstPipeline_->commandLine();
    return true;
}

tri::media::app_fusion::AppFusionOptions PrivateVideoRuntime::buildAppFusionOptions(PrivateWorkMode mode) const {
    const auto visibleCfg = configManager_.configForModeCommand(toCommandName(PrivateWorkMode::VisibleOnly));

    // 对三种融合模式，复合相机已经由上层串口切到对应输出：
    // visible_lowlight  -> LowlightOnly
    // visible_thermal   -> ThermalOnly
    // visible_composite -> LowlightThermalComposite
    // 这里直接使用目标模式对应的 composite 采集配置，避免落回单路 composite pipeline。
    const auto targetCfg = configManager_.configForModeCommand(toCommandName(mode));

    tri::media::app_fusion::AppFusionOptions opt;
    opt.visibleDevice = visibleCfg.device.empty() ? "/dev/video3" : visibleCfg.device;
    opt.compositeDevice = targetCfg.device.empty() ? "/dev/video1" : targetCfg.device;
    opt.udpHost = targetCfg.udpHost.empty() ? visibleCfg.udpHost : targetCfg.udpHost;
    opt.udpPort = targetCfg.udpPort > 0 ? targetCfg.udpPort : visibleCfg.udpPort;

    opt.visibleWidth = visibleCfg.width > 0 ? visibleCfg.width : 1600;
    opt.visibleHeight = visibleCfg.height > 0 ? visibleCfg.height : 1200;
    opt.compositeWidth = targetCfg.width > 0 ? targetCfg.width : 800;
    opt.compositeHeight = targetCfg.height > 0 ? targetCfg.height : 600;
    opt.fps = targetCfg.fps > 0 ? targetCfg.fps : 30;

    opt.compositeAlpha = 0.35;
    opt.convertElement = "videoconvert";
    opt.verbose = false;
    return opt;
}

bool PrivateVideoRuntime::startAppFusionMode(PrivateWorkMode mode) {
    const auto opt = buildAppFusionOptions(mode);
    std::cerr << "[VIDEO][RUNTIME] start APP RGB fusion mode=" << toString(mode)
              << " command=" << toCommandName(mode)
              << " visible=" << opt.visibleDevice << " " << opt.visibleWidth << "x" << opt.visibleHeight
              << " composite=" << opt.compositeDevice << " " << opt.compositeWidth << "x" << opt.compositeHeight
              << " fps=" << opt.fps
              << " udp=" << opt.udpHost << ":" << opt.udpPort << "\n";

    if (!appFusionPipeline_.start(opt)) {
        lastError_ = "failed to start app RGB fusion pipeline: " + appFusionPipeline_.lastError();
        return false;
    }

    usingAppFusion_ = true;
    currentMode_ = mode;
    hasMode_ = true;
    activeDescription_ = appFusionPipeline_.description();
    return true;
}

} // namespace tri::mode
