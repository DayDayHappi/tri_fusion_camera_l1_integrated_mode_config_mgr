#pragma once

#include "media/gstreamer/GStreamerPipelineConfig.h"

#include <string>
#include <vector>

namespace tri::media::gstreamer {

class GStreamerPipelineBuilder {
public:
    static std::vector<std::string> buildUdpMpegTsH264Args(
        const GStreamerPipelineConfig& config);

    // Backward-compatible wrapper. It now delegates to buildUdpMpegTsH264Args().
    static std::vector<std::string> buildUdpMpegTsYuyvH264Args(
        const GStreamerPipelineConfig& config);

    static std::string toShellCommand(const std::vector<std::string>& args);

private:
    static bool isMjpegInput(const GStreamerPipelineConfig& config);
    static std::string normalizeRawFormat(const std::string& format);
    static std::string boolToGst(bool value);
    static std::string shellQuote(const std::string& value);
};

} // namespace tri::media::gstreamer
