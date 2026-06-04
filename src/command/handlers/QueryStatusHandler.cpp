#include "command/handlers/QueryStatusHandler.h"

#include "foundation/error/ErrorCode.h"

namespace tri::command {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<CommandResult> QueryStatusHandler::handle(const Command&) {
    if (service_ == nullptr) {
        return Result<CommandResult>::error(ErrorCode::NotInitialized, "query-status handler service is null");
    }

    auto flat = service_->queryFlatStatus();
    if (!flat) return Result<CommandResult>::error(flat.status().code(), flat.status().describe());

    auto result = CommandResult::ok(flat.value());
    result.fields["command"] = "QUERY_STATUS";
    return Result<CommandResult>::ok(std::move(result));
}

} // namespace tri::command
