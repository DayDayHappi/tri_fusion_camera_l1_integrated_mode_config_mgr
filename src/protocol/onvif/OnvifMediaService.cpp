#include "protocol/onvif/OnvifMediaService.h"
#include "foundation/error/ErrorCode.h"
namespace tri::protocol::onvif {
using tri::foundation::ErrorCode;
using tri::foundation::Result;
Result<void> OnvifMediaService::init(const tri::foundation::MediaConfig& mediaConfig, std::string host) {
    if (host.empty()) return Result<void>::error(ErrorCode::InvalidArgument, "ONVIF host is empty");
    mediaConfig_ = mediaConfig;
    host_ = std::move(host);
    initialized_ = true;
    return Result<void>::success();
}
Result<OnvifStreamUri> OnvifMediaService::getStreamUri() const {
    if (!initialized_) return Result<OnvifStreamUri>::error(ErrorCode::NotInitialized, "ONVIF media service is not initialized");
    OnvifStreamUri out;
    out.uri = "rtsp://" + host_ + ":" + std::to_string(mediaConfig_.rtspPort) + mediaConfig_.rtspPath;
    return Result<OnvifStreamUri>::ok(std::move(out));
}
} // namespace tri::protocol::onvif
