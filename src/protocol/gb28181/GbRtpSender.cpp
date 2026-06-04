#include "protocol/gb28181/GbRtpSender.h"
#include "foundation/error/ErrorCode.h"
namespace tri::protocol::gb28181 {
using tri::foundation::ErrorCode;
using tri::foundation::Result;
Result<void> GbRtpSender::init(tri::service::StreamService* streamService) { if (streamService == nullptr) return Result<void>::error(ErrorCode::InvalidArgument, "stream service is null"); streamService_ = streamService; return Result<void>::success(); }
Result<void> GbRtpSender::start(GbInviteContext context) { if (streamService_ == nullptr) return Result<void>::error(ErrorCode::NotInitialized, "GB RTP sender is not initialized"); if (context.remoteIp.empty() || context.remotePort == 0) return Result<void>::error(ErrorCode::InvalidArgument, "invalid GB RTP destination"); context_ = std::move(context); running_ = true; return Result<void>::success(); }
void GbRtpSender::stop() { running_ = false; }
Result<tri::media::EncodedFrame> GbRtpSender::waitFrame(int timeoutMs) { if (!running_) return Result<tri::media::EncodedFrame>::error(ErrorCode::MainStreamNotReady, "GB RTP sender is not running"); return streamService_->waitFrame(timeoutMs); }
} // namespace tri::protocol::gb28181
