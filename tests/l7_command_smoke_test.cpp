#include "command/Command.h"
#include "command/CommandBus.h"
#include "command/CommandRouter.h"
#include "command/DefaultCommandRegistry.h"
#include "foundation/config/ConfigManager.h"
#include "foundation/event/EventBus.h"
#include "foundation/log/Logger.h"
#include "mode/ModeManager.h"
#include "service/MediaService.h"
#include "service/ModeService.h"
#include "service/ProtocolService.h"
#include "service/StatusService.h"
#include "service/StreamService.h"

#include <iostream>

int main(int argc, char** argv) {
    const std::string configDir = argc > 1 ? argv[1] : "./configs";

    auto cfg = tri::foundation::ConfigManager::instance().loadAll(configDir);
    if (!cfg) {
        std::cerr << "config load failed: " << cfg.status().describe() << "\n";
        return 1;
    }

    tri::foundation::EventBus eventBus;
    auto ev = eventBus.init(false);
    if (!ev) {
        std::cerr << "event bus init failed: " << ev.status().describe() << "\n";
        return 1;
    }

    tri::media::MainStream mainStream;
    auto ms = mainStream.init(tri::foundation::ConfigManager::instance().media());
    if (!ms) {
        std::cerr << "main stream init failed: " << ms.status().describe() << "\n";
        return 1;
    }

    // L7 冒烟测试不打开硬件、不初始化 L6 ModeManager 的真实 runtime，
    // 只验证 command/service 层可以编译、注册和处理不依赖硬件的命令。
    tri::mode::ModeManager modeManager;

    tri::service::ModeService modeService;
    auto modeInit = modeService.init(&modeManager);
    if (!modeInit) {
        std::cerr << "mode service init failed: " << modeInit.status().describe() << "\n";
        return 1;
    }

    tri::service::StreamService streamService;
    auto streamInit = streamService.init(&mainStream);
    if (!streamInit) {
        std::cerr << "stream service init failed: " << streamInit.status().describe() << "\n";
        return 1;
    }

    tri::service::MediaService mediaService;
    auto mediaInit = mediaService.init(&modeManager);
    if (!mediaInit) {
        std::cerr << "media service init failed: " << mediaInit.status().describe() << "\n";
        return 1;
    }

    tri::service::ProtocolService protocolService;
    auto protoInit = protocolService.init(tri::foundation::ConfigManager::instance().protocol());
    if (!protoInit) {
        std::cerr << "protocol service init failed: " << protoInit.status().describe() << "\n";
        return 1;
    }

    tri::service::StatusService statusService;
    tri::service::ServiceRuntime runtime;
    runtime.modeManager = &modeManager;
    runtime.mainStream = &mainStream;
    auto statusInit = statusService.init(runtime, &protocolService);
    if (!statusInit) {
        std::cerr << "status service init failed: " << statusInit.status().describe() << "\n";
        return 1;
    }

    tri::command::CommandRouter router;
    auto reg = tri::command::registerDefaultCommandHandlers(router, {
        &modeService,
        &streamService,
        &mediaService,
        &statusService
    });
    if (!reg) {
        std::cerr << "register handlers failed: " << reg.status().describe() << "\n";
        return 1;
    }

    tri::command::CommandBus bus;
    auto busInit = bus.init(&router, &eventBus);
    if (!busInit) {
        std::cerr << "command bus init failed: " << busInit.status().describe() << "\n";
        return 1;
    }

    auto start = bus.submit(tri::command::makeCommand(
        tri::command::CommandType::StartStream,
        tri::command::CommandSource::LocalCli));
    if (!start || !start.value().accepted) {
        std::cerr << "START_STREAM failed: "
                  << (start ? start.value().status.describe() : start.status().describe()) << "\n";
        return 1;
    }

    auto query = bus.submit(tri::command::makeCommand(
        tri::command::CommandType::QueryStatus,
        tri::command::CommandSource::LocalCli));
    if (!query || !query.value().accepted) {
        std::cerr << "QUERY_STATUS failed: "
                  << (query ? query.value().status.describe() : query.status().describe()) << "\n";
        return 1;
    }

    std::cout << "L7 command/service smoke test passed\n";
    std::cout << "main_stream_state=" << query.value().fields["stream.main.state"] << "\n";
    return 0;
}
