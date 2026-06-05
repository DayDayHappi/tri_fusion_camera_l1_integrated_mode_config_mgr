#include "device_control/CompositeSensorController.h"
#include "device_control/CompositeSensorControlTypes.h"
#include "foundation/config/ConfigManager.h"
#include "foundation/error/ErrorCode.h"
#include "foundation/error/Result.h"
#include "media/gstreamer/GStreamerPipeline.h"
#include "media/gstreamer/GStreamerPipelineConfigManager.h"
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
#include <vector>

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
        << "  --host IP                 UDP video target host, default 192.168.1.153\n"
        << "  --port PORT               UDP video target port, default 5004\n"
        << "  --gst-launch PATH         gst-launch binary, default gst-launch-1.0\n"
        << "  --no-verbose              do not pass -v to gst-launch\n"
        << "  --no-eos                  do not pass -e to gst-launch\n\n"
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
        << "Deprecated compatibility options:\n"
        << "  --device PATH             same as --composite-device\n"
        << "  --width/--height/--fps/--format are ignored; values now come from configs/camera.yaml\n\n";
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
    } catch (...) {
        std::cerr << "[ERROR] invalid integer value: " << text << "\n";
        return false;
    }
    return true;
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


std::string fusionModeValueName(std::uint16_t value) {
    switch (value) {
        case 1: return "infrared";
        case 2: return "lowlight";
        case 3: return "fusion";
        default: return "unknown";
    }
}

std::string fusionColorValueName(std::uint16_t value) {
    switch (value) {
        case 1: return "black_white";
        case 2: return "forest";
        case 3: return "snow";
        case 4: return "ocean";
        case 5: return "city";
        case 6: return "desert";
        case 7: return "default";
        default: return "unknown";
    }
}

std::string contourModeValueName(std::uint16_t value) {
    switch (value) {
        case 0: return "off";
        case 1: return "red";
        case 2: return "green";
        case 3: return "blue";
        case 4: return "purple";
        default: return "unknown";
    }
}

std::string infraredPolarityValueName(std::uint16_t value) {
    switch (value) {
        case 0: return "white_hot";
        case 1: return "black_hot";
        default: return "unknown";
    }
}

std::string correctionValueName(std::uint16_t value) {
    if (value == 0) return "idle";
    if (value == 1) return "correction";
    return "unknown";
}

int signed16(std::uint16_t value) {
    return static_cast<int>(static_cast<std::int16_t>(value));
}

struct CompositeRegisterSpec {
    int index;
    const char* name;
    const char* displayName;
    tri::device_control::CompositeSensorRegister reg;
    bool signedValue;
    std::string (*valueNameFn)(std::uint16_t);
};

