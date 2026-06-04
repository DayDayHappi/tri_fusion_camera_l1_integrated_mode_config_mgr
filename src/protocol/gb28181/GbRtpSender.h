#pragma once
#include "foundation/error/Result.h"
#include "media/frame/EncodedFrame.h"
#include "protocol/gb28181/Gb28181Types.h"
#include "service/StreamService.h"
namespace tri::protocol::gb28181 {
class GbRtpSender final {
public:
    tri::foundation::Result<void> init(tri::service::StreamService* streamService);
    tri::foundation::Result<void> start(GbInviteContext context);
    void stop();
    tri::foundation::Result<tri::media::EncodedFrame> waitFrame(int timeoutMs);
    bool running() const noexcept { return running_; }
    const GbInviteContext& context() const noexcept { return context_; }
private:
    tri::service::StreamService* streamService_{nullptr};
    GbInviteContext context_{};
    bool running_{false};
};
} // namespace tri::protocol::gb28181
