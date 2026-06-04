#include "command/handlers/SetWorkModeHandler.h"

#include "foundation/error/ErrorCode.h"
#include "mode/WorkMode.h"

namespace tri::command {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<CommandResult> SetWorkModeHandler::handle(const Command& command) {
    if (service_ == nullptr) {
        return Result<CommandResult>::error(ErrorCode::NotInitialized, "set-work-mode handler service is null");
    }

    auto modeText = command.param("mode");
    if (modeText.empty()) modeText = command.param("work_mode");
    if (modeText.empty()) {
        return Result<CommandResult>::error(ErrorCode::InvalidCommand, "SET_WORK_MODE requires param: mode");
    }

    const auto mode = tri::mode::workModeFromString(modeText);
    if (mode == tri::mode::WorkMode::Unknown) {
        return Result<CommandResult>::error(ErrorCode::UnsupportedWorkMode, "unsupported work mode: " + modeText);
    }

    auto ret = service_->setWorkMode(mode);
    if (!ret) return Result<CommandResult>::error(ret.status().code(), ret.status().describe());

    return Result<CommandResult>::ok(CommandResult::ok({
        {"work_mode", tri::mode::toString(mode)},
        {"command", "SET_WORK_MODE"},
    }));
}

} // namespace tri::command
