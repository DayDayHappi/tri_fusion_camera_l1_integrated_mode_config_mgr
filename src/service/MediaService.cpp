#include "service/MediaService.h"

#include "foundation/error/ErrorCode.h"

namespace tri::service {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> MediaService::init(tri::mode::ModeManager* modeManager) {
    if (modeManager == nullptr) {
        return Result<void>::error(ErrorCode::InvalidArgument, "mode manager is null");
    }
    modeManager_ = modeManager;
    return Result<void>::success();
}

Result<void> MediaService::stopCurrentPipeline() {
    if (modeManager_ == nullptr) {
        return Result<void>::error(ErrorCode::NotInitialized, "media service is not initialized");
    }
    modeManager_->stop();
    return Result<void>::success();
}

} // namespace tri::service
