#include "media/gstreamer/GStreamerPipeline.h"
#include "media/gstreamer/GStreamerPipelineBuilder.h"

#include <utility>
#include <iostream>
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
    std::cerr << "[VIDEO][START] starting GStreamer pipeline\n";
    std::cerr << "[VIDEO][START] source=" << config_.sourceName
              << " device=" << config_.device
              << " input_codec=" << config_.inputCodec
              << " raw_format=" << config_.rawFormat
              << " size=" << config_.width << "x" << config_.height
              << " fps=" << config_.fps
              << " encoder=" << config_.encoder
              << " udp=" << config_.udpHost << ":" << config_.udpPort
              << "\n";

    const auto args = GStreamerPipelineBuilder::buildUdpMpegTsH264Args(config_);

    std::cerr << "[VIDEO][GST-CMD] "
              << GStreamerPipelineBuilder::toShellCommand(args)
              << "\n";

    if (!process_.start(args)) {
        lastError_ = process_.lastError();
        std::cerr << "[VIDEO][ERROR] failed to start GStreamer pipeline: "
                  << lastError_ << "\n";
        return false;
    }

    std::cerr << "[VIDEO][START] GStreamer pipeline started\n";
    lastError_.clear();
    return true;
}

bool GStreamerPipeline::stop() {
    std::cerr << "[VIDEO][STOP] stopping GStreamer pipeline\n";

    if (!process_.stop(config_.stopTimeoutMs)) {
        lastError_ = process_.lastError();
        std::cerr << "[VIDEO][ERROR] failed to stop GStreamer pipeline: "
                  << lastError_ << "\n";
        return false;
    }

    std::cerr << "[VIDEO][STOP] GStreamer pipeline stopped\n";
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
