#include "service/StatusService.h"

#include "foundation/error/ErrorCode.h"

namespace tri::service {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> StatusService::init(ServiceRuntime runtime,
                                 ProtocolService* protocolService) {
    if (runtime.modeManager == nullptr) {
        return Result<void>::error(ErrorCode::InvalidArgument, "mode manager is null");
    }
    runtime_ = runtime;
    protocolService_ = protocolService;
    initialized_ = true;
    return Result<void>::success();
}

Result<SystemStatus> StatusService::querySystemStatus() const {
    if (!initialized_) {
        return Result<SystemStatus>::error(ErrorCode::NotInitialized, "status service is not initialized");
    }

    SystemStatus status;
    if (runtime_.modeManager) status.mode = runtime_.modeManager->state();
    if (runtime_.cameraManager) status.cameras = runtime_.cameraManager->queryStatus();
    if (runtime_.compositeController) status.compositeSensor = runtime_.compositeController->getState();
    if (runtime_.mainStream) {
        status.mainStreamState = runtime_.mainStream->state();
        status.mainStreamStats = runtime_.mainStream->stats();
    }
    if (protocolService_) status.protocol = protocolService_->status();

    return Result<SystemStatus>::ok(std::move(status));
}

Result<std::unordered_map<std::string, std::string>> StatusService::queryFlatStatus() const {
    auto status = querySystemStatus();
    if (!status) {
        return Result<std::unordered_map<std::string, std::string>>::error(status.status().code(),
                                                                           status.status().describe());
    }
    return Result<std::unordered_map<std::string, std::string>>::ok(flattenStatus(status.value()));
}

} // namespace tri::service
