#include "command/handlers/StartStreamHandler.h"

#include "foundation/error/ErrorCode.h"
#include "service/ServiceTypes.h"

namespace tri::command {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<CommandResult> StartStreamHandler::handle(const Command&) {
    if (service_ == nullptr) {
        return Result<CommandResult>::error(ErrorCode::NotInitialized, "start-stream handler service is null");
    }

    auto ret = service_->startMainStream();
    if (!ret) return Result<CommandResult>::error(ret.status().code(), ret.status().describe());

    auto state = service_->state();
    return Result<CommandResult>::ok(CommandResult::ok({
        {"stream", "main"},
        {"state", state ? tri::service::toString(state.value()) : "UNKNOWN"},
        {"command", "START_STREAM"},
    }));
}

} // namespace tri::command
