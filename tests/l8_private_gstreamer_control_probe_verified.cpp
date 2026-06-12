#include "device_control/CompositeSensorController.h"
#include "device_control/CompositeSensorControlTypes.h"
#include "foundation/config/ConfigManager.h"
#include "foundation/config/ConfigTypes.h"
#include "media/gstreamer/GStreamerPipelineConfigManager.h"
#include "mode/PrivateVideoRuntime.h"
#include "protocol/private/PrivateControlServer.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <signal.h>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::atomic_bool gStopRequested{false};

void onSignal(int) {
    gStopRequested.store(true);
}

std::string jsonEscape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

std::string hex16(std::uint16_t value) {
    std::ostringstream oss;
    oss << "0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << value;
    return oss.str();
}

tri::protocol::private_api::PrivateHttpResult jsonResult(const std::string& body,
                                                         bool ok,
                                                         int statusCode = 200,
                                                         const std::string& statusText = "OK") {
    tri::protocol::private_api::PrivateHttpResult result;
    result.ok = ok;
    result.statusCode = statusCode;
    result.statusText = statusText;
    result.bodyJson = body;
    return result;
}

tri::protocol::private_api::PrivateHttpResult errorJson(const std::string& action,
                                                        const std::string& error,
                                                        int statusCode = 200) {
    std::ostringstream body;
    body << "{\"ok\":false";
    if (!action.empty()) body << ",\"action\":\"" << jsonEscape(action) << "\"";
    body << ",\"error\":\"" << jsonEscape(error) << "\"}";
    return jsonResult(body.str(), false, statusCode, statusCode == 200 ? "OK" : "Error");
}

void printUsage(const char* program) {
    std::cout
        << "Usage:\n"
        << "  " << program << " [options]\n\n"
        << "Config options:\n"
        << "  --config-dir DIR          config directory, default ./configs\n"
        << "  --visible-device PATH     override visible camera video node\n"
        << "  --composite-device PATH   override composite camera video node\n\n"
        << "Video output options:\n"
        << "  --host IP                 UDP video target host, default value comes from configs/media.yaml or config manager\n"
        << "  --port PORT               UDP video target port, default value comes from configs/media.yaml or config manager\n"
        << "  --gst-launch PATH         gst-launch binary, default gst-launch-1.0\n"
        << "  --no-verbose              do not pass -v to gst-launch mode\n"
        << "  --no-eos                  do not pass -e to gst-launch mode\n\n"
        << "Serial control options:\n"
        << "  --serial-dev PATH         serial device, default value comes from configs/serial.yaml\n"
        << "  --serial-baud N           baudrate, default value comes from configs/serial.yaml\n"
        << "  --serial-timeout-ms N     timeout ms, default value comes from configs/serial.yaml\n"
        << "  --serial-retry N          retry count, default value comes from configs/serial.yaml\n"
        << "  --no-serial               do not switch composite sensor by serial\n"
        << "  --no-ack                  do not wait serial ACK\n"
        << "  --stable-ms N             wait after serial switch, default 500\n\n"
        << "Control options:\n"
        << "  --control-port PORT       private control HTTP port, default 18080\n"
        << "  --control-bind IP         bind address, default 0.0.0.0\n\n"
        << "Compatibility options:\n"
        << "  --device PATH             same as --composite-device\n"
        << "  --width/--height/--fps/--format/--io-mode/--encoder are ignored\n\n"
        << "Composite parameter APIs:\n"
        << "  /api/v1/composite/fusion_color/{black_white|forest|snow|ocean|city|desert|default}\n"
        << "  /api/v1/composite/contour/{off|red|green|blue|purple}\n"
        << "  /api/v1/composite/infrared_polarity/{white_hot|black_hot}\n"
        << "  /api/v1/composite/query_config\n"
        << "  /api/v1/composite/read_all_registers\n"
        << "  /api/v1/composite/registration\n"
        << "  /api/v1/composite/registration/{infrared|lowlight}/{x|y}/{value}\n"
        << "  /api/v1/composite/registration/{infrared|lowlight}/move/{left|right|up|down}/{step}\n";
}

bool readValue(int& i, int argc, char** argv, std::string* out) {
    if (i + 1 >= argc) {
        std::cerr << "[ERROR] missing value after " << argv[i] << "\n";
        return false;
    }
    *out = argv[++i];
    return true;
}

