#include "foundation/config/ConfigManager.h"
#include "foundation/config/ConfigValidator.h"
#include "foundation/utils/FileUtils.h"
#include "foundation/utils/StringUtils.h"

#include <filesystem>

namespace tri::foundation {

namespace {
int toInt(const FlatConfig& cfg, const std::string& key, int fallback) {
    auto it = cfg.find(key);
    if (it == cfg.end() || it->second.empty()) return fallback;
    try { return std::stoi(it->second); } catch (...) { return fallback; }
}
std::string get(const FlatConfig& cfg, const std::string& key, std::string fallback = {}) {
    auto it = cfg.find(key);
    return it == cfg.end() ? fallback : str::unquote(it->second);
}
bool getBool(const FlatConfig& cfg, const std::string& key, bool fallback = false) {
    auto it = cfg.find(key);
    return it == cfg.end() ? fallback : str::toBool(it->second, fallback);
}
}

ConfigManager& ConfigManager::instance() {
    static ConfigManager manager;
    return manager;
}

Result<void> ConfigManager::loadAll(const std::string& configDir) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto r1 = loadCamera(configDir + "/camera.yaml"); if (!r1) return r1;
    auto r2 = loadSerial(configDir + "/serial.yaml"); if (!r2) return r2;
    auto r3 = loadMode(configDir + "/mode.yaml"); if (!r3) return r3;
    auto r4 = loadMedia(configDir + "/media.yaml"); if (!r4) return r4;
    auto r5 = loadProtocol(configDir + "/protocol.yaml"); if (!r5) return r5;
    auto vr = ConfigValidator::validate(config_); if (!vr) return vr;
    loaded_ = true;
    return Result<void>::success();
}

const SystemConfig& ConfigManager::system() const { return config_; }
const CameraConfig& ConfigManager::camera() const { return config_.camera; }
const SerialConfig& ConfigManager::serial() const { return config_.serial; }
const ModeConfig& ConfigManager::mode() const { return config_.mode; }
const MediaConfig& ConfigManager::media() const { return config_.media; }
const ProtocolConfig& ConfigManager::protocol() const { return config_.protocol; }

Result<void> ConfigManager::loadCamera(const std::string& path) {
    auto cfg = parseSimpleYaml(path); if (!cfg) return Result<void>::error(cfg.status().code(), cfg.status().describe());
    auto& c = config_.camera;
    c.visible.enable = getBool(cfg.value(), "camera.visible.enable", true);
    c.visible.name = get(cfg.value(), "camera.visible.name", "visible_camera");
    c.visible.videoNode = get(cfg.value(), "camera.visible.video_node", "/dev/tri_visible_video");
    c.visible.width = toInt(cfg.value(), "camera.visible.width", 1920);
    c.visible.height = toInt(cfg.value(), "camera.visible.height", 1080);
    c.visible.fps = toInt(cfg.value(), "camera.visible.fps", 30);
    c.visible.format = get(cfg.value(), "camera.visible.format", "mjpeg");

    c.compositeLowThermal.enable = getBool(cfg.value(), "camera.composite_low_thermal.enable", true);
    c.compositeLowThermal.name = get(cfg.value(), "camera.composite_low_thermal.name", "composite_low_thermal_camera");
    c.compositeLowThermal.videoNode = get(cfg.value(), "camera.composite_low_thermal.video_node", "/dev/tri_composite_video");
    c.compositeLowThermal.metadataNode = get(cfg.value(), "camera.composite_low_thermal.metadata_node", "/dev/tri_composite_meta");
    c.compositeLowThermal.width = toInt(cfg.value(), "camera.composite_low_thermal.width", 800);
    c.compositeLowThermal.height = toInt(cfg.value(), "camera.composite_low_thermal.height", 600);
    c.compositeLowThermal.fps = toInt(cfg.value(), "camera.composite_low_thermal.fps", 30);
    c.compositeLowThermal.format = get(cfg.value(), "camera.composite_low_thermal.format", "yuyv422");
    c.compositeLowThermal.metadataFormat = get(cfg.value(), "camera.composite_low_thermal.metadata_format", "uvch");
    return Result<void>::success();
}

Result<void> ConfigManager::loadSerial(const std::string& path) {
    auto cfg = parseSimpleYaml(path); if (!cfg) return Result<void>::error(cfg.status().code(), cfg.status().describe());
    auto& s = config_.serial;
    s.enable = getBool(cfg.value(), "serial.composite_sensor.enable", true);
    s.dev = get(cfg.value(), "serial.composite_sensor.dev", "/dev/tri_composite_serial");
    s.baudrate = toInt(cfg.value(), "serial.composite_sensor.baudrate", 115200);
    s.databits = toInt(cfg.value(), "serial.composite_sensor.databits", 8);
    s.stopbits = toInt(cfg.value(), "serial.composite_sensor.stopbits", 1);
    s.parity = get(cfg.value(), "serial.composite_sensor.parity", "none");
    s.timeoutMs = toInt(cfg.value(), "serial.composite_sensor.timeout_ms", 500);
    s.retryCount = toInt(cfg.value(), "serial.composite_sensor.retry_count", 3);
    const std::string prefix = "serial.composite_sensor.commands.";
    for (const auto& [k,v] : cfg.value()) if (str::startsWith(k, prefix)) s.commands[k.substr(prefix.size())] = str::unquote(v);
    return Result<void>::success();
}

