#include "media/gstreamer/GStreamerPipelineConfigManager.h"

#include "foundation/config/ConfigManager.h"
#include "foundation/utils/StringUtils.h"

#include <algorithm>

namespace tri::media::gstreamer {

namespace {

std::string lowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

} // namespace

foundation::Result<void> GStreamerPipelineConfigManager::load(
    const GStreamerConfigManagerOptions& options) {
    options_ = options;

    auto& cfgMgr = foundation::ConfigManager::instance();
    auto loadRet = cfgMgr.loadAll(options_.configDir);
    if (loadRet) {
        camera_ = cfgMgr.camera();
        lastMessage_ = "loaded camera/media configuration from " + options_.configDir;
    } else {
        // Keep the program usable for board-side smoke tests even if configs were not copied.
        camera_.visible.enable = true;
        camera_.visible.name = "visible_camera";
        camera_.visible.videoNode = "/dev/video3";
        camera_.visible.width = 1920;
        camera_.visible.height = 1080;
        camera_.visible.fps = 30;
        camera_.visible.format = "mjpeg";

        camera_.compositeLowThermal.enable = true;
        camera_.compositeLowThermal.name = "composite_low_thermal_camera";
        camera_.compositeLowThermal.videoNode = "/dev/video1";
        camera_.compositeLowThermal.width = 800;
        camera_.compositeLowThermal.height = 600;
        camera_.compositeLowThermal.fps = 30;
        camera_.compositeLowThermal.format = "yuy2";

        lastMessage_ = "config load failed, using built-in fallback: " + loadRet.status().describe();
    }

    if (!options_.visibleDeviceOverride.empty()) {
        camera_.visible.videoNode = options_.visibleDeviceOverride;
    }
    if (!options_.compositeDeviceOverride.empty()) {
        camera_.compositeLowThermal.videoNode = options_.compositeDeviceOverride;
    }

    modeSourceMap_.clear();
    modeSourceMap_["visible"] = "visible";
    modeSourceMap_["lowlight"] = "composite";
    modeSourceMap_["thermal"] = "composite";
    modeSourceMap_["lowlight_thermal"] = "composite";
    modeSourceMap_["visible_lowlight"] = "composite";
    modeSourceMap_["visible_thermal"] = "composite";
    modeSourceMap_["visible_composite"] = "composite";

    loadModeSourceMapFromCameraYaml(options_.configDir + "/camera.yaml");
    return foundation::Result<void>::success();
}

GStreamerPipelineConfig GStreamerPipelineConfigManager::configForModeCommand(
    const std::string& commandName) const {
    const std::string source = sourceNameForModeCommand(commandName);
    if (source == "visible") {
        return buildFromEndpoint("visible", camera_.visible);
    }
    return buildFromEndpoint("composite", camera_.compositeLowThermal);
}

std::string GStreamerPipelineConfigManager::sourceNameForModeCommand(
    const std::string& commandName) const {
    const auto mode = normalizeModeCommand(commandName);
    const auto it = modeSourceMap_.find(mode);
    if (it == modeSourceMap_.end()) {
        return "composite";
    }
    return normalizeSourceName(it->second);
}

GStreamerPipelineConfig GStreamerPipelineConfigManager::buildFromEndpoint(
    const std::string& sourceName,
    const foundation::CameraEndpointConfig& endpoint) const {
    GStreamerPipelineConfig cfg;
    cfg.sourceName = sourceName;
    cfg.gstLaunchPath = options_.gstLaunchPath;
    cfg.eosOnStop = options_.eosOnStop;
    cfg.verbose = options_.verbose;
    cfg.device = endpoint.videoNode;
    cfg.width = endpoint.width > 0 ? endpoint.width : (sourceName == "visible" ? 1920 : 800);
    cfg.height = endpoint.height > 0 ? endpoint.height : (sourceName == "visible" ? 1080 : 600);
    cfg.fps = endpoint.fps > 0 ? endpoint.fps : 30;
    cfg.inputCodec = normalizeInputCodec(endpoint.format);
    cfg.rawFormat = normalizeRawFormat(endpoint.format);
    cfg.udpHost = options_.udpHost;
    cfg.udpPort = options_.udpPort;
    return cfg;
}

std::string GStreamerPipelineConfigManager::normalizeModeCommand(std::string commandName) {
    commandName = foundation::str::unquote(foundation::str::trim(std::move(commandName)));
    commandName = lowerCopy(commandName);
    if (commandName == "1" || commandName == "visible_only") return "visible";
    if (commandName == "2" || commandName == "lowlight_only") return "lowlight";
    if (commandName == "3" || commandName == "thermal_only") return "thermal";
    if (commandName == "4" || commandName == "lowlight_thermal_composite") return "lowlight_thermal";
    if (commandName == "5" || commandName == "visible_lowlight_fusion") return "visible_lowlight";
    if (commandName == "6" || commandName == "visible_thermal_fusion") return "visible_thermal";
    if (commandName == "7" || commandName == "visible_composite_fusion") return "visible_composite";
    return commandName;
}

std::string GStreamerPipelineConfigManager::normalizeSourceName(std::string sourceName) {
    sourceName = foundation::str::unquote(foundation::str::trim(std::move(sourceName)));
    sourceName = lowerCopy(sourceName);
    if (sourceName == "visible_camera" || sourceName == "visible_only") return "visible";
    if (sourceName == "composite_low_thermal" || sourceName == "low_thermal" || sourceName == "compound") return "composite";
    return sourceName == "visible" ? "visible" : "composite";
}

std::string GStreamerPipelineConfigManager::normalizeInputCodec(const std::string& format) {
    const auto f = lowerCopy(foundation::str::unquote(format));
    if (f == "mjpeg" || f == "mjpg" || f == "jpeg" || f == "image/jpeg") return "mjpeg";
    return "raw";
}

std::string GStreamerPipelineConfigManager::normalizeRawFormat(const std::string& format) {
    const auto f = lowerCopy(foundation::str::unquote(format));
    if (f == "mjpeg" || f == "mjpg" || f == "jpeg" || f == "image/jpeg") return "MJPG";
    if (f == "yuyv" || f == "yuyv422" || f == "yuy2" || f == "yuyv 4:2:2") return "YUY2";
    if (f == "uyvy" || f == "uyvy422") return "UYVY";
    if (f == "nv12") return "NV12";
    return format.empty() ? "YUY2" : format;
}

void GStreamerPipelineConfigManager::loadModeSourceMapFromCameraYaml(
    const std::string& cameraYamlPath) {
    auto parsed = foundation::parseSimpleYaml(cameraYamlPath);
    if (!parsed) {
        return;
    }

    const std::string prefix = "camera.mode_source.";
    for (const auto& kv : parsed.value()) {
        if (!foundation::str::startsWith(kv.first, prefix)) {
            continue;
        }
        const auto modeName = normalizeModeCommand(kv.first.substr(prefix.size()));
        const auto sourceName = normalizeSourceName(kv.second);
        if (!modeName.empty()) {
            modeSourceMap_[modeName] = sourceName;
        }
    }
}

} // namespace tri::media::gstreamer
