#include "command/handlers/StopStreamHandler.h"

#include "foundation/error/ErrorCode.h"
#include "service/ServiceTypes.h"

namespace tri::command {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<CommandResult> StopStreamHandler::handle(const Command&) {
    if (streamService_ == nullptr) {
        return Result<CommandResult>::error(ErrorCode::NotInitialized, "stop-stream handler stream service is null");
    }

    if (mediaService_ != nullptr) {
        auto stopPipeline = mediaService_->stopCurrentPipeline();
        if (!stopPipeline) {
            return Result<CommandResult>::error(stopPipeline.status().code(), stopPipeline.status().describe());
        }
    }

    auto ret = streamService_->stopMainStream();
    if (!ret) return Result<CommandResult>::error(ret.status().code(), ret.status().describe());

    auto state = streamService_->state();
    return Result<CommandResult>::ok(CommandResult::ok({
        {"stream", "main"},
        {"state", state ? tri::service::toString(state.value()) : "UNKNOWN"},
        {"command", "STOP_STREAM"},
    }));
}

} // namespace tri::command
