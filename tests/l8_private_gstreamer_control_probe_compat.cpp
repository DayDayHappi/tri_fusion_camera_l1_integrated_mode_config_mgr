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
#include <iostream>
#include <mutex>
#include <signal.h>
#include <sstream>
#include <string>
#include <thread>

namespace {

std::atomic_bool gStopRequested{false};

void onSignal(int) {
    gStopRequested.store(true);
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
        << "Modes:\n"
        << "  /api/v1/mode/visible\n"
        << "  /api/v1/mode/lowlight\n"
        << "  /api/v1/mode/thermal\n"
        << "  /api/v1/mode/lowlight_thermal\n"
        << "  /api/v1/mode/visible_lowlight\n"
        << "  /api/v1/mode/visible_thermal\n"
        << "  /api/v1/mode/visible_composite\n";
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

} // namespace

int main(int argc, char** argv) {
    using tri::device_control::CompositeSensorController;
    using tri::device_control::CompositeSensorOutputMode;
    using tri::media::gstreamer::GStreamerConfigManagerOptions;
    using tri::media::gstreamer::GStreamerPipelineConfigManager;
    using tri::mode::PrivateVideoRuntime;
    using tri::protocol::private_api::PrivateControlResult;
    using tri::protocol::private_api::PrivateControlServer;
    using tri::protocol::private_api::PrivateControlServerConfig;
    using tri::protocol::private_api::PrivateWorkMode;
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
            std::cout << "[SERIAL] set composite output mode: "
                      << tri::device_control::toString(compositeMode) << "\n";
            auto switchRet = compositeController.setOutputMode(compositeMode);
            if (!switchRet) {
                // Best effort: try to restore previous video runtime.
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

    // 当前工程的 PrivateControlServer 只有 start(config, ModeSwitchCallback) 这个接口，
    // 不要传 composite parameter callback，否则会和当前头文件不兼容。
    if (!server.start(controlCfg, switchCallback)) {
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
