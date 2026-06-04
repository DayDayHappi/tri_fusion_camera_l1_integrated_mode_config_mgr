#include "command/handlers/GetWorkModeHandler.h"

#include "foundation/error/ErrorCode.h"
#include "mode/WorkMode.h"

namespace tri::command {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<CommandResult> GetWorkModeHandler::handle(const Command&) {
    if (service_ == nullptr) {
        return Result<CommandResult>::error(ErrorCode::NotInitialized, "get-work-mode handler service is null");
    }

    auto mode = service_->getWorkMode();
    if (!mode) return Result<CommandResult>::error(mode.status().code(), mode.status().describe());

    return Result<CommandResult>::ok(CommandResult::ok({
        {"work_mode", tri::mode::toString(mode.value())},
        {"command", "GET_WORK_MODE"},
    }));
}

} // namespace tri::command