const std::vector<CompositeRegisterSpec>& compositeConfigRegisterSpecs() {
    using tri::device_control::CompositeSensorRegister;
    static const std::vector<CompositeRegisterSpec> specs = {
        {1,  "fusion_mode",                    "融合模式",        CompositeSensorRegister::FusionMode,                   false, fusionModeValueName},
        {2,  "fusion_color",                   "融合颜色",        CompositeSensorRegister::FusionColor,                  false, fusionColorValueName},
        {3,  "contour_mode",                   "轮廓模式",        CompositeSensorRegister::ContourMode,                  false, contourModeValueName},
        {4,  "infrared_polarity",              "红外极性",        CompositeSensorRegister::InfraredPolarity,             false, infraredPolarityValueName},
        {5,  "infrared_correction",            "红外校正",        CompositeSensorRegister::InfraredCorrection,           false, correctionValueName},
        {6,  "infrared_brightness",            "红外亮度",        CompositeSensorRegister::InfraredBrightness,           false, nullptr},
        {7,  "infrared_contrast",              "红外对比度",      CompositeSensorRegister::InfraredContrast,             false, nullptr},
        {8,  "lowlight_brightness",            "微光亮度",        CompositeSensorRegister::LowlightBrightness,           false, nullptr},
        {9,  "lowlight_contrast",              "微光对比度",      CompositeSensorRegister::LowlightContrast,             false, nullptr},
        {10, "infrared_registration_zoom",     "红外配准挡位",    CompositeSensorRegister::InfraredRegistrationZoom,     false, nullptr},
        {11, "infrared_registration_offset_x", "红外配准 x 偏移", CompositeSensorRegister::InfraredRegistrationOffsetX,  true,  nullptr},
        {12, "infrared_registration_offset_y", "红外配准 y 偏移", CompositeSensorRegister::InfraredRegistrationOffsetY,  true,  nullptr},
        {13, "lowlight_registration_zoom",     "微光配准挡位",    CompositeSensorRegister::LowlightRegistrationZoom,     false, nullptr},
        {14, "lowlight_registration_offset_x", "微光配准 x 偏移", CompositeSensorRegister::LowlightRegistrationOffsetX,  true,  nullptr},
        {15, "lowlight_registration_offset_y", "微光配准 y 偏移", CompositeSensorRegister::LowlightRegistrationOffsetY,  true,  nullptr},
    };
    return specs;
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
        } else if (arg == "--width" || arg == "--height" || arg == "--fps" || arg == "--format" || arg == "--io-mode" || arg == "--encoder") {
            // Compatibility: consume value but ignore. Per-mode source configuration now comes from ConfigManager.
            std::string ignored;
            if (!readValue(i, argc, argv, &ignored)) return false;
            std::cout << "[WARN] " << arg << " is ignored by this version; edit configs/camera.yaml instead.\n";
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
    using tri::device_control::ContourMode;
    using tri::device_control::FusionColor;
    using tri::device_control::InfraredPolarity;
    using tri::media::gstreamer::GStreamerConfigManagerOptions;
    using tri::media::gstreamer::GStreamerPipeline;
    using tri::media::gstreamer::GStreamerPipelineConfig;
    using tri::media::gstreamer::GStreamerPipelineConfigManager;
    using tri::protocol::private_api::PrivateCompositeControlKind;
    using tri::protocol::private_api::PrivateCompositeControlRequest;
    using tri::protocol::private_api::PrivateCompositeConfigSnapshot;
    using tri::protocol::private_api::PrivateCompositeRegisterValue;
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

    PrivateWorkMode currentMode = PrivateWorkMode::LowlightThermalComposite;
    GStreamerPipelineConfig initialVideoConfig = gstConfigMgr.configForModeCommand(toCommandName(currentMode));

    std::mutex pipelineMutex;
    GStreamerPipeline pipeline(initialVideoConfig);
    CompositeSensorController compositeController;

    std::cout << "\n========== Private GStreamer Control Probe ==========" << "\n";
    std::cout << "[CONTROL] port=" << controlCfg.port << "\n";
    std::cout << "[VIDEO] initial_mode=" << toString(currentMode)
              << " source=" << initialVideoConfig.sourceName
              << " device=" << initialVideoConfig.device
              << " format=" << initialVideoConfig.rawFormat
              << " codec=" << initialVideoConfig.inputCodec
              << " size=" << initialVideoConfig.width << "x" << initialVideoConfig.height
              << " fps=" << initialVideoConfig.fps
              << " udp=" << initialVideoConfig.udpHost << ":" << initialVideoConfig.udpPort << "\n";
    std::cout << "[VIDEO CMD] " << pipeline.commandLine() << "\n";
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
    } else {
        std::cout << "[WARN] serial control disabled, HTTP mode command will only switch video pipeline source.\n";
    }
    std::cerr << "[VIDEO][MAIN] initial pipeline start\n";
