#include "media/gstreamer/GStreamerPipeline.h"
#include "media/gstreamer/GStreamerPipelineBuilder.h"

#include <utility>

namespace tri::media::gstreamer {

GStreamerPipeline::GStreamerPipeline(GStreamerPipelineConfig config)
    : config_(std::move(config)) {}

void GStreamerPipeline::setConfig(const GStreamerPipelineConfig& config) {
    config_ = config;
}

const GStreamerPipelineConfig& GStreamerPipeline::config() const {
    return config_;
}

bool GStreamerPipeline::start() {
    const auto args = GStreamerPipelineBuilder::buildUdpMpegTsH264Args(config_);
    if (!process_.start(args)) {
        lastError_ = process_.lastError();
        return false;
    }
    lastError_.clear();
    return true;
}

bool GStreamerPipeline::stop() {
    if (!process_.stop(config_.stopTimeoutMs)) {
        lastError_ = process_.lastError();
        return false;
    }
    lastError_.clear();
    return true;
}

bool GStreamerPipeline::restart() {
    if (!stop()) {
        return false;
    }
    return start();
}

bool GStreamerPipeline::isRunning() const {
    return process_.isRunning();
}

std::string GStreamerPipeline::commandLine() const {
    return GStreamerPipelineBuilder::toShellCommand(
        GStreamerPipelineBuilder::buildUdpMpegTsH264Args(config_));
}

const std::string& GStreamerPipeline::lastError() const {
    return lastError_;
}

} // namespace tri::media::gstreamer
