#include "mode/PrivateVideoRuntime.h"

#include "protocol/private/PrivateControlServer.h"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

namespace tri::mode {
namespace {

using tri::protocol::private_api::PrivateWorkMode;
using tri::protocol::private_api::toCommandName;
using tri::protocol::private_api::toString;

struct VisibleShrinkBorder {
    int left = 0;
    int right = 0;
    int top = 0;
    int bottom = 0;
};

VisibleShrinkBorder splitVisibleShrinkPixels(int horizontalPixels, int verticalPixels) {
    if (horizontalPixels < 0) horizontalPixels = 0;
    if (verticalPixels < 0) verticalPixels = 0;

    VisibleShrinkBorder border;
    border.left = horizontalPixels / 2;
    border.right = horizontalPixels - border.left;
    border.top = verticalPixels / 2;
    border.bottom = verticalPixels - border.top;
    return border;
}

bool isAppFusionMode(PrivateWorkMode mode) {
    return mode == PrivateWorkMode::VisibleLowlightFusion ||
           mode == PrivateWorkMode::VisibleThermalFusion ||
           mode == PrivateWorkMode::VisibleCompositeFusion;
}

std::string trimCopy(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;

    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;

    return text.substr(begin, end - begin);
}

bool parseIntStrict(const std::string& text, int* value) {
    if (value == nullptr) return false;

    try {
        std::size_t consumed = 0;
        const int parsed = std::stoi(trimCopy(text), &consumed, 10);
        const std::string trimmed = trimCopy(text);
        if (consumed != trimmed.size()) return false;
        *value = parsed;
        return true;
    } catch (...) {
        return false;
    }
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

bool PrivateVideoRuntime::setAppFusionVisiblePositionOffset(int offsetX, int offsetY) {
    appFusionVisibleOffsetX_ = offsetX;
    appFusionVisibleOffsetY_ = offsetY;
    if (usingAppFusion_ && appFusionPipeline_.isRunning()) {
        return appFusionPipeline_.setVisiblePositionOffset(offsetX, offsetY);
    }
    return true;
}

bool PrivateVideoRuntime::moveAppFusionVisiblePositionOffset(int deltaX, int deltaY) {
    return setAppFusionVisiblePositionOffset(appFusionVisibleOffsetX_ + deltaX,
                                             appFusionVisibleOffsetY_ + deltaY);
}

std::pair<int, int> PrivateVideoRuntime::appFusionVisiblePositionOffset() const {
    return {appFusionVisibleOffsetX_, appFusionVisibleOffsetY_};
}

bool PrivateVideoRuntime::setAppFusionVisibleShrinkPixels(int horizontalPixels, int verticalPixels) {
    if (horizontalPixels < 0 || verticalPixels < 0) {
        lastError_ = "visible shrink pixels must be non-negative";
        return false;
    }

    appFusionVisibleShrinkHorizontal_ = horizontalPixels;
    appFusionVisibleShrinkVertical_ = verticalPixels;

    if (usingAppFusion_ && appFusionPipeline_.isRunning()) {
        return appFusionPipeline_.setVisibleShrinkPixels(horizontalPixels, verticalPixels);
    }
    return true;
}

std::pair<int, int> PrivateVideoRuntime::appFusionVisibleShrinkPixels() const {
    return {appFusionVisibleShrinkHorizontal_, appFusionVisibleShrinkVertical_};
}

bool PrivateVideoRuntime::saveAppFusionVisibleAdjustment(const std::string& path) {
    if (path.empty()) {
        lastError_ = "visible adjustment save path is empty";
        return false;
    }

    const std::string tmpPath = path + ".tmp";
    std::ofstream out(tmpPath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        lastError_ = "failed to open visible adjustment temp file for write: " + tmpPath;
        return false;
    }

    out << "# Tri-fusion visible fusion adjustment\n";
    out << "# Saved by /api/v1/fusion/visible_adjustment/save\n";
    out << "visible_offset_x=" << appFusionVisibleOffsetX_ << "\n";
    out << "visible_offset_y=" << appFusionVisibleOffsetY_ << "\n";
    out << "visible_shrink_horizontal=" << appFusionVisibleShrinkHorizontal_ << "\n";
    out << "visible_shrink_vertical=" << appFusionVisibleShrinkVertical_ << "\n";
    out.close();

    if (!out) {
        lastError_ = "failed to write visible adjustment temp file: " + tmpPath;
        std::remove(tmpPath.c_str());
        return false;
    }

    if (std::rename(tmpPath.c_str(), path.c_str()) != 0) {
        lastError_ = "failed to rename visible adjustment temp file to final path: " + path;
        std::remove(tmpPath.c_str());
        return false;
    }

    std::cerr << "[VIDEO][RUNTIME] saved visible fusion adjustment path=" << path
              << " offset=" << appFusionVisibleOffsetX_ << "," << appFusionVisibleOffsetY_
              << " shrink=" << appFusionVisibleShrinkHorizontal_ << "," << appFusionVisibleShrinkVertical_
              << "\n";
    return true;
}

bool PrivateVideoRuntime::loadAppFusionVisibleAdjustment(const std::string& path) {
    if (path.empty()) {
        lastError_ = "visible adjustment load path is empty";
        return false;
    }

    std::ifstream in(path);
    if (!in.is_open()) {
        std::cerr << "[VIDEO][RUNTIME] visible fusion adjustment config not found, use defaults path="
                  << path << "\n";
        return true;
    }

    int offsetX = appFusionVisibleOffsetX_;
    int offsetY = appFusionVisibleOffsetY_;
    int shrinkHorizontal = appFusionVisibleShrinkHorizontal_;
    int shrinkVertical = appFusionVisibleShrinkVertical_;

    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        std::string trimmed = trimCopy(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        const std::size_t eq = trimmed.find('=');
        if (eq == std::string::npos) {
            lastError_ = "invalid visible adjustment line without '=' at " + path + ":" + std::to_string(lineNo);
            return false;
        }

        const std::string key = trimCopy(trimmed.substr(0, eq));
        const std::string valueText = trimCopy(trimmed.substr(eq + 1));
        int value = 0;
        if (!parseIntStrict(valueText, &value)) {
            lastError_ = "invalid integer value for key '" + key + "' at " + path + ":" + std::to_string(lineNo);
            return false;
        }

        if (key == "visible_offset_x") {
            offsetX = value;
        } else if (key == "visible_offset_y") {
            offsetY = value;
        } else if (key == "visible_shrink_horizontal") {
            shrinkHorizontal = value;
        } else if (key == "visible_shrink_vertical") {
            shrinkVertical = value;
        } else {
            std::cerr << "[VIDEO][RUNTIME] ignore unknown visible adjustment key=" << key
                      << " path=" << path << " line=" << lineNo << "\n";
        }
    }

    if (offsetX < -4096 || offsetX > 4096 || offsetY < -4096 || offsetY > 4096) {
        lastError_ = "loaded visible offset out of range -4096..4096 from " + path;
        return false;
    }
    if (shrinkHorizontal < 0 || shrinkHorizontal > 798 || shrinkVertical < 0 || shrinkVertical > 598) {
        lastError_ = "loaded visible shrink out of range, horizontal 0..798 vertical 0..598 from " + path;
        return false;
    }

    appFusionVisibleOffsetX_ = offsetX;
    appFusionVisibleOffsetY_ = offsetY;
    appFusionVisibleShrinkHorizontal_ = shrinkHorizontal;
    appFusionVisibleShrinkVertical_ = shrinkVertical;

    std::cerr << "[VIDEO][RUNTIME] loaded visible fusion adjustment path=" << path
              << " offset=" << appFusionVisibleOffsetX_ << "," << appFusionVisibleOffsetY_
              << " shrink=" << appFusionVisibleShrinkHorizontal_ << "," << appFusionVisibleShrinkVertical_
              << "\n";
    return true;
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
    opt.visibleOffsetX = appFusionVisibleOffsetX_;
    opt.visibleOffsetY = appFusionVisibleOffsetY_;

    const auto border = splitVisibleShrinkPixels(appFusionVisibleShrinkHorizontal_,
                                                 appFusionVisibleShrinkVertical_);
    opt.visibleCropLeft = border.left;
    opt.visibleCropRight = border.right;
    opt.visibleCropTop = border.top;
    opt.visibleCropBottom = border.bottom;

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
              << " udp=" << opt.udpHost << ":" << opt.udpPort
              << " visible_shrink_black_border=L" << opt.visibleCropLeft
              << " R" << opt.visibleCropRight
              << " T" << opt.visibleCropTop
              << " B" << opt.visibleCropBottom
              << " horizontal=" << appFusionVisibleShrinkHorizontal_
              << " vertical=" << appFusionVisibleShrinkVertical_
              << " visible_position_offset=" << opt.visibleOffsetX << "," << opt.visibleOffsetY
              << "\n";

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