std::cerr << "[VIDEO][MAIN] initial mode=lowlight_thermal\n";
std::cerr << "[VIDEO][MAIN] command_line=" << pipeline.commandLine() << "\n";
    if (!pipeline.start()) {
        std::cerr << "[ERROR] failed to start GStreamer pipeline: " << pipeline.lastError() << "\n";
        compositeController.shutdown();
        return 1;
    }
    std::cout << "[OK] GStreamer video pipeline started.\n";

    PrivateControlServer server;

    auto switchCallback = [&](PrivateWorkMode targetMode) -> PrivateControlResult {
        std::cerr << "[VIDEO][MODE] switch request received\n";
        std::lock_guard<std::mutex> lock(pipelineMutex);
        PrivateControlResult result;
        result.currentMode = toString(currentMode);

        const std::string commandName = toCommandName(targetMode);
        const GStreamerPipelineConfig targetVideoConfig = gstConfigMgr.configForModeCommand(commandName);

        std::cout << "[CONTROL] mode switch requested: "
                  << toString(currentMode) << " -> " << toString(targetMode)
                  << " command=" << commandName
                  << " source=" << targetVideoConfig.sourceName
                  << " device=" << targetVideoConfig.device << "\n";

        CompositeSensorOutputMode compositeMode = CompositeSensorOutputMode::Unknown;
        bool needCompositeSwitch = false;
        if (!mapPrivateModeToCompositeOutput(targetMode, &compositeMode, &needCompositeSwitch)) {
            result.ok = false;
            result.message = "unsupported private work mode";
            return result;
        }

        if (!pipeline.stop()) {
            result.ok = false;
            result.message = "failed to stop gstreamer pipeline: " + pipeline.lastError();
            return result;
        }

        if (serialEnabled && needCompositeSwitch) {
            std::cout << "[SERIAL] set composite output mode: "
                      << tri::device_control::toString(compositeMode) << "\n";
            auto switchRet = compositeController.setOutputMode(compositeMode);
            if (!switchRet) {
                std::cerr << "[VIDEO][MODE] starting new pipeline after mode switch\n";
                pipeline.start();
                result.ok = false;
                result.message = "serial setOutputMode failed: " + switchRet.status().describe();
                return result;
            }
            auto stableRet = compositeController.waitStable(stableMs);
            if (!stableRet) {
                std::cerr << "[VIDEO][MODE] starting new pipeline after mode switch\n";
                pipeline.start();
                result.ok = false;
                result.message = "serial waitStable failed: " + stableRet.status().describe();
                return result;
            }
        }

        pipeline.setConfig(targetVideoConfig);
        currentMode = targetMode;

std::cerr << "[VIDEO][MODE] new pipeline config:"
          << " source=" << targetVideoConfig.sourceName
          << " device=" << targetVideoConfig.device
          << " input_codec=" << targetVideoConfig.inputCodec
          << " raw_format=" << targetVideoConfig.rawFormat
          << " size=" << targetVideoConfig.width << "x" << targetVideoConfig.height
          << " fps=" << targetVideoConfig.fps
          << " udp=" << targetVideoConfig.udpHost << ":" << targetVideoConfig.udpPort
          << "\n";

std::cerr << "[VIDEO][MODE] new command_line="
          << pipeline.commandLine()
          << "\n";

        std::cout << "[VIDEO] starting source=" << targetVideoConfig.sourceName
                  << " device=" << targetVideoConfig.device
                  << " codec=" << targetVideoConfig.inputCodec
                  << " format=" << targetVideoConfig.rawFormat
                  << " size=" << targetVideoConfig.width << "x" << targetVideoConfig.height
                  << " fps=" << targetVideoConfig.fps << "\n";
        std::cout << "[VIDEO CMD] " << pipeline.commandLine() << "\n";

        if (!pipeline.start()) {
            result.ok = false;
            result.message = "failed to restart gstreamer pipeline after mode switch: " + pipeline.lastError();
            return result;
        }

        std::ostringstream msg;
        msg << "mode accepted, source=" << targetVideoConfig.sourceName
            << ", device=" << targetVideoConfig.device
            << ", video pipeline restarted";
        result.ok = true;
        result.currentMode = toString(currentMode);
        result.message = msg.str();
        return result;
    };


    auto compositeControlCallback = [&](const PrivateCompositeControlRequest& request) -> PrivateControlResult {
        std::lock_guard<std::mutex> lock(pipelineMutex);
        PrivateControlResult result;
        result.currentMode = toString(currentMode);

        if (!serialEnabled) {
            result.ok = false;
            result.message = "serial control is disabled; composite parameter cannot be written";
            return result;
        }

        std::cout << "[CONTROL] composite parameter requested: "
                  << request.name << "=" << request.valueName
                  << " (" << request.value << ")\n";

        tri::foundation::Result<void> ret = tri::foundation::Result<void>::error(
            tri::foundation::ErrorCode::InvalidArgument, "unsupported composite control request");

        switch (request.kind) {
            case PrivateCompositeControlKind::FusionColor:
                ret = compositeController.setFusionColor(static_cast<FusionColor>(request.value));
                break;
            case PrivateCompositeControlKind::ContourMode:
                ret = compositeController.setContourMode(static_cast<ContourMode>(request.value));
                break;
            case PrivateCompositeControlKind::InfraredPolarity:
                ret = compositeController.setInfraredPolarity(static_cast<InfraredPolarity>(request.value));
                break;
            case PrivateCompositeControlKind::Unknown:
                break;
        }

        if (!ret) {
            result.ok = false;
            result.message = "serial write register failed: " + ret.status().describe();
            return result;
        }

        result.ok = true;
        result.message = "succeeded: composite parameter applied: " + request.name + "=" + request.valueName;
        return result;
    };

    auto compositeConfigQueryCallback = [&]() -> PrivateCompositeConfigSnapshot {
        std::lock_guard<std::mutex> lock(pipelineMutex);
        PrivateCompositeConfigSnapshot snapshot;

        if (!serialEnabled) {
            snapshot.ok = false;
            snapshot.message = "failed: serial control is disabled; composite config cannot be read";
            return snapshot;
        }

        for (const auto& spec : compositeConfigRegisterSpecs()) {
            const auto address = static_cast<std::uint16_t>(spec.reg);
            auto readRet = compositeController.readRegister(spec.reg);
            if (!readRet) {
                snapshot.ok = false;
                std::ostringstream msg;
                msg << "failed: read register " << spec.name
                    << " at 0x" << std::hex << std::uppercase << address << std::dec
                    << " failed: " << readRet.status().describe();
                snapshot.message = msg.str();
                return snapshot;
            }

            const std::uint16_t raw = readRet.value();
            PrivateCompositeRegisterValue item;
            item.index = spec.index;
            item.name = spec.name;
            item.displayName = spec.displayName;
            item.address = address;
            item.rawValue = raw;
            item.value = spec.signedValue ? signed16(raw) : static_cast<int>(raw);
            item.valueName = spec.valueNameFn ? spec.valueNameFn(raw) : std::string{};
            snapshot.registers.push_back(std::move(item));
        }

        snapshot.ok = true;
        snapshot.message = "succeeded: composite config read successfully";
        return snapshot;
    };

    if (!server.start(controlCfg, switchCallback, compositeControlCallback, compositeConfigQueryCallback)) {
        std::cerr << "[ERROR] failed to start private control server: " << server.lastError() << "\n";
        std::cerr << "[VIDEO][MODE] stopping old pipeline before mode switch\n";
        pipeline.stop();
        compositeController.shutdown();
        return 1;
    }
    std::cout << "[OK] private control server started on port " << controlCfg.port << ".\n";

    while (!gStopRequested.load()) {
        {
            std::lock_guard<std::mutex> lock(pipelineMutex);
            if (!pipeline.isRunning()) {
                std::cerr << "[ERROR] gst-launch exited unexpectedly.\n";
                server.stop();
                compositeController.shutdown();
                return 1;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    std::cout << "\n[INFO] stopping private control server...\n";
    server.stop();
    std::cout << "[INFO] stopping GStreamer pipeline...\n";
    pipeline.stop();
    if (serialEnabled) {
        std::cout << "[INFO] shutting down composite sensor controller...\n";
        compositeController.shutdown();
    }
    std::cout << "[OK] stopped.\n";
    return 0;
}
