#include "command/CommandBus.h"

#include "foundation/error/ErrorCode.h"
#include "foundation/event/Event.h"
#include "foundation/log/Logger.h"

namespace tri::command {
using tri::foundation::ErrorCode;
using tri::foundation::Event;
using tri::foundation::EventType;
using tri::foundation::LogCategory;
using tri::foundation::Result;

Result<void> CommandBus::init(CommandRouter* router, tri::foundation::EventBus* eventBus) {
    if (router == nullptr) {
        return Result<void>::error(ErrorCode::InvalidArgument, "command router is null");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    router_ = router;
    eventBus_ = eventBus;
    initialized_ = true;
    return Result<void>::success();
}

Result<CommandResult> CommandBus::submit(const Command& command) {
    CommandRouter* router = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_ || router_ == nullptr) {
            return Result<CommandResult>::error(ErrorCode::NotInitialized, "command bus is not initialized");
        }
        router = router_;
    }

    TRI_LOG_INFO(LogCategory::Command) << "command submit id=" << command.id
                                       << " type=" << toString(command.type)
                                       << " source=" << toString(command.source);

    auto ret = router->route(command);
    if (!ret) {
        publishProtocolError(command, ret.status());
        return ret;
    }
    if (!ret.value().status.ok()) {
        publishProtocolError(command, ret.value().status);
    }
    return ret;
}

void CommandBus::publishProtocolError(const Command& command, const tri::foundation::Status& status) {
    if (eventBus_ == nullptr || status.ok()) return;

    Event ev;
    ev.type = EventType::ProtocolError;
    ev.name = "command_error";
    ev.fields["command_id"] = std::to_string(command.id);
    ev.fields["command_type"] = toString(command.type);
    ev.fields["source"] = toString(command.source);
    ev.fields["error"] = status.describe();
    eventBus_->publish(ev);
}

} // namespace tri::command
