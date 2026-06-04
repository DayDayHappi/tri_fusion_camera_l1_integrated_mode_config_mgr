#include "algorithm/FusionEngine.h"

#include "command/Command.h"
#include "command/CommandBus.h"
#include "command/CommandRouter.h"
#include "command/DefaultCommandRegistry.h"

#include "device/CameraManager.h"
#include "device_control/CompositeSensorController.h"

#include "foundation/config/ConfigManager.h"
#include "foundation/error/Result.h"
#include "foundation/event/EventBus.h"
#include "foundation/log/Logger.h"

#include "media/frame/EncodedFrame.h"
#include "media/stream/MainStream.h"

#include "mode/ModeManager.h"
#include "mode/ModeTypes.h"
#include "mode/WorkMode.h"

#include "protocol/ProtocolManager.h"
#include "protocol/ProtocolTypes.h"

#include "service/MediaService.h"
#include "service/ModeService.h"
#include "service/ProtocolService.h"
#include "service/StatusService.h"
#include "service/StreamService.h"

#include <chrono>
#include <iostream>
#include <string>
#include <unordered_map>

namespace {

void printUsage(const char* app) {
    std::cout
        << "Usage:\n"
        << "  sudo " << app << " [config_dir] [protocol] [mode] [seconds]\n\n"
        << "Example:\n"
        << "  sudo " << app << " ./configs gb28181 LOWLIGHT_THERMAL_COMPOSITE 5\n"
        << "  sudo " << app << " ./configs gb28181 THERMAL_ONLY 5\n"
        << "  sudo " << app << " ./configs gb28181 LOWLIGHT_ONLY 5\n"
        << "  sudo " << app << " ./configs gb28181 VISIBLE_ONLY 5\n"
        << "  sudo " << app << " ./configs gb28181 VISIBLE_COMPOSITE_FUSION 5\n\n"
        << "protocol:\n"
        << "  gb28181 | onvif | private | rtsp\n\n"
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

bool checkProtocolResponse(const char* step,
                           const tri::foundation::Result<tri::protocol::ProtocolResponse>& ret) {
    if (!ret) {
        std::cerr << "[FAIL] " << step << ": " << ret.status().describe() << "\n";
        return false;
    }

    const auto& response = ret.value();
    if (response.code < 200 || response.code >= 300) {
        std::cerr << "[REJECT] " << step
                  << ": code=" << response.code
                  << " message=" << response.message << "\n";
        for (const auto& kv : response.fields) {
            std::cerr << "         " << kv.first << "=" << kv.second << "\n";
        }
        return false;
    }

    std::cout << "[OK] " << step
              << " code=" << response.code
              << " message=" << response.message << "\n";

    for (const auto& kv : response.fields) {
        std::cout << "       " << kv.first << "=" << kv.second << "\n";
    }

    return true;
}

tri::protocol::ProtocolKind parseProtocolKind(const std::string& text) {
    auto kind = tri::protocol::protocolKindFromString(text);
    if (kind != tri::protocol::ProtocolKind::Unknown) {
        return kind;
    }

    if (text == "gb" || text == "GB") {
        return tri::protocol::ProtocolKind::Gb28181;
    }

    if (text == "private_api" || text == "private-api") {
        return tri::protocol::ProtocolKind::PrivateApi;
    }

    return tri::protocol::ProtocolKind::Unknown;
}

void printStatusMap(const std::string& title,
                    const std::unordered_map<std::string, std::string>& fields) {
    std::cout << "[" << title << "]\n";
    for (const auto& kv : fields) {
        std::cout << "  " << kv.first << "=" << kv.second << "\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    const std::string configDir = argc >= 2 ? argv[1] : "./configs";
    const std::string protocolText = argc >= 3 ? argv[2] : "gb28181";
    const std::string modeText = argc >= 4 ? argv[3] : "LOWLIGHT_THERMAL_COMPOSITE";
    const int seconds = argc >= 5 ? std::stoi(argv[4]) : 5;

    if (configDir == "-h" || configDir == "--help") {
        printUsage(argv[0]);
        return 0;
    }

    const auto protocolKind = parseProtocolKind(protocolText);
    if (protocolKind == tri::protocol::ProtocolKind::Unknown) {
        std::cerr << "[FAIL] invalid protocol kind: " << protocolText << "\n";
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
    std::cout << "[INFO] protocol.active=" << config.protocol().active << "\n";
    std::cout << "[INFO] requested_protocol=" << tri::protocol::toString(protocolKind) << "\n";
    std::cout << "[INFO] requested_mode=" << modeText << "\n";

    tri::foundation::EventBus eventBus;
    if (!checkVoid("event bus init", eventBus.init(false, 1))) {
        return 1;
    }

    tri::device::CameraManager cameraManager;
    if (!checkVoid("camera manager init", cameraManager.init(config.camera(), &eventBus))) {
        return 1;
    }

    tri::device_control::CompositeSensorController compositeController;
    if (!checkVoid("composite sensor controller init",
                   compositeController.init(config.serial(), &eventBus))) {
        std::cerr
            << "[HINT] 如果这里失败，优先检查 configs/serial.yaml 里的 dev 是否正确，"
            << "例如 /dev/ttyACM0、/dev/ttyACM1 或 /dev/ttyS3。\n";
        return 1;
    }

    tri::media::MainStream mainStream;
    if (!checkVoid("main stream init", mainStream.init(config.media()))) {
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
        return 1;
    }

    auto plan = modeManager.planFor(targetMode);
    if (!plan) {
        std::cerr << "[FAIL] build mode switch plan: " << plan.status().describe() << "\n";
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

    tri::service::ModeService modeService;
    if (!checkVoid("mode service init", modeService.init(&modeManager))) {
        return 1;
    }

    tri::service::StreamService streamService;
    if (!checkVoid("stream service init", streamService.init(&mainStream))) {
        return 1;
    }

    tri::service::MediaService mediaService;
    if (!checkVoid("media service init", mediaService.init(&modeManager))) {
        return 1;
    }

    tri::service::ProtocolService protocolService;
    if (!checkVoid("protocol service init", protocolService.init(config.protocol()))) {
        return 1;
    }

    tri::service::StatusService statusService;
    tri::service::ServiceRuntime serviceRuntime;
    serviceRuntime.modeManager = &modeManager;
    serviceRuntime.cameraManager = &cameraManager;
    serviceRuntime.compositeController = &compositeController;
    serviceRuntime.mainStream = &mainStream;

    if (!checkVoid("status service init",
                   statusService.init(serviceRuntime, &protocolService))) {
        return 1;
    }

    tri::command::CommandRouter router;
    auto reg = tri::command::registerDefaultCommandHandlers(router, {
        &modeService,
        &streamService,
        &mediaService,
        &statusService,
    });
    if (!checkVoid("register default command handlers", reg)) {
        return 1;
    }

    tri::command::CommandBus commandBus;
    if (!checkVoid("command bus init", commandBus.init(&router, &eventBus))) {
        return 1;
    }

    tri::protocol::ProtocolRuntimeContext protocolRuntime;
    protocolRuntime.commandBus = &commandBus;
    protocolRuntime.protocolService = &protocolService;
    protocolRuntime.streamService = &streamService;

    tri::protocol::ProtocolManager protocolManager;
    if (!checkVoid("L8 protocol manager init",
                   protocolManager.init(config.protocol(), protocolRuntime))) {
        return 1;
    }

    std::cout << "[SCENE] L8 ProtocolManager start active protocol\n";
    auto startProtocol = protocolManager.startActiveProtocol();
    if (!startProtocol) {
        std::cerr << "[FAIL] start active protocol: "
                  << startProtocol.status().describe() << "\n";
        std::cerr << "[HINT] 当前 protocol.yaml active=" << config.protocol().active
                  << "，如果你传入的 protocol 不是它，就会被拒绝。\n";
        return 1;
    }
    std::cout << "[OK] active protocol started\n";

    std::cout
        << "[SCENE] L8 protocol submit SET_WORK_MODE -> L7 CommandBus -> L6 ModeManager -> real sensor/camera\n";

    auto setMode = protocolManager.submit(
        protocolKind,
        tri::command::CommandType::SetWorkMode,
        {{"mode", modeText}}
    );
    if (!checkProtocolResponse("L8 SET_WORK_MODE", setMode)) {
        return 1;
    }

    auto getMode = protocolManager.submit(
        protocolKind,
        tri::command::CommandType::GetWorkMode
    );
    if (!checkProtocolResponse("L8 GET_WORK_MODE", getMode)) {
        return 1;
    }

    auto queryAfterMode = protocolManager.submit(
        protocolKind,
        tri::command::CommandType::QueryStatus
    );
    if (!checkProtocolResponse("L8 QUERY_STATUS_AFTER_MODE", queryAfterMode)) {
        return 1;
    }
    printStatusMap("STATUS_AFTER_MODE", queryAfterMode.value().fields);

    std::cout
        << "[SCENE] L8 protocol submit START_STREAM -> StreamService/MainStream\n";

    auto startStream = protocolManager.submit(
        protocolKind,
        tri::command::CommandType::StartStream
    );
    if (!checkProtocolResponse("L8 START_STREAM", startStream)) {
        return 1;
    }

    std::cout << "[WAIT] waiting encoded MainStream frames for "
              << seconds << " seconds\n";

    std::uint64_t frames = 0;
    std::uint64_t bytes = 0;

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(seconds);

    while (std::chrono::steady_clock::now() < deadline) {
        auto frame = streamService.waitFrame(1000);
        if (!frame) {
            std::cout << "[WARN] wait frame timeout: "
                      << frame.status().describe() << "\n";
            continue;
        }

        ++frames;
        bytes += frame.value().data.size();

        std::cout << "[FRAME]"
                  << " seq=" << frame.value().sequence
                  << " bytes=" << frame.value().data.size()
                  << " ptsMs=" << frame.value().ptsMs
                  << " key=" << (frame.value().keyFrame ? "true" : "false")
                  << "\n";
    }

    std::cout << "[RESULT]"
              << " received_frames=" << frames
              << " received_bytes=" << bytes
              << " mainStream.framesIn=" << mainStream.stats().framesIn
              << " mainStream.framesDropped=" << mainStream.stats().framesDropped
              << "\n";

    auto queryAfterFrames = protocolManager.submit(
        protocolKind,
        tri::command::CommandType::QueryStatus
    );
    if (!checkProtocolResponse("L8 QUERY_STATUS_AFTER_FRAMES", queryAfterFrames)) {
        return 1;
    }
    printStatusMap("STATUS_AFTER_FRAMES", queryAfterFrames.value().fields);

    std::cout << "[SCENE] L8 protocol submit STOP_STREAM\n";
    auto stopStream = protocolManager.submit(
        protocolKind,
        tri::command::CommandType::StopStream
    );
    if (!checkProtocolResponse("L8 STOP_STREAM", stopStream)) {
        return 1;
    }

    protocolManager.stopAll();
    modeManager.stop();
    compositeController.shutdown();
    cameraManager.closeAll();
    eventBus.shutdown();

    if (frames == 0) {
        std::cerr
            << "[FAIL] L8 -> L7 -> L6 控制链路已经执行，但没有收到 MainStream 编码帧。\n"
            << "[CHECK] 继续检查：\n"
            << "  1. configs/camera.yaml 里的 video_node 是否对应真实相机；\n"
            << "  2. configs/serial.yaml 里的串口 dev 是否正确；\n"
            << "  3. 当前模式是否需要可见光相机，而可见光节点是否存在；\n"
            << "  4. L4 PipelineBuilder 当前模式是否真正 push EncodedFrame 到 MainStream；\n"
            << "  5. MPP 编码器或 MJPEG 解码器是否在当前板子环境可用。\n";
        return 2;
    }

    std::cout << "[PASS] L8 manual integration probe passed. Real camera path is working.\n";
    return 0;
}
