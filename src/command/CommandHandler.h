#pragma once

#include "command/Command.h"
#include "command/CommandResult.h"
#include "foundation/error/Result.h"

namespace tri::command {

class CommandHandler {
public:
    virtual ~CommandHandler() = default;
    virtual CommandType type() const noexcept = 0;
    virtual tri::foundation::Result<CommandResult> handle(const Command& command) = 0;
};

} // namespace tri::command
