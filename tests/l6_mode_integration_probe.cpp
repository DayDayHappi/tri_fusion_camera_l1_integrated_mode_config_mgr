#include "algorithm/FusionEngine.h"
#include "device/CameraManager.h"
#include "device_control/CompositeSensorController.h"
#include "foundation/config/ConfigManager.h"
#include "foundation/event/EventBus.h"
#include "foundation/log/Logger.h"
#include "media/stream/MainStream.h"
#include "mode/ModeManager.h"
#include "mode/WorkMode.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace {

void printUsage(const char* app) {
    std::cout
        << "Usage:\n"
        << "  sudo " << app << " [config_dir] [mode] [seconds]\n\n"
        << "Examples:\n"
        << "  sudo " << app << " ./configs LOWLIGHT_ONLY 5\n"
        << "  sudo " << app << " ./configs THERMAL_ONLY 5\n"
        << "  sudo " << app << " ./configs LOWLIGHT_THERMAL_COMPOSITE 5\n"
        << "  sudo " << app << " ./configs VISIBLE_ONLY 5\n"
        << "  sudo " << app << " ./configs VISIBLE_THERMAL_FUSION 5\n\n"
        << "Modes:\n"
        << "  VISIBLE_ONLY\n"
        << "  LOWLIGHT_ONLY\n"
        << "  THERMAL_ONLY\n"
        << "  LOWLIGHT_THERMAL_COMPOSITE\n"
        << "  VISIBLE_LOWLIGHT_FUSION\n"
        << "  VISIBLE_THERMAL_FUSION\n"
        << "  VISIBLE_COMPOSITE_FUSION\n";
}

bool checkResult(const char* step, const tri::foundation::Result<void>& ret) {
    if (!ret) {
        std::cerr << "[FAIL] " << step << ": " << ret.status().describe() << "\n";
        return false;
    }
    std::cout << "[OK] " << step << "\n";
    return true;
}

} // namespace

int main(int argc, char** argv) {
    const std::string configDir = argc >= 2 ? argv[1] : "./configs";
    const std::string modeText = argc >= 3 ? argv[2] : "LOWLIGHT_THERMAL_COMPOSITE";
    const int seconds = argc >= 4 ? std::stoi(argv[3]) : 5;

    if (modeText == "-h" || modeText == "--help") {
        printUsage(argv[0]);
        return 0;
    }

    tri::foundation::LoggerConfig logCfg;
    logCfg.minLevel = tri::foundation::LogLevel::Debug;
    auto logRet = tri::foundation::Logger::instance().init(logCfg);
    if (!logRet) {
        std::cerr << "[FAIL] logger init: " << logRet.status().describe() << "\n";
        return 1;
    }

    auto& config = tri::foundation::ConfigManager::instance();
    auto cfgRet = config.loadAll(configDir);
    if (!cfgRet) {
        std::cerr << "[FAIL] load config from " << configDir
                  << ": " << cfgRet.status().describe() << "\n";
        return 1;
    }
    std::cout << "[OK] config loaded: " << configDir << "\n";

    const auto mode = tri::mode::workModeFromString(modeText);
    if (mode == tri::mode::WorkMode::Unknown) {
        std::cerr << "[FAIL] unsupported mode text: " << modeText << "\n";
        printUsage(argv[0]);
        return 1;
    }

    tri::foundation::EventBus eventBus;
    if (!checkResult("event bus init", eventBus.init(false, 1))) return 1;

    tri::device::CameraManager cameraManager;
    if (!checkResult("camera manager init",
                     cameraManager.init(config.camera(), &eventBus))) {
        return 1;
    }

    tri::device_control::CompositeSensorController compositeController;
    if (!checkResult("composite sensor controller init",
                     compositeController.init(config.serial(), &eventBus))) {
        return 1;
    }

    tri::media::MainStream mainStream;
    if (!checkResult("main stream init", mainStream.init(config.media()))) {
        return 1;
    }

    tri::algorithm::FusionEngine fusionEngine;

    tri::mode::ModeRuntimeContext runtime;
    runtime.cameraManager = &cameraManager;
    runtime.compositeController = &compositeController;
    runtime.mainStream = &mainStream;
    runtime.eventBus = &eventBus;

    /*
     * 当前 L4 第一版支持 MJPEG passthrough。
     * 这里先不绑定真实 MPP Decoder / Encoder：
     * - runtime.mjpegDecoder = nullptr 时，L4 使用 MjpegDecoderPassthrough；
     * - runtime.encoder = nullptr 时，L4 走 EncodedPassthrough；
     * 这样可以先验证 L6 -> L3/L4 的真实联调路径。
     */
    runtime.mjpegDecoder = nullptr;
    runtime.encoder = nullptr;
    runtime.fusionEngine = &fusionEngine;

    tri::mode::ModeManager modeManager;
    if (!checkResult("mode manager init",
                     modeManager.init(config.mode(), config.media(), runtime))) {
        return 1;
    }

    auto planRet = modeManager.planFor(mode);
    if (!planRet) {
        std::cerr << "[FAIL] build mode plan: " << planRet.status().describe() << "\n";
        return 1;
    }

    const auto& plan = planRet.value();
    std::cout << "[PLAN] mode=" << tri::mode::toString(plan.targetMode)
              << " requireVisible=" << (plan.requireVisible ? "true" : "false")
              << " requireComposite=" << (plan.requireComposite ? "true" : "false")
              << " requireCompositeSensorSwitch=" << (plan.requireCompositeSensorSwitch ? "true" : "false")
              << " compositeOutput=" << tri::device_control::toString(plan.compositeOutput)
              << " pipeline=" << tri::media::toString(plan.pipeline.type)
              << "\n";

    std::cout << "[RUN] switching mode to " << modeText << " ...\n";
    auto swRet = modeManager.switchTo(mode);
    if (!swRet) {
        std::cerr << "[FAIL] mode switch: " << swRet.status().describe() << "\n";
        std::cerr << "[STATE] lastError=" << modeManager.state().lastError << "\n";
        return 1;
    }

    std::cout << "[OK] mode switched, waiting frames for " << seconds << " seconds ...\n";

    std::uint64_t frameCount = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);

    while (std::chrono::steady_clock::now() < deadline) {
        auto frame = mainStream.waitFrame(1000);
        if (!frame) {
            std::cout << "[WARN] wait main stream frame timeout\n";
            continue;
        }

        ++frameCount;
        std::cout << "[FRAME] seq=" << frame->sequence
                  << " codec=" << static_cast<int>(frame->codec)
                  << " bytes=" << frame->data.size()
                  << " ptsMs=" << frame->ptsMs
                  << "\n";
    }

    std::cout << "[RESULT] frames=" << frameCount
              << " mainStream.framesIn=" << mainStream.stats().framesIn
              << " dropped=" << mainStream.stats().framesDropped
              << "\n";

    modeManager.stop();
    compositeController.shutdown();
    cameraManager.closeAll();
    eventBus.shutdown();

    if (frameCount == 0) {
        std::cerr << "[FAIL] no frame received from MainStream\n";
        return 2;
    }

    std::cout << "[PASS] L6 integration probe passed\n";
    return 0;
}
