#pragma once

#include "foundation/config/ConfigTypes.h"
#include "foundation/error/Result.h"
#include "media/gstreamer/GStreamerPipelineConfig.h"

#include <string>
#include <unordered_map>

namespace tri::media::gstreamer {

struct GStreamerConfigManagerOptions {
    std::string configDir{"./configs"};
    std::string gstLaunchPath{"gst-launch-1.0"};
    std::string udpHost{"192.168.1.153"};
    int udpPort{5004};
    bool verbose{true};
    bool eosOnStop{true};
    std::string visibleDeviceOverride;
    std::string compositeDeviceOverride;
};

class GStreamerPipelineConfigManager {
public:
    foundation::Result<void> load(const GStreamerConfigManagerOptions& options);

    GStreamerPipelineConfig configForModeCommand(const std::string& commandName) const;
    std::string sourceNameForModeCommand(const std::string& commandName) const;
    const std::string& lastMessage() const noexcept { return lastMessage_; }

private:
    GStreamerPipelineConfig buildFromEndpoint(const std::string& sourceName,
                                              const foundation::CameraEndpointConfig& endpoint) const;
    static std::string normalizeModeCommand(std::string commandName);
    static std::string normalizeSourceName(std::string sourceName);
    static std::string normalizeInputCodec(const std::string& format);
    static std::string normalizeRawFormat(const std::string& format);
    void loadModeSourceMapFromCameraYaml(const std::string& cameraYamlPath);

private:
    GStreamerConfigManagerOptions options_{};
    foundation::CameraConfig camera_{};
    std::unordered_map<std::string, std::string> modeSourceMap_;
    std::string lastMessage_;
};

} // namespace tri::media::gstreamer
