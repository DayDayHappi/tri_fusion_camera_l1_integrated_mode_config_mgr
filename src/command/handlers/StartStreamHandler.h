#pragma once

#include "command/CommandHandler.h"
#include "service/StreamService.h"

namespace tri::command {

class StartStreamHandler final : public CommandHandler {
public:
    explicit StartStreamHandler(tri::service::StreamService* service) : service_(service) {}
    CommandType type() const noexcept override { return CommandType::StartStream; }
    tri::foundation::Result<CommandResult> handle(const Command& command) override;

private:
    tri::service::StreamService* service_{nullptr};
};

} // namespace tri::command
