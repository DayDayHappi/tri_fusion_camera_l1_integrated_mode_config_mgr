#pragma once

#include "command/CommandHandler.h"
#include "service/ModeService.h"

namespace tri::command {

class GetWorkModeHandler final : public CommandHandler {
public:
    explicit GetWorkModeHandler(tri::service::ModeService* service) : service_(service) {}
    CommandType type() const noexcept override { return CommandType::GetWorkMode; }
    tri::foundation::Result<CommandResult> handle(const Command& command) override;

private:
    tri::service::ModeService* service_{nullptr};
};

} // namespace tri::command
