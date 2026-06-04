#pragma once

#include "command/CommandRouter.h"
#include "service/MediaService.h"
#include "service/ModeService.h"
#include "service/StatusService.h"
#include "service/StreamService.h"

namespace tri::command {

struct DefaultCommandServices {
    tri::service::ModeService* modeService{nullptr};
    tri::service::StreamService* streamService{nullptr};
    tri::service::MediaService* mediaService{nullptr};
    tri::service::StatusService* statusService{nullptr};
};

tri::foundation::Result<void> registerDefaultCommandHandlers(CommandRouter& router,
                                                             DefaultCommandServices services);

} // namespace tri::command
