#pragma once

#include "command/CommandHandler.h"
#include "service/StatusService.h"

namespace tri::command {

class QueryStatusHandler final : public CommandHandler {
public:
    explicit QueryStatusHandler(tri::service::StatusService* service) : service_(service) {}
    CommandType type() const noexcept override { return CommandType::QueryStatus; }
    tri::foundation::Result<CommandResult> handle(const Command& command) override;

private:
    tri::service::StatusService* service_{nullptr};
};

} // namespace tri::command
