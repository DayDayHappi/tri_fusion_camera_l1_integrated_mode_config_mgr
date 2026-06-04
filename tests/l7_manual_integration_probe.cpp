#include "algorithm/FusionEngine.h"
#include "command/Command.h"
#include "command/CommandBus.h"
#include "command/CommandRouter.h"
#include "command/DefaultCommandRegistry.h"
#include "foundation/config/ConfigManager.h"
#include "foundation/error/Result.h"
#include "foundation/event/EventBus.h"
#include "foundation/log/Logger.h"
#include "device/CameraManager.h"
#include "device_control/CompositeSensorController.h"
#include "media/frame/EncodedFrame.h"
#include "media/stream/MainStream.h"
#include "mode/ModeManager.h"
#include "mode/ModeSwitchExecutor.h"
#include "mode/WorkMode.h"
#include "service/MediaService.h"
#include "service/ModeService.h"
#include "service/ProtocolService.h"
#include "service/StatusService.h"
#include "service/StreamService.h"

#include <chrono>
#include <iostream>
#include <string>

namespace {

void printUsage(const char* app) {
    std::cout
        << "Usage:\n"
        << "  " << app << " [config_dir] [source] [mode] [seconds]\n\n"
        << "Example:\n"
        << "  sudo " << app << " ./configs gb28181 LOWLIGHT_THERMAL_COMPOSITE 5\n"
        << "  sudo " << app << " ./configs gb28181 THERMAL_ONLY 5\n"
        << "  sudo " << app << " ./configs gb28181 VISIBLE_COMPOSITE_FUSION 5\n\n"
        << "source:\n"
        << "  gb28181 | onvif | private | local\n\n"
        << "mode:\n"
        << "  VISIBLE_ONLY\n"
        << "  LOWLIGHT_ONLY\n"
        << "  THERMAL_ONLY\n"
        << "  LOWLIGHT_THERMAL_COMPOSITE\n"
        << "  VISIBLE_LOWLIGHT_FUSION\n"
        << "  VISIBLE_THERMAL_FUSION\n"
        << "  VISIBLE_COMPOSITE_FUSION\n";
}

bool checkVoid(const char* step, const tri::foundation::Result<void>& ret) {
    if (!ret) {
        std::cerr << "[FAIL] " << step << ": " << ret.status().describe() << "\n";
        return false;
    }
    std::cout << "[OK] " << step << "\n";
    return true;
}

tri::command::CommandSource parseSource(const std::string& text) {
    if (text == "gb28181" || text == "gb") return tri::command::CommandSource::Gb28181;
    if (text == "onvif") return tri::command::CommandSource::Onvif;
    if (text == "private" || text == "private_api") return tri::command::CommandSource::PrivateApi;
    if (text == "local" || text == "cli") return tri::command::CommandSource::LocalCli;
    return tri::command::CommandSource::Unknown;
}

bool printCommandResult(const char* name, const tri::foundation::Result<tri::command::CommandResult>& ret) {
    if (!ret) {
        std::cerr << "[FAIL] " << name << ": " << ret.status().describe() << "\n";
        return false;
    }
    const auto& r = ret.value();
    if (!r.accepted) {
        std::cerr << "[REJECT] " << name << ": " << r.status.describe() << "\n";
        return false;
    }
    std::cout << "[OK] " << name << " accepted\n";
    for (const auto& kv : r.fields) {
        std::cout << "       " << kv.first << "=" << kv.second << "\n";
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    const std::string configDir = argc >= 2 ? argv[1] : "./configs";
    const std::string sourceText = argc >= 3 ? argv[2] : "gb28181";
    const std::string modeText = argc >= 4 ? argv[3] : "LOWLIGHT_THERMAL_COMPOSITE";
    const int seconds = argc >= 5 ? std::stoi(argv[4]) : 5;

    if (configDir == "-h" || configDir == "--help") {
        printUsage(argv[0]);
        return 0;
    }

    const auto source = parseSource(sourceText);
    if (source == tri::command::CommandSource::Unknown) {
        std::cerr << "[FAIL] invalid command source: " << sourceText << "\n";
        printUsage(argv[0]);
        return 1;
    }

    const auto targetMode = tri::mode::workModeFromString(modeText);
    if (targetMode == tri::mode::WorkMode::Unknown) {
        std::cerr << "[FAIL] invalid work mode: " << modeText << "\n";
        printUsage(argv[0]);
        return 1;
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
        std::cerr << "[FAIL] load config: " << cfgRet.status().describe() << "\n";
        return 1;
    }
    std::cout << "[OK] config loaded from " << configDir << "\n";

    tri::foundation::EventBus eventBus;
    if (!checkVoid("event bus init", eventBus.init(false, 1))) return 1;

    tri::device::CameraManager cameraManager;
    if (!checkVoid("camera manager init", cameraManager.init(config.camera(), &eventBus))) return 1;

    tri::device_control::CompositeSensorController compositeController;
    if (!checkVoid("composite sensor controller init", compositeController.init(config.serial(), &eventBus))) return 1;

    tri::media::MainStream mainStream;
    if (!checkVoid("main stream init", mainStream.init(config.media()))) return 1;

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
    if (!checkVoid("mode manager init", modeManager.init(config.mode(), config.media(), modeRuntime))) return 1;

    auto plan = modeManager.planFor(targetMode);
    if (!plan) {
        std::cerr << "[FAIL] plan target mode: " << plan.status().describe() << "\n";
        return 1;
    }
    std::cout << "[PLAN] mode=" << tri::mode::toString(plan.value().targetMode)
              << " compositeOutput=" << tri::device_control::toString(plan.value().compositeOutput)
              << " requireVisible=" << (plan.value().requireVisible ? "true" : "false")
              << " requireComposite=" << (plan.value().requireComposite ? "true" : "false")
              << "\n";

    tri::service::ModeService modeService;
    if (!checkVoid("mode service init", modeService.init(&modeManager))) return 1;

    tri::service::StreamService streamService;
    if (!checkVoid("stream service init", streamService.init(&mainStream))) return 1;

    tri::service::MediaService mediaService;
    if (!checkVoid("media service init", mediaService.init(&modeManager))) return 1;

    tri::service::ProtocolService protocolService;
    if (!checkVoid("protocol service init", protocolService.init(config.protocol()))) return 1;

    auto acquire = protocolService.acquire(source);
    if (!acquire) {
        std::cerr << "[FAIL] active protocol acquire rejected: " << acquire.status().describe() << "\n";
        std::cerr << "       当前 protocol.yaml active=" << config.protocol().active
                  << "，如果要模拟 private/onvif，请先改 protocol.active 或 allow_multi_online。\n";
        return 1;
    }
    std::cout << "[OK] active protocol acquired by " << sourceText << "\n";

    tri::service::StatusService statusService;
    tri::service::ServiceRuntime serviceRuntime;
    serviceRuntime.modeManager = &modeManager;
    serviceRuntime.cameraManager = &cameraManager;
    serviceRuntime.compositeController = &compositeController;
    serviceRuntime.mainStream = &mainStream;
    if (!checkVoid("status service init", statusService.init(serviceRuntime, &protocolService))) return 1;

    tri::command::CommandRouter router;
    auto reg = tri::command::registerDefaultCommandHandlers(router, {
        &modeService,
        &streamService,
        &mediaService,
        &statusService,
    });
    if (!checkVoid("register default command handlers", reg)) return 1;

    tri::command::CommandBus bus;
    if (!checkVoid("command bus init", bus.init(&router, &eventBus))) return 1;

    std::cout << "[SCENE] external protocol -> CommandBus -> SetWorkModeHandler -> ModeService -> ModeManager\n";
    auto setMode = bus.submit(tri::command::makeCommand(
        tri::command::CommandType::SetWorkMode,
        source,
        {{"mode", modeText}},
        sourceText));
    if (!printCommandResult("SET_WORK_MODE", setMode)) return 1;

    auto getMode = bus.submit(tri::command::makeCommand(
        tri::command::CommandType::GetWorkMode,
        source,
        {},
        sourceText));
    if (!printCommandResult("GET_WORK_MODE", getMode)) return 1;

    auto query1 = bus.submit(tri::command::makeCommand(
        tri::command::CommandType::QueryStatus,
        source,
        {},
        sourceText));
    if (!printCommandResult("QUERY_STATUS_AFTER_SWITCH", query1)) return 1;

    std::cout << "[WAIT] waiting MainStream frames for " << seconds << " seconds\n";
    std::uint64_t frames = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        auto frame = streamService.waitFrame(1000);
        if (!frame) {
            std::cout << "[WARN] wait frame timeout: " << frame.status().describe() << "\n";
            continue;
        }
        ++frames;
        std::cout << "[FRAME] seq=" << frame.value().sequence
                  << " bytes=" << frame.value().data.size()
                  << " ptsMs=" << frame.value().ptsMs
                  << "\n";
    }

    std::cout << "[RESULT] received_frames=" << frames
              << " mainStream.framesIn=" << mainStream.stats().framesIn
              << " dropped=" << mainStream.stats().framesDropped
              << "\n";

    auto stop = bus.submit(tri::command::makeCommand(
        tri::command::CommandType::StopStream,
        source,
        {},
        sourceText));
    if (!printCommandResult("STOP_STREAM", stop)) return 1;

    protocolService.release(source);
    modeManager.stop();
    compositeController.shutdown();
    cameraManager.closeAll();
    eventBus.shutdown();

    if (frames == 0) {
        std::cerr << "[FAIL] L7 command path worked, but no MainStream frame was received.\n"
                  << "       继续检查相机节点、串口、复合相机输出模式、L4 Pipeline。\n";
        return 2;
    }

    std::cout << "[PASS] L7 manual integration probe passed\n";
    return 0;
}
