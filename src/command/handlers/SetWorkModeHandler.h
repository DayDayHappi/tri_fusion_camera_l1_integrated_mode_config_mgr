#pragma once

#include "command/CommandHandler.h"
#include "service/ModeService.h"

namespace tri::command {

class SetWorkModeHandler final : public CommandHandler {
public:
    explicit SetWorkModeHandler(tri::service::ModeService* service) : service_(service) {}
    CommandType type() const noexcept override { return CommandType::SetWorkMode; }
    tri::foundation::Result<CommandResult> handle(const Command& command) override;

private:
    tri::service::ModeService* service_{nullptr};
};

} // namespace tri::command
