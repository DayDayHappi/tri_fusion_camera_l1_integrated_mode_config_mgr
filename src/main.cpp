#include "algorithm/FusionEngine.h"

#include "device/CameraManager.h"
#include "device_control/CompositeSensorController.h"

#include "foundation/config/ConfigManager.h"
#include "foundation/error/Result.h"
#include "foundation/event/EventBus.h"
#include "foundation/log/Logger.h"

#include "media/stream/MainStream.h"

#include "mode/ModeManager.h"
#include "mode/ModeTypes.h"
#include "mode/WorkMode.h"

#include "protocol/udp/UdpMpegTsPublisher.h"

#include "service/StreamService.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

namespace {

std::atomic<bool> g_stop{false};

void onSignal(int) {
    g_stop = true;
}

bool checkVoid(const char* step, const tri::foundation::Result<void>& ret) {
    if (!ret) {
        std::cerr << "[FAIL] " << step << ": " << ret.status().describe() << "\n";
        return false;
    }

    std::cout << "[OK] " << step << "\n";
    return true;
}

void printUsage(const char* app) {
    std::cout
        << "Usage:\n"
        << "  sudo " << app << " [config_dir] [mode] [seconds] [udp_url]\n\n"
        << "Example:\n"
        << "  sudo " << app << " ./configs LOWLIGHT_THERMAL_COMPOSITE 0 "
        << "\"udp://192.168.1.35:5004?pkt_size=1316\"\n\n"
        << "mode:\n"
        << "  VISIBLE_ONLY\n"
        << "  LOWLIGHT_ONLY\n"
        << "  THERMAL_ONLY\n"
        << "  LOWLIGHT_THERMAL_COMPOSITE\n"
        << "  VISIBLE_LOWLIGHT_FUSION\n"
        << "  VISIBLE_THERMAL_FUSION\n"
        << "  VISIBLE_COMPOSITE_FUSION\n";
}

tri::protocol::udp::FfmpegPipeInputFormat pipeInputFormatFromCameraFormat(const std::string& format) {
    if (format == "yuyv" || format == "YUYV" ||
        format == "yuyv422" || format == "YUYV422") {
        return tri::protocol::udp::FfmpegPipeInputFormat::RawYuyv422;
    }

    return tri::protocol::udp::FfmpegPipeInputFormat::Mjpeg;
}

} // namespace