bool readIntValue(int& i, int argc, char** argv, int* out) {
    std::string text;
    if (!readValue(i, argc, argv, &text)) return false;
    try {
        *out = std::stoi(text);
        return true;
    } catch (...) {
        std::cerr << "[ERROR] invalid integer value: " << text << "\n";
        return false;
    }
}

bool mapPrivateModeToCompositeOutput(
    tri::protocol::private_api::PrivateWorkMode mode,
    tri::device_control::CompositeSensorOutputMode* outputMode,
    bool* needCompositeSwitch) {
    using tri::device_control::CompositeSensorOutputMode;
    using tri::protocol::private_api::PrivateWorkMode;

    if (outputMode == nullptr || needCompositeSwitch == nullptr) return false;
    *needCompositeSwitch = true;

    switch (mode) {
        case PrivateWorkMode::VisibleOnly:
            *needCompositeSwitch = false;
            *outputMode = CompositeSensorOutputMode::Unknown;
            return true;
        case PrivateWorkMode::LowlightOnly:
        case PrivateWorkMode::VisibleLowlightFusion:
            *outputMode = CompositeSensorOutputMode::LowlightOnly;
            return true;
        case PrivateWorkMode::ThermalOnly:
        case PrivateWorkMode::VisibleThermalFusion:
            *outputMode = CompositeSensorOutputMode::ThermalOnly;
            return true;
        case PrivateWorkMode::LowlightThermalComposite:
        case PrivateWorkMode::VisibleCompositeFusion:
            *outputMode = CompositeSensorOutputMode::LowlightThermalComposite;
            return true;
    }
    return false;
}

bool parseArgs(int argc,
               char** argv,
               tri::media::gstreamer::GStreamerConfigManagerOptions* gstMgrOptions,
               tri::protocol::private_api::PrivateControlServerConfig* controlCfg,
               tri::foundation::SerialConfig* serialOverride,
               bool* hasSerialOverride,
               bool* serialEnabled,
               bool* ackRequired,
               int* stableMs) {
    if (gstMgrOptions == nullptr || controlCfg == nullptr || serialOverride == nullptr ||
        hasSerialOverride == nullptr || serialEnabled == nullptr || ackRequired == nullptr || stableMs == nullptr) {
        return false;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == "--config-dir") {
            if (!readValue(i, argc, argv, &gstMgrOptions->configDir)) return false;
        } else if (arg == "--visible-device") {
            if (!readValue(i, argc, argv, &gstMgrOptions->visibleDeviceOverride)) return false;
        } else if (arg == "--composite-device" || arg == "--device") {
            if (!readValue(i, argc, argv, &gstMgrOptions->compositeDeviceOverride)) return false;
        } else if (arg == "--host") {
            if (!readValue(i, argc, argv, &gstMgrOptions->udpHost)) return false;
        } else if (arg == "--port") {
            if (!readIntValue(i, argc, argv, &gstMgrOptions->udpPort)) return false;
        } else if (arg == "--gst-launch") {
            if (!readValue(i, argc, argv, &gstMgrOptions->gstLaunchPath)) return false;
        } else if (arg == "--control-bind") {
            if (!readValue(i, argc, argv, &controlCfg->bindAddress)) return false;
        } else if (arg == "--control-port") {
            int port = 0;
            if (!readIntValue(i, argc, argv, &port)) return false;
            if (port <= 0 || port > 65535) {
                std::cerr << "[ERROR] invalid control port: " << port << "\n";
                return false;
            }
            controlCfg->port = static_cast<std::uint16_t>(port);
        } else if (arg == "--serial-dev") {
            *hasSerialOverride = true;
            if (!readValue(i, argc, argv, &serialOverride->dev)) return false;
        } else if (arg == "--serial-baud") {
            *hasSerialOverride = true;
            if (!readIntValue(i, argc, argv, &serialOverride->baudrate)) return false;
        } else if (arg == "--serial-timeout-ms") {
            *hasSerialOverride = true;
            if (!readIntValue(i, argc, argv, &serialOverride->timeoutMs)) return false;
        } else if (arg == "--serial-retry") {
            *hasSerialOverride = true;
            if (!readIntValue(i, argc, argv, &serialOverride->retryCount)) return false;
        } else if (arg == "--stable-ms") {
            if (!readIntValue(i, argc, argv, stableMs)) return false;
        } else if (arg == "--no-serial") {
            *serialEnabled = false;
        } else if (arg == "--no-ack") {
            *ackRequired = false;
        } else if (arg == "--no-verbose") {
            gstMgrOptions->verbose = false;
        } else if (arg == "--no-eos") {
            gstMgrOptions->eosOnStop = false;
        } else if (arg == "--width" || arg == "--height" || arg == "--fps" ||
                   arg == "--format" || arg == "--io-mode" || arg == "--encoder") {
            std::string ignored;
            if (!readValue(i, argc, argv, &ignored)) return false;
            std::cout << "[WARN] " << arg
                      << " is ignored by this version; edit configs/camera.yaml instead.\n";
        } else {
            std::cerr << "[ERROR] unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return false;
        }
    }
    return true;
}