Result<void> ConfigManager::loadMode(const std::string& path) {
    auto cfg = parseSimpleYaml(path); if (!cfg) return Result<void>::error(cfg.status().code(), cfg.status().describe());
    auto& m = config_.mode;
    m.defaultMode = get(cfg.value(), "mode.default", "LOWLIGHT_THERMAL_COMPOSITE");
    const std::string modesPrefix = "mode.modes.";
    for (const auto& [k,v] : cfg.value()) {
        if (!str::startsWith(k, modesPrefix)) continue;
        auto rest = k.substr(modesPrefix.size());
        auto dot = rest.find('.');
        if (dot == std::string::npos) continue;
        auto modeName = rest.substr(0, dot);
        auto field = rest.substr(dot + 1);
        if (field == "enable") m.enabledModes[modeName] = str::toBool(v, true);
        if (field == "composite_output") m.compositeOutputs[modeName] = str::unquote(v);
    }
    return Result<void>::success();
}

Result<void> ConfigManager::loadMedia(const std::string& path) {
    auto cfg = parseSimpleYaml(path); if (!cfg) return Result<void>::error(cfg.status().code(), cfg.status().describe());
    auto& m = config_.media;
    m.codec = get(cfg.value(), "media.main_stream.codec", "h264");
    m.encoder = get(cfg.value(), "media.main_stream.encoder", "mpp");
    m.width = toInt(cfg.value(), "media.main_stream.width", 1920);
    m.height = toInt(cfg.value(), "media.main_stream.height", 1080);
    m.fps = toInt(cfg.value(), "media.main_stream.fps", 25);
    m.bitrate = toInt(cfg.value(), "media.main_stream.bitrate", 4096);
    m.gop = toInt(cfg.value(), "media.main_stream.gop", 25);
    m.lowLatency = getBool(cfg.value(), "media.main_stream.low_latency", true);
    m.dropFrame = getBool(cfg.value(), "media.main_stream.drop_frame", true);
    m.rtspEnable = getBool(cfg.value(), "media.rtsp.enable", true);
    m.rtspPort = toInt(cfg.value(), "media.rtsp.port", 8554);
    m.rtspPath = get(cfg.value(), "media.rtsp.path", "/live/main");
    return Result<void>::success();
}

Result<void> ConfigManager::loadProtocol(const std::string& path) {
    auto cfg = parseSimpleYaml(path); if (!cfg) return Result<void>::error(cfg.status().code(), cfg.status().describe());
    auto& p = config_.protocol;
    p.active = get(cfg.value(), "protocol.active", "gb28181");
    p.allowMultiOnline = getBool(cfg.value(), "protocol.allow_multi_online", false);
    p.reserveMultiOnlineArchitecture = getBool(cfg.value(), "protocol.reserve_multi_online_architecture", true);
    p.priority.clear();
    for (int i=0;;++i) {
        auto key = "protocol.priority." + std::to_string(i);
        auto it = cfg.value().find(key);
        if (it == cfg.value().end()) break;
        p.priority.push_back(str::unquote(it->second));
    }
    if (p.priority.empty()) p.priority = {"gb28181", "onvif", "private"};
    return Result<void>::success();
}

Result<FlatConfig> parseSimpleYaml(const std::string& path) {
    auto text = file::readText(path);
    if (!text) return Result<FlatConfig>::error(text.status().code(), text.status().describe());
    FlatConfig out;
    std::vector<std::pair<int, std::string>> stack;
    std::istringstream in(text.value());
    std::string line;
    int listIndex = 0;
    while (std::getline(in, line)) {
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        if (str::trim(line).empty()) continue;
        int indent = 0; while (indent < static_cast<int>(line.size()) && line[indent] == ' ') ++indent;
        std::string t = str::trim(line);
        while (!stack.empty() && stack.back().first >= indent) stack.pop_back();
        std::string prefix;
        for (std::size_t i=0; i<stack.size(); ++i) { if (i) prefix += '.'; prefix += stack[i].second; }
        if (str::startsWith(t, "- ")) {
            std::string key = prefix + "." + std::to_string(listIndex++);
            out[key] = str::trim(t.substr(2));
            continue;
        }
        listIndex = 0;
        auto colon = t.find(':');
        if (colon == std::string::npos) continue;
        std::string key = str::trim(t.substr(0, colon));
        std::string value = str::trim(t.substr(colon + 1));
        if (value.empty()) {
            stack.push_back({indent, key});
        } else {
            std::string full = prefix.empty() ? key : prefix + "." + key;
            out[full] = value;
        }
    }
    return out;
}

} // namespace tri::foundation
