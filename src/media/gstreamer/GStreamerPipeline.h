#pragma once

#include "media/gstreamer/GStreamerPipelineConfig.h"
#include "media/gstreamer/GStreamerProcess.h"

#include <string>

namespace tri::media::gstreamer {

class GStreamerPipeline {
public:
    explicit GStreamerPipeline(GStreamerPipelineConfig config);

    void setConfig(const GStreamerPipelineConfig& config);
    const GStreamerPipelineConfig& config() const;

    bool start();
    bool stop();
    bool restart();
    bool isRunning() const;

    std::string commandLine() const;

    const std::string& lastError() const;

private:
    GStreamerPipelineConfig config_;
    GStreamerProcess process_;
    std::string lastError_;
};

} // namespace tri::media::gstreamer
