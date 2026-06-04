#include "command/Command.h"
#include "command/CommandBus.h"
#include "command/CommandRouter.h"
#include "command/DefaultCommandRegistry.h"
#include "foundation/config/ConfigTypes.h"
#include "foundation/event/EventBus.h"
#include "media/stream/MainStream.h"
#include "mode/ModeManager.h"
#include "protocol/ActiveProtocolGuard.h"
#include "protocol/private/PrivateApiService.h"
#include "service/MediaService.h"
#include "service/ModeService.h"
#include "service/ProtocolService.h"
#include "service/StatusService.h"
#include "service/StreamService.h"

#include <iostream>

int main() {
    tri::foundation::EventBus eventBus;
    auto evInit = eventBus.init(false);
    if (!evInit) {
        std::cerr << "event bus init failed: " << evInit.status().describe() << "\n";
        return 1;
    }

    tri::media::MainStream mainStream;
    tri::foundation::MediaConfig mediaConfig;
    auto msInit = mainStream.init(mediaConfig);
    if (!msInit) {
        std::cerr << "main stream init failed: " << msInit.status().describe() << "\n";
        return 1;
    }

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

    tri::foundation::ProtocolConfig protocolConfig;
    protocolConfig.active = "private";
    protocolConfig.allowMultiOnline = false;
    protocolConfig.reserveMultiOnlineArchitecture = true;
    protocolConfig.priority = {"private", "gb28181", "onvif"};

    tri::service::ProtocolService protocolService;
    auto protoInit = protocolService.init(protocolConfig);
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
        std::cerr << "register command handlers failed: " << reg.status().describe() << "\n";
        return 1;
    }

    tri::command::CommandBus bus;
    auto busInit = bus.init(&router, &eventBus);
    if (!busInit) {
        std::cerr << "command bus init failed: " << busInit.status().describe() << "\n";
        return 1;
    }

    tri::protocol::ActiveProtocolGuard guard;
    auto guardInit = guard.init(&protocolService);
    if (!guardInit) {
        std::cerr << "guard init failed: " << guardInit.status().describe() << "\n";
        return 1;
    }

    tri::protocol::private_api::PrivateApiService api;
    auto apiInit = api.init(&bus, &guard);
    if (!apiInit) {
        std::cerr << "private API init failed: " << apiInit.status().describe() << "\n";
        return 1;
    }

    auto start = api.handleLine("POST /api/v1/stream/start");
    if (!start || start.value().code != 200) {
        std::cerr << "private start failed: " << (start ? start.value().message : start.status().describe()) << "\n";
        return 1;
    }

    auto status = api.handleLine("GET /api/v1/status");
    if (!status || status.value().code != 200) {
        std::cerr << "private status failed: " << (status ? status.value().message : status.status().describe()) << "\n";
        return 1;
    }

    auto stop = api.handleLine("POST /api/v1/stream/stop");
    if (!stop || stop.value().code != 200) {
        std::cerr << "private stop failed: " << (stop ? stop.value().message : stop.status().describe()) << "\n";
        return 1;
    }

    std::cout << "L8 protocol smoke test passed\n";
    return 0;
}
