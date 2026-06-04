#include "algorithm/FusionEngine.h"

#include "device/CameraManager.h"
#include "device_control/CompositeSensorController.h"

#include "foundation/config/ConfigManager.h"
#include "foundation/error/Result.h"
#include "foundation/event/EventBus.h"
#include "foundation/log/Logger.h"
#include "hardware/mpp/MppEncoder.h"

#include "media/stream/MainStream.h"

#include "mode/ModeManager.h"
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

tri::mode::WorkMode parseMode(const std::string& text) {
    const auto mode = tri::mode::workModeFromString(text);
    if (mode == tri::mode::WorkMode::Unknown) {
        return tri::mode::WorkMode::LowlightThermalComposite;
    }
    return mode;
}

bool isYuyv422Format(const std::string& format) {
    return format == "yuyv" ||
           format == "YUYV" ||
           format == "yuyv422" ||
           format == "YUYV422";
}

} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    std::signal(SIGPIPE, SIG_IGN);

    const std::string configDir = argc >= 2 ? argv[1] : "./configs";
    const std::string modeText = argc >= 3 ? argv[2] : "LOWLIGHT_THERMAL_COMPOSITE";
    const int seconds = argc >= 4 ? std::stoi(argv[3]) : 0;

    // 板子 IP: 192.168.1.14
    // 主机 IP: 192.168.1.35
    // 这里填接收端主机 IP。
    const std::string udpUrl =
        argc >= 5 ? argv[4] : "udp://192.168.1.35:5004?pkt_size=1316";

    const auto targetMode = parseMode(modeText);

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

    const auto& compositeCfg = config.camera().compositeLowThermal;

    std::cout << "[CONFIG] configDir=" << configDir << "\n";
    std::cout << "[CONFIG] targetMode=" << tri::mode::toString(targetMode) << "\n";
    std::cout << "[CONFIG] composite.video_node=" << compositeCfg.videoNode << "\n";
    std::cout << "[CONFIG] composite.metadata_node=" << compositeCfg.metadataNode << "\n";
    std::cout << "[CONFIG] composite.format=" << compositeCfg.format << "\n";
    std::cout << "[CONFIG] composite.size=" << compositeCfg.width << "x" << compositeCfg.height << "\n";
    std::cout << "[CONFIG] composite.fps=" << compositeCfg.fps << "\n";
    std::cout << "[CONFIG] udpUrl=" << udpUrl << "\n";

    if (!isYuyv422Format(compositeCfg.format)) {
        std::cerr << "[FAIL] this probe expects composite camera format yuyv422, actual="
                  << compositeCfg.format << "\n";
        return 1;
    }

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

    tri::service::StreamService streamService;
    if (!checkVoid("stream service init", streamService.init(&mainStream))) {
        compositeController.shutdown();
        cameraManager.closeAll();
        eventBus.shutdown();
        return 1;
    }

    tri::algorithm::FusionEngine fusionEngine;

    tri::hardware::mpp::MppEncoder mppEncoder;

    tri::mode::ModeRuntimeContext modeRuntime;
    modeRuntime.cameraManager = &cameraManager;
    modeRuntime.compositeController = &compositeController;
    modeRuntime.mainStream = &mainStream;
    modeRuntime.eventBus = &eventBus;

    // 当前 UDP YUYV 测试不走工程内 MPP 解码/编码。
    // 数据路径：CaptureNode -> MediaPipeline -> MainStream -> UdpMpegTsPublisher -> ffmpeg
    modeRuntime.mjpegDecoder = nullptr;
    modeRuntime.encoder = &mppEncoder;
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
        std::cerr << "[FAIL] build mode plan: " << plan.status().describe() << "\n";
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

    if (!checkVoid("mode manager switchTo", modeManager.switchTo(targetMode))) {
        modeManager.stop();
        compositeController.shutdown();
        cameraManager.closeAll();
        eventBus.shutdown();
        return 1;
    }

    tri::protocol::udp::UdpMpegTsPublisherConfig udpCfg;
    udpCfg.udpUrl = udpUrl;
    udpCfg.inputFormat = tri::protocol::udp::FfmpegPipeInputFormat::RawYuyv422;
    udpCfg.width = static_cast<std::uint32_t>(compositeCfg.width);
    udpCfg.height = static_cast<std::uint32_t>(compositeCfg.height);
    udpCfg.fps = static_cast<std::uint32_t>(compositeCfg.fps);
    udpCfg.encoder = "libx264";
    udpCfg.preset = "ultrafast";
    udpCfg.tune = "zerolatency";
    udpCfg.ffmpegPath = "ffmpeg";
    udpCfg.logLevel = "warning";

    tri::protocol::udp::UdpMpegTsPublisher publisher;
    if (!checkVoid("udp mpegts publisher start",
                   publisher.start(udpCfg, &streamService))) {
        modeManager.stop();
        compositeController.shutdown();
        cameraManager.closeAll();
        eventBus.shutdown();
        return 1;
    }

    std::cout << "[READY] L0-L8 MainStream UDP YUYV422 probe is running\n";
    std::cout << "[CHAIN] CameraManager -> ModeManager -> MediaPipeline -> MainStream -> StreamService -> UdpMpegTsPublisher\n";
    std::cout << "[SEND]  " << udpUrl << "\n";
    std::cout << "[PLAY]  ffplay -fflags nobuffer -flags low_delay -framedrop -sync ext "
                 "-vf \"setpts=RTCTIME/1000000\" udp://@:5004\n";

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
                  << " publisher.framesPulled=" << pubStats.framesPulled
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

    const auto pubStats = publisher.stats();
    const auto msStats = mainStream.stats();

    std::cout << "[RESULT]"
              << " mainStream.framesIn=" << msStats.framesIn
              << " mainStream.dropped=" << msStats.framesDropped
              << " publisher.framesWritten=" << pubStats.framesWritten
              << " publisher.bytesWritten=" << pubStats.bytesWritten
              << " publisher.writeErrors=" << pubStats.writeErrors
              << "\n";

    std::cout << "[EXIT] stopped\n";
    return pubStats.framesWritten > 0 && pubStats.writeErrors == 0 ? 0 : 2;
}
