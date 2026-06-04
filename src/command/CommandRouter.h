#pragma once

#include "command/CommandHandler.h"
#include "foundation/error/Result.h"

#include <memory>
#include <unordered_map>

namespace tri::command {

class CommandRouter final {
public:
    CommandRouter() = default;
    CommandRouter(const CommandRouter&) = delete;
    CommandRouter& operator=(const CommandRouter&) = delete;

    tri::foundation::Result<void> registerHandler(std::unique_ptr<CommandHandler> handler);
    tri::foundation::Result<CommandResult> route(const Command& command);

    bool hasHandler(CommandType type) const;
    std::size_t handlerCount() const noexcept { return handlers_.size(); }

private:
    struct EnumHash {
        std::size_t operator()(CommandType type) const noexcept {
            return static_cast<std::size_t>(type);
        }
    };

    std::unordered_map<CommandType, std::unique_ptr<CommandHandler>, EnumHash> handlers_;
};

} // namespace tri::command
