#pragma once

#include "command/CommandHandler.h"
#include "service/MediaService.h"
#include "service/StreamService.h"

namespace tri::command {

class StopStreamHandler final : public CommandHandler {
public:
    StopStreamHandler(tri::service::StreamService* streamService,
                      tri::service::MediaService* mediaService = nullptr)
        : streamService_(streamService), mediaService_(mediaService) {}

    CommandType type() const noexcept override { return CommandType::StopStream; }
    tri::foundation::Result<CommandResult> handle(const Command& command) override;

private:
    tri::service::StreamService* streamService_{nullptr};
    tri::service::MediaService* mediaService_{nullptr};
};

} // namespace tri::command
