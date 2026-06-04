#include "command/DefaultCommandRegistry.h"

#include "command/handlers/GetWorkModeHandler.h"
#include "command/handlers/QueryStatusHandler.h"
#include "command/handlers/SetWorkModeHandler.h"
#include "command/handlers/StartStreamHandler.h"
#include "command/handlers/StopStreamHandler.h"
#include "foundation/error/ErrorCode.h"

namespace tri::command {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> registerDefaultCommandHandlers(CommandRouter& router,
                                            DefaultCommandServices services) {
    if (services.modeService == nullptr || services.streamService == nullptr ||
        services.statusService == nullptr) {
        return Result<void>::error(ErrorCode::InvalidArgument,
                                   "default command services require mode, stream and status services");
    }

    auto r1 = router.registerHandler(std::make_unique<SetWorkModeHandler>(services.modeService));
    if (!r1) return r1;
    auto r2 = router.registerHandler(std::make_unique<GetWorkModeHandler>(services.modeService));
    if (!r2) return r2;
    auto r3 = router.registerHandler(std::make_unique<StartStreamHandler>(services.streamService));
    if (!r3) return r3;
    auto r4 = router.registerHandler(std::make_unique<StopStreamHandler>(services.streamService,
                                                                         services.mediaService));
    if (!r4) return r4;
    auto r5 = router.registerHandler(std::make_unique<QueryStatusHandler>(services.statusService));
    if (!r5) return r5;

    return Result<void>::success();
}

} // namespace tri::command