struct CachedCompositeStatus {
    std::string fusionColor{"unknown"};
    std::string contour{"unknown"};
    std::string infraredPolarity{"unknown"};
};

std::string cachedCompositeStatusJson(const CachedCompositeStatus& status) {
    std::ostringstream body;
    body << "{"
         << "\"source\":\"cached\","
         << "\"fusion_color\":\"" << jsonEscape(status.fusionColor) << "\","
         << "\"contour\":\"" << jsonEscape(status.contour) << "\","
         << "\"infrared_polarity\":\"" << jsonEscape(status.infraredPolarity) << "\""
         << "}";
    return body.str();
}

} // namespace

int main(int argc, char** argv) {
    using tri::device_control::CompositeSensorController;
    using tri::device_control::CompositeSensorOutputMode;
    using tri::device_control::ContourMode;
    using tri::device_control::FusionColor;
    using tri::device_control::InfraredPolarity;
    using tri::media::gstreamer::GStreamerConfigManagerOptions;
    using tri::media::gstreamer::GStreamerPipelineConfigManager;
    using tri::mode::PrivateVideoRuntime;
    using tri::protocol::private_api::CompositeControlCallbacks;
    using tri::protocol::private_api::PrivateControlResult;
    using tri::protocol::private_api::PrivateControlServer;
    using tri::protocol::private_api::PrivateControlServerConfig;
    using tri::protocol::private_api::PrivateHttpResult;
    using tri::protocol::private_api::PrivateWorkMode;
    using tri::device_control::CompositeSensorRegister;
    using tri::protocol::private_api::toCommandName;
    using tri::protocol::private_api::toString;

    ::signal(SIGINT, onSignal);
    ::signal(SIGTERM, onSignal);

    GStreamerConfigManagerOptions gstMgrOptions;
    PrivateControlServerConfig controlCfg;

    tri::foundation::SerialConfig serialOverride;
    bool hasSerialOverride = false;
    bool serialEnabled = true;
    bool ackRequired = true;
    int stableMs = 500;

    if (!parseArgs(argc, argv, &gstMgrOptions, &controlCfg, &serialOverride,
                   &hasSerialOverride, &serialEnabled, &ackRequired, &stableMs)) {
        return 2;
    }

    GStreamerPipelineConfigManager gstConfigMgr;
    auto mgrRet = gstConfigMgr.load(gstMgrOptions);
    if (!mgrRet) {
        std::cerr << "[ERROR] failed to initialize GStreamerPipelineConfigManager: "
                  << mgrRet.status().describe() << "\n";
        return 1;
    }
    std::cout << "[CONFIG] " << gstConfigMgr.lastMessage() << "\n";

    auto& globalConfig = tri::foundation::ConfigManager::instance();
    tri::foundation::SerialConfig serialCfg = globalConfig.serial();
    if (serialCfg.dev.empty()) {
        serialCfg.enable = true;
        serialCfg.dev = "/dev/ttyACM0";
        serialCfg.baudrate = 115200;
        serialCfg.databits = 8;
        serialCfg.stopbits = 1;
        serialCfg.parity = "none";
        serialCfg.timeoutMs = 500;
        serialCfg.retryCount = 3;
    }
    if (hasSerialOverride) {
        if (!serialOverride.dev.empty()) serialCfg.dev = serialOverride.dev;
        if (serialOverride.baudrate > 0) serialCfg.baudrate = serialOverride.baudrate;
        if (serialOverride.timeoutMs > 0) serialCfg.timeoutMs = serialOverride.timeoutMs;
        if (serialOverride.retryCount > 0) serialCfg.retryCount = serialOverride.retryCount;
    }

    std::mutex runtimeMutex;
    std::mutex serialMutex;
    std::mutex statusMutex;
    CachedCompositeStatus cachedComposite;

    PrivateWorkMode currentMode = PrivateWorkMode::LowlightThermalComposite;
    PrivateVideoRuntime videoRuntime(gstConfigMgr);
    CompositeSensorController compositeController;

    std::cout << "\n========== Private GStreamer Control Probe ==========" << "\n";
    std::cout << "[CONTROL] port=" << controlCfg.port << "\n";
    std::cout << "[VIDEO] initial_mode=" << toString(currentMode)
              << " command=" << toCommandName(currentMode) << "\n";
    std::cout << "[SERIAL] enabled=" << (serialEnabled ? "true" : "false")
              << " dev=" << serialCfg.dev
              << " baud=" << serialCfg.baudrate
              << " ack=" << (ackRequired ? "true" : "false")
              << " stable_ms=" << stableMs << "\n";
    std::cout << "[INFO] Press Ctrl+C to stop.\n\n";

    if (serialEnabled) {
        compositeController.setAckRequired(ackRequired);
        auto serialInit = compositeController.init(serialCfg, nullptr);
        if (!serialInit) {
            std::cerr << "[ERROR] failed to init CompositeSensorController: "
                      << serialInit.status().describe() << "\n";
            return 1;
        }
        std::cout << "[OK] CompositeSensorController initialized.\n";

        CompositeSensorOutputMode initialCompositeMode = CompositeSensorOutputMode::Unknown;
        bool needInitialCompositeSwitch = false;
        if (mapPrivateModeToCompositeOutput(currentMode, &initialCompositeMode, &needInitialCompositeSwitch) &&
            needInitialCompositeSwitch) {
            std::lock_guard<std::mutex> serialLock(serialMutex);
            std::cout << "[SERIAL] initial composite output mode: "
                      << tri::device_control::toString(initialCompositeMode) << "\n";
            auto switchRet = compositeController.setOutputMode(initialCompositeMode);
            if (!switchRet) {
                std::cerr << "[ERROR] initial serial setOutputMode failed: "
                          << switchRet.status().describe() << "\n";
                compositeController.shutdown();
                return 1;
            }
            auto stableRet = compositeController.waitStable(stableMs);
            if (!stableRet) {
                std::cerr << "[ERROR] initial serial waitStable failed: "
                          << stableRet.status().describe() << "\n";
                compositeController.shutdown();
                return 1;
            }
        }
    } else {
        std::cout << "[WARN] serial control disabled, HTTP mode command will only switch video runtime.\n";
    }

    if (!videoRuntime.start(currentMode)) {
        std::cerr << "[ERROR] failed to start video runtime: " << videoRuntime.lastError() << "\n";
        compositeController.shutdown();
        return 1;
    }
    std::cout << "[OK] video runtime started: " << videoRuntime.activeDescription() << "\n";

    PrivateControlServer server;

    auto switchCallback = [&](PrivateWorkMode targetMode) -> PrivateControlResult {
        std::lock_guard<std::mutex> lock(runtimeMutex);
        PrivateControlResult result;
        result.currentMode = toString(currentMode);

        const std::string commandName = toCommandName(targetMode);
        std::cout << "[CONTROL] mode switch requested: "
                  << toString(currentMode) << " -> " << toString(targetMode)
                  << " command=" << commandName << "\n";

        CompositeSensorOutputMode compositeMode = CompositeSensorOutputMode::Unknown;
        bool needCompositeSwitch = false;
        if (!mapPrivateModeToCompositeOutput(targetMode, &compositeMode, &needCompositeSwitch)) {
            result.ok = false;
            result.message = "unsupported private work mode";
            return result;
        }

        // Stop video first to release /dev/video* before changing composite output mode.
        if (!videoRuntime.stop()) {
            result.ok = false;
            result.message = "failed to stop current video runtime: " + videoRuntime.lastError();
            return result;
        }

        if (serialEnabled && needCompositeSwitch) {
            std::lock_guard<std::mutex> serialLock(serialMutex);
            std::cout << "[SERIAL] set composite output mode: "
                      << tri::device_control::toString(compositeMode) << "\n";
            auto switchRet = compositeController.setOutputMode(compositeMode);
            if (!switchRet) {
                videoRuntime.start(currentMode);
                result.ok = false;
                result.message = "serial setOutputMode failed: " + switchRet.status().describe();
                return result;
            }
            auto stableRet = compositeController.waitStable(stableMs);
            if (!stableRet) {
                videoRuntime.start(currentMode);
                result.ok = false;
                result.message = "serial waitStable failed: " + stableRet.status().describe();
                return result;
            }
        }

        if (!videoRuntime.start(targetMode)) {
            const std::string err = videoRuntime.lastError();
            videoRuntime.start(currentMode);
            result.ok = false;
            result.message = "failed to start target video runtime: " + err;
            return result;
        }

        currentMode = targetMode;
        result.ok = true;
        result.currentMode = toString(currentMode);
        result.message = "mode accepted, video runtime switched, active=" + videoRuntime.activeDescription();
        std::cout << "[OK] " << result.message << "\n";
        return result;
    };

    auto ensureSerialReady = [&]() -> PrivateHttpResult {
        if (!serialEnabled) {
            return errorJson("composite_control", "serial control is disabled by --no-serial");
        }
        if (!compositeController.isReady()) {
            return errorJson("composite_control", "composite sensor controller is not ready");
        }
        return jsonResult("{\"ok\":true}", true);
    };

    auto makeSetResult = [](const std::string& action,
                            const std::string& field,
                            const std::string& valueName,
                            std::uint16_t value,
                            bool ok,
                            const std::string& message) -> PrivateHttpResult {
        std::ostringstream body;
        body << "{"
             << "\"ok\":" << (ok ? "true" : "false") << ","
             << "\"action\":\"" << jsonEscape(action) << "\","
             << "\"result\":\"" << (ok ? "succeeded" : "failed") << "\","
             << "\"" << jsonEscape(field) << "\":\"" << jsonEscape(valueName) << "\","
             << "\"value\":" << value << ","
             << "\"message\":\"" << jsonEscape(message) << "\""
             << "}";
        return jsonResult(body.str(), ok);
    };

    CompositeControlCallbacks compositeCallbacks;

    compositeCallbacks.setFusionColor = [&](const std::string& colorName) -> PrivateHttpResult {
        FusionColor color{};
        if (!tri::device_control::fusionColorFromCommandName(colorName, &color)) {
            return errorJson("set_fusion_color", "unsupported fusion_color: " + colorName + "; supported: black_white, forest, snow, ocean, city, desert, default, 7");
        }
        auto ready = ensureSerialReady();
        if (!ready.ok) return ready;

        const auto value = static_cast<std::uint16_t>(color);
        {
            std::lock_guard<std::mutex> serialLock(serialMutex);
            auto ret = compositeController.setFusionColor(color);
            if (!ret) {
                return makeSetResult("set_fusion_color", "fusion_color", colorName, value, false,
                                     "failed: " + ret.status().describe());
            }
        }
        const std::string canonical = tri::device_control::fusionColorToName(color);
        {
            std::lock_guard<std::mutex> statusLock(statusMutex);
            cachedComposite.fusionColor = canonical;
        }
        return makeSetResult("set_fusion_color", "fusion_color", canonical, value, true,
                             "fusion color updated");
    };

    compositeCallbacks.setContour = [&](const std::string& contourName) -> PrivateHttpResult {
        ContourMode contour{};
        if (!tri::device_control::contourModeFromCommandName(contourName, &contour)) {
            return errorJson("set_contour", "unsupported contour: " + contourName + "; supported: off, red, green, blue, purple");
        }
        auto ready = ensureSerialReady();
        if (!ready.ok) return ready;

        const auto value = static_cast<std::uint16_t>(contour);
        {
            std::lock_guard<std::mutex> serialLock(serialMutex);
            auto ret = compositeController.setContourMode(contour);
            if (!ret) {
                return makeSetResult("set_contour", "contour", contourName, value, false,
                                     "failed: " + ret.status().describe());
            }
        }
        const std::string canonical = tri::device_control::contourModeToName(contour);
        {
            std::lock_guard<std::mutex> statusLock(statusMutex);
            cachedComposite.contour = canonical;
        }
        return makeSetResult("set_contour", "contour", canonical, value, true,
                             "contour updated");
    };

    compositeCallbacks.setInfraredPolarity = [&](const std::string& polarityName) -> PrivateHttpResult {
        InfraredPolarity polarity{};
        if (!tri::device_control::infraredPolarityFromCommandName(polarityName, &polarity)) {
            return errorJson("set_infrared_polarity", "unsupported infrared_polarity: " + polarityName + "; supported: white_hot, black_hot");
        }
        auto ready = ensureSerialReady();
        if (!ready.ok) return ready;

        const auto value = static_cast<std::uint16_t>(polarity);
        {
            std::lock_guard<std::mutex> serialLock(serialMutex);
            auto ret = compositeController.setInfraredPolarity(polarity);
            if (!ret) {
                return makeSetResult("set_infrared_polarity", "infrared_polarity", polarityName, value, false,
                                     "failed: " + ret.status().describe());
            }
        }
        const std::string canonical = tri::device_control::infraredPolarityToName(polarity);
        {
            std::lock_guard<std::mutex> statusLock(statusMutex);
            cachedComposite.infraredPolarity = canonical;
        }
        return makeSetResult("set_infrared_polarity", "infrared_polarity", canonical, value, true,
                             "infrared polarity updated");
    };

    auto readDescriptorList = [&](const std::string& action,
                                  const std::vector<tri::device_control::CompositeRegisterDescriptor>& descriptors,
                                  bool stopOnFirstFailure) -> PrivateHttpResult {
        auto ready = ensureSerialReady();
        if (!ready.ok) return ready;

        std::ostringstream entries;
        std::ostringstream objectConfig;
        int success = 0;
        int failed = 0;
        bool firstEntry = true;
        bool firstObject = true;
        std::string firstError;

        std::lock_guard<std::mutex> serialLock(serialMutex);
        for (const auto& desc : descriptors) {
            auto ret = compositeController.readRegister(desc.reg);
            if (!firstEntry) entries << ",";
            firstEntry = false;

            entries << "{\"name\":\"" << jsonEscape(desc.name) << "\","
                    << "\"address\":\"" << hex16(desc.address) << "\",";
            if (ret) {
                ++success;
                entries << "\"value\":" << ret.value() << ",\"ok\":true}";
                if (!firstObject) objectConfig << ",";
                firstObject = false;
                objectConfig << "\"" << jsonEscape(desc.name) << "\":{"
                             << "\"register\":\"" << hex16(desc.address) << "\","
                             << "\"value\":" << ret.value()
                             << "}";
            } else {
                ++failed;
                const std::string err = ret.status().describe();
                if (firstError.empty()) firstError = std::string("failed to read register ") + desc.name + ": " + err;
                entries << "\"value\":null,\"ok\":false,\"error\":\"" << jsonEscape(err) << "\"}";
                if (stopOnFirstFailure) break;
            }
        }

        if (action == "query_config") {
            std::ostringstream body;
            body << "{\"ok\":" << (failed == 0 ? "true" : "false")
                 << ",\"action\":\"query_config\"";
            if (failed != 0) {
                body << ",\"error\":\"" << jsonEscape(firstError) << "\""
                     << ",\"partial_config\":{" << objectConfig.str() << "}";
            } else {
                body << ",\"config\":{" << objectConfig.str() << "}";
            }
            body << "}";
            return jsonResult(body.str(), failed == 0);
        }

        std::ostringstream body;
        body << "{\"ok\":" << (failed == 0 ? "true" : "false")
             << ",\"action\":\"read_all_registers\""
             << ",\"total\":" << (success + failed)
             << ",\"success\":" << success
             << ",\"failed\":" << failed
             << ",\"registers\":[" << entries.str() << "]} ";
        return jsonResult(body.str(), failed == 0);
    };

    auto registrationRegister = [](const std::string& sensor,
                                   const std::string& axis,
                                   CompositeSensorRegister* reg) -> bool {
        if (reg == nullptr) return false;
        if (sensor == "infrared" && axis == "x") {
            *reg = CompositeSensorRegister::InfraredRegistrationOffsetX;
            return true;
        }
        if (sensor == "infrared" && axis == "y") {
            *reg = CompositeSensorRegister::InfraredRegistrationOffsetY;
            return true;
        }
        if (sensor == "lowlight" && axis == "x") {
            *reg = CompositeSensorRegister::LowlightRegistrationOffsetX;
            return true;
        }
        if (sensor == "lowlight" && axis == "y") {
            *reg = CompositeSensorRegister::LowlightRegistrationOffsetY;
            return true;
        }
        return false;
    };

    auto setRegistrationValueVerifiedLocked = [&](CompositeSensorRegister reg,
                                                  std::int16_t value) -> tri::foundation::Result<std::uint16_t> {
        return compositeController.writeRegisterVerified(
            reg,
            static_cast<std::uint16_t>(value));
    };

    compositeCallbacks.setRegistrationOffset =
        [&](const std::string& sensor, const std::string& axis, int value) -> PrivateHttpResult {
        CompositeSensorRegister reg{};
        if (!registrationRegister(sensor, axis, &reg)) {
            return errorJson("set_registration_offset",
                             "sensor must be infrared or lowlight, axis must be x or y");
        }
        if (value < -32768 || value > 32767) {
            return errorJson("set_registration_offset", "value must be in int16 range -32768..32767");
        }
        auto ready = ensureSerialReady();
        if (!ready.ok) return ready;

        const auto requestedValue = static_cast<std::int16_t>(value);
        std::uint16_t actualRaw = 0;
        {
            std::lock_guard<std::mutex> serialLock(serialMutex);
            auto ret = setRegistrationValueVerifiedLocked(reg, requestedValue);
            if (!ret) {
                return errorJson("set_registration_offset", ret.status().describe());
            }
            actualRaw = ret.value();
        }

        const auto actualValue = static_cast<std::int16_t>(actualRaw);

        std::ostringstream body;
        body << "{\"ok\":true,\"action\":\"set_registration_offset\""
             << ",\"sensor\":\"" << jsonEscape(sensor) << "\""
             << ",\"axis\":\"" << jsonEscape(axis) << "\""
             << ",\"register\":\"" << hex16(static_cast<std::uint16_t>(reg)) << "\""
             << ",\"requested_value\":" << value
             << ",\"actual_value\":" << actualValue
             << ",\"raw_value\":" << actualRaw
             << ",\"verified\":true"
             << ",\"message\":\"registration offset updated and verified\"}";
        return jsonResult(body.str(), true);
    };

    compositeCallbacks.moveRegistrationOffset =
        [&](const std::string& sensor, const std::string& direction, int step) -> PrivateHttpResult {
        std::string axis;
        int delta = 0;
        if (direction == "left")  { axis = "x"; delta = -step; }
        else if (direction == "right") { axis = "x"; delta = step; }
        else if (direction == "up")    { axis = "y"; delta = -step; }
        else if (direction == "down")  { axis = "y"; delta = step; }
        else {
            return errorJson("move_registration_offset",
                             "direction must be left, right, up or down");
        }

        CompositeSensorRegister reg{};
        if (!registrationRegister(sensor, axis, &reg)) {
            return errorJson("move_registration_offset", "sensor must be infrared or lowlight");
        }
        auto ready = ensureSerialReady();
        if (!ready.ok) return ready;

        if (step <= 0) {
            return errorJson("move_registration_offset", "step must be greater than 0");
        }

        int oldValue = 0;
        int requestedValue = 0;
        std::uint16_t actualRaw = 0;
        {
            std::lock_guard<std::mutex> serialLock(serialMutex);
            auto readRet = compositeController.readRegister(reg);
            if (!readRet) {
                return errorJson("move_registration_offset",
                                 "failed to read current offset: " + readRet.status().describe());
            }
            oldValue = static_cast<std::int16_t>(readRet.value());
            requestedValue = oldValue + delta;
            if (requestedValue < -32768 || requestedValue > 32767) {
                return errorJson("move_registration_offset",
                                 "new value exceeds int16 range -32768..32767");
            }
            auto writeRet = setRegistrationValueVerifiedLocked(
                reg, static_cast<std::int16_t>(requestedValue));
            if (!writeRet) {
                return errorJson("move_registration_offset", writeRet.status().describe());
            }
            actualRaw = writeRet.value();
        }

        const auto actualValue = static_cast<std::int16_t>(actualRaw);

        std::ostringstream body;
        body << "{\"ok\":true,\"action\":\"move_registration_offset\""
             << ",\"sensor\":\"" << jsonEscape(sensor) << "\""
             << ",\"direction\":\"" << jsonEscape(direction) << "\""
             << ",\"axis\":\"" << axis << "\""
             << ",\"step\":" << step
             << ",\"old_value\":" << oldValue
             << ",\"requested_value\":" << requestedValue
             << ",\"actual_value\":" << actualValue
             << ",\"raw_value\":" << actualRaw
             << ",\"verified\":true"
             << ",\"register\":\"" << hex16(static_cast<std::uint16_t>(reg)) << "\"}";
        return jsonResult(body.str(), true);
    };

    compositeCallbacks.queryRegistration = [&]() -> PrivateHttpResult {
        auto ready = ensureSerialReady();
        if (!ready.ok) return ready;

        std::uint16_t irXRaw = 0;
        std::uint16_t irYRaw = 0;
        std::uint16_t lowXRaw = 0;
        std::uint16_t lowYRaw = 0;
        {
            std::lock_guard<std::mutex> serialLock(serialMutex);
            auto irX = compositeController.readRegister(CompositeSensorRegister::InfraredRegistrationOffsetX);
            if (!irX) return errorJson("query_registration", irX.status().describe());
            auto irY = compositeController.readRegister(CompositeSensorRegister::InfraredRegistrationOffsetY);
            if (!irY) return errorJson("query_registration", irY.status().describe());
            auto lowX = compositeController.readRegister(CompositeSensorRegister::LowlightRegistrationOffsetX);
            if (!lowX) return errorJson("query_registration", lowX.status().describe());
            auto lowY = compositeController.readRegister(CompositeSensorRegister::LowlightRegistrationOffsetY);
            if (!lowY) return errorJson("query_registration", lowY.status().describe());
            irXRaw = irX.value(); irYRaw = irY.value(); lowXRaw = lowX.value(); lowYRaw = lowY.value();
        }

        std::ostringstream body;
        body << "{\"ok\":true,\"action\":\"query_registration\",\"registration\":{"
             << "\"infrared\":{\"x\":" << static_cast<std::int16_t>(irXRaw)
             << ",\"y\":" << static_cast<std::int16_t>(irYRaw) << "},"
             << "\"lowlight\":{\"x\":" << static_cast<std::int16_t>(lowXRaw)
             << ",\"y\":" << static_cast<std::int16_t>(lowYRaw) << "}}}";
        return jsonResult(body.str(), true);
    };

    compositeCallbacks.queryConfig = [&]() -> PrivateHttpResult {
        return readDescriptorList("query_config",
                                  tri::device_control::keyCompositeConfigRegisterDescriptors(),
                                  true);
    };

    compositeCallbacks.readAllRegisters = [&]() -> PrivateHttpResult {
        return readDescriptorList("read_all_registers",
                                  tri::device_control::allCompositeRegisterDescriptors(),
                                  false);
    };

    compositeCallbacks.currentCompositeStatusJson = [&]() -> std::string {
        std::lock_guard<std::mutex> statusLock(statusMutex);
        return cachedCompositeStatusJson(cachedComposite);
    };

    if (!server.start(controlCfg, switchCallback, compositeCallbacks)) {
        std::cerr << "[ERROR] failed to start private control server: " << server.lastError() << "\n";
        videoRuntime.stop();
        compositeController.shutdown();
        return 1;
    }
    std::cout << "[OK] private control server started on port " << controlCfg.port << ".\n";

    while (!gStopRequested.load()) {
        {
            std::lock_guard<std::mutex> lock(runtimeMutex);
            if (!videoRuntime.isRunning()) {
                std::cerr << "[ERROR] video runtime exited unexpectedly: "
                          << videoRuntime.lastError() << "\n";
                server.stop();
                compositeController.shutdown();
                return 1;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    std::cout << "\n[INFO] stopping private control server...\n";
    server.stop();
    std::cout << "[INFO] stopping video runtime...\n";
    videoRuntime.stop();
    if (serialEnabled) {
        std::cout << "[INFO] shutting down composite sensor controller...\n";
        compositeController.shutdown();
    }
    std::cout << "[OK] stopped.\n";
    return 0;
}
