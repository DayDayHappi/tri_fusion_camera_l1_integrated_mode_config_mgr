#include "command/CommandRouter.h"

#include "foundation/error/ErrorCode.h"

namespace tri::command {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> CommandRouter::registerHandler(std::unique_ptr<CommandHandler> handler) {
    if (!handler) {
        return Result<void>::error(ErrorCode::InvalidArgument, "command handler is null");
    }
    const auto type = handler->type();
    if (type == CommandType::Unknown) {
        return Result<void>::error(ErrorCode::InvalidArgument, "cannot register unknown command handler");
    }
    handlers_[type] = std::move(handler);
    return Result<void>::success();
}

Result<CommandResult> CommandRouter::route(const Command& command) {
    if (command.type == CommandType::Unknown) {
        return Result<CommandResult>::error(ErrorCode::InvalidCommand, "unknown command type");
    }

    auto it = handlers_.find(command.type);
    if (it == handlers_.end() || !it->second) {
        return Result<CommandResult>::error(ErrorCode::Unsupported,
                                            "no command handler registered: " + toString(command.type));
    }

    return it->second->handle(command);
}

bool CommandRouter::hasHandler(CommandType type) const {
    return handlers_.find(type) != handlers_.end();
}

} // namespace tri::command
