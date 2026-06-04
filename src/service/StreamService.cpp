#include "service/StreamService.h"

#include "foundation/error/ErrorCode.h"

namespace tri::service {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> StreamService::init(tri::media::MainStream* mainStream) {
    if (mainStream == nullptr) {
        return Result<void>::error(ErrorCode::InvalidArgument, "main stream is null");
    }
    mainStream_ = mainStream;
    return Result<void>::success();
}

Result<void> StreamService::startMainStream() {
    if (mainStream_ == nullptr) return Result<void>::error(ErrorCode::NotInitialized, "stream service is not initialized");
    mainStream_->start();
    return Result<void>::success();
}

Result<void> StreamService::stopMainStream() {
    if (mainStream_ == nullptr) return Result<void>::error(ErrorCode::NotInitialized, "stream service is not initialized");
    mainStream_->stop();
    return Result<void>::success();
}

Result<void> StreamService::clearMainStream() {
    if (mainStream_ == nullptr) return Result<void>::error(ErrorCode::NotInitialized, "stream service is not initialized");
    mainStream_->clear();
    return Result<void>::success();
}

Result<tri::media::StreamState> StreamService::state() const {
    if (mainStream_ == nullptr) return Result<tri::media::StreamState>::error(ErrorCode::NotInitialized, "stream service is not initialized");
    return Result<tri::media::StreamState>::ok(mainStream_->state());
}

Result<tri::media::StreamProfile> StreamService::profile() const {
    if (mainStream_ == nullptr) return Result<tri::media::StreamProfile>::error(ErrorCode::NotInitialized, "stream service is not initialized");
    return Result<tri::media::StreamProfile>::ok(mainStream_->profile());
}

Result<tri::media::StreamStats> StreamService::stats() const {
    if (mainStream_ == nullptr) return Result<tri::media::StreamStats>::error(ErrorCode::NotInitialized, "stream service is not initialized");
    return Result<tri::media::StreamStats>::ok(mainStream_->stats());
}

Result<tri::media::EncodedFrame> StreamService::latestFrame() const {
    if (mainStream_ == nullptr) return Result<tri::media::EncodedFrame>::error(ErrorCode::NotInitialized, "stream service is not initialized");
    auto frame = mainStream_->latestFrame();
    if (!frame) return Result<tri::media::EncodedFrame>::error(ErrorCode::MainStreamNotReady, "main stream has no frame");
    return Result<tri::media::EncodedFrame>::ok(*frame);
}

Result<tri::media::EncodedFrame> StreamService::waitFrame(int timeoutMs) {
    if (mainStream_ == nullptr) return Result<tri::media::EncodedFrame>::error(ErrorCode::NotInitialized, "stream service is not initialized");
    auto frame = mainStream_->waitFrame(timeoutMs);
    if (!frame) return Result<tri::media::EncodedFrame>::error(ErrorCode::Timeout, "wait main stream frame timeout");
    return Result<tri::media::EncodedFrame>::ok(*frame);
}

} // namespace tri::service
