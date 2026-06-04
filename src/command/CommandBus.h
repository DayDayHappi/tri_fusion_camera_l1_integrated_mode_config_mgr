#pragma once

#include "command/CommandRouter.h"
#include "foundation/error/Result.h"
#include "foundation/event/EventBus.h"

#include <mutex>

namespace tri::command {

class CommandBus final {
public:
    CommandBus() = default;
    CommandBus(const CommandBus&) = delete;
    CommandBus& operator=(const CommandBus&) = delete;

    tri::foundation::Result<void> init(CommandRouter* router,
                                       tri::foundation::EventBus* eventBus = nullptr);
    tri::foundation::Result<CommandResult> submit(const Command& command);

    bool initialized() const noexcept { return initialized_; }

private:
    void publishProtocolError(const Command& command, const tri::foundation::Status& status);

    mutable std::mutex mutex_;
    CommandRouter* router_{nullptr};
    tri::foundation::EventBus* eventBus_{nullptr};
    bool initialized_{false};
};

} // namespace tri::command