int main(int argc, char** argv) {
    const std::string configDir = argc >= 2 ? argv[1] : "./configs";
    const std::string modeText = argc >= 3 ? argv[2] : "LOWLIGHT_THERMAL_COMPOSITE";
    const int seconds = argc >= 4 ? std::stoi(argv[3]) : 0;
    const std::string udpUrl =
        argc >= 5 ? argv[4] : "udp://192.168.1.35:5004?pkt_size=1316";

    if (configDir == "-h" || configDir == "--help") {
        printUsage(argv[0]);
        return 0;
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    std::signal(SIGPIPE, SIG_IGN);

    const auto targetMode = tri::mode::workModeFromString(modeText);
    if (targetMode == tri::mode::WorkMode::Unknown) {
        std::cerr << "[FAIL] invalid work mode: " << modeText << "\n";
        printUsage(argv[0]);
        return 1;
    }

    tri::foundation::LoggerConfig logCfg;
    logCfg.minLevel = tri::foundation::LogLevel::Info;

    auto logRet = tri::foundation::Logger::instance().init(logCfg);
    if (!logRet) {
        std::cerr << "[FAIL] logger init: " << logRet.status().describe() << "\n";
        return 1;
    }

    auto& config = tri::foundation::ConfigManager::instance();

    auto cfgRet = config.loadAll(configDir);
    if (!cfgRet) {
        std::cerr << "[FAIL] load config: " << cfgRet.status().describe() << "\n";
        return 1;
    }

    std::cout << "[OK] config loaded from " << configDir << "\n";
    std::cout << "[INFO] mode=" << modeText << "\n";
    std::cout << "[INFO] udpUrl=" << udpUrl << "\n";

    tri::foundation::EventBus eventBus;
    if (!checkVoid("event bus init", eventBus.init(false, 1))) {
        return 1;
    }

    tri::device::CameraManager cameraManager;
    if (!checkVoid("camera manager init", cameraManager.init(config.camera(), &eventBus))) {
        eventBus.shutdown();
        return 1;
    }

    tri::device_control::CompositeSensorController compositeController;
    if (!checkVoid("composite sensor controller init",
                   compositeController.init(config.serial(), &eventBus))) {
        cameraManager.closeAll();
        eventBus.shutdown();
        return 1;
    }

    tri::media::MainStream mainStream;
    if (!checkVoid("main stream init", mainStream.init(config.media()))) {
        compositeController.shutdown();
        cameraManager.closeAll();
        eventBus.shutdown();
        return 1;
    }

    tri::algorithm::FusionEngine fusionEngine;

    tri::mode::ModeRuntimeContext modeRuntime;
    modeRuntime.cameraManager = &cameraManager;
    modeRuntime.compositeController = &compositeController;
    modeRuntime.mainStream = &mainStream;
    modeRuntime.eventBus = &eventBus;
    modeRuntime.mjpegDecoder = nullptr;
    modeRuntime.encoder = nullptr;
    modeRuntime.fusionEngine = &fusionEngine;

    tri::mode::ModeManager modeManager;
    if (!checkVoid("mode manager init",
                   modeManager.init(config.mode(), config.media(), modeRuntime))) {
        compositeController.shutdown();
        cameraManager.closeAll();
        eventBus.shutdown();
        return 1;
    }

    auto plan = modeManager.planFor(targetMode);
    if (!plan) {
        std::cerr << "[FAIL] build mode switch plan: " << plan.status().describe() << "\n";
        modeManager.stop();
        compositeController.shutdown();
        cameraManager.closeAll();
        eventBus.shutdown();
        return 1;
    }

    std::cout << "[PLAN]"
              << " targetMode=" << tri::mode::toString(plan.value().targetMode)
              << " requireVisible=" << (plan.value().requireVisible ? "true" : "false")
              << " requireComposite=" << (plan.value().requireComposite ? "true" : "false")
              << " requireCompositeSensorSwitch="
              << (plan.value().requireCompositeSensorSwitch ? "true" : "false")
              << " compositeOutput="
              << tri::device_control::toString(plan.value().compositeOutput)
              << " requireFusion=" << (plan.value().requireFusion ? "true" : "false")
              << "\n";

    tri::service::StreamService streamService;
    if (!checkVoid("stream service init", streamService.init(&mainStream))) {
        modeManager.stop();
        compositeController.shutdown();
        cameraManager.closeAll();
        eventBus.shutdown();
        return 1;
    }

    std::cout << "[SCENE] switch mode and start media pipeline\n";
    if (!checkVoid("mode manager switchTo", modeManager.switchTo(targetMode))) {
        modeManager.stop();
        compositeController.shutdown();
        cameraManager.closeAll();
        eventBus.shutdown();
        return 1;
    }

    const auto& endpoint =
        (plan.value().requireVisible && !plan.value().requireComposite)
            ? config.camera().visible
            : config.camera().compositeLowThermal;

    tri::protocol::udp::UdpMpegTsPublisherConfig pushConfig;
    pushConfig.udpUrl = udpUrl;
    pushConfig.width = static_cast<std::uint32_t>(endpoint.width);
    pushConfig.height = static_cast<std::uint32_t>(endpoint.height);
    pushConfig.fps = static_cast<std::uint32_t>(endpoint.fps);
    pushConfig.inputFormat = pipeInputFormatFromCameraFormat(endpoint.format);

    tri::protocol::udp::UdpMpegTsPublisher publisher;
    if (!checkVoid("udp mpegts publisher start",
                   publisher.start(pushConfig, &streamService))) {
        modeManager.stop();
        compositeController.shutdown();
        cameraManager.closeAll();
        eventBus.shutdown();
        return 1;
    }

    std::cout << "[READY] pushing H264 MPEG-TS UDP stream\n";
    std::cout << "[SEND]  " << udpUrl << "\n";
    std::cout << "[PLAY]  ffplay -fflags nobuffer -flags low_delay -framedrop "
                 "-sync ext -vf \"setpts=RTCTIME/1000000\" udp://@:5004\n";

    const auto begin = std::chrono::steady_clock::now();

    while (!g_stop) {
        if (seconds > 0) {
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - begin).count();

            if (elapsed >= seconds) {
                break;
            }
        }

        const auto msStats = mainStream.stats();
        const auto pubStats = publisher.stats();

        std::cout << "[RUNNING]"
                  << " mainStream.framesIn=" << msStats.framesIn
                  << " mainStream.dropped=" << msStats.framesDropped
                  << " publisher.framesWritten=" << pubStats.framesWritten
                  << " publisher.bytesWritten=" << pubStats.bytesWritten
                  << " publisher.writeErrors=" << pubStats.writeErrors
                  << "\n";

        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    publisher.stop();
    modeManager.stop();
    compositeController.shutdown();
    cameraManager.closeAll();
    eventBus.shutdown();

    std::cout << "[EXIT] stopped\n";
    return 0;
}
