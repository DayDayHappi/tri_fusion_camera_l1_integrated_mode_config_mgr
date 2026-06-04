#include "protocol/ActiveProtocolGuard.h"

#include "foundation/error/ErrorCode.h"

namespace tri::protocol {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> ActiveProtocolGuard::init(tri::service::ProtocolService* service) {
    if (service == nullptr) return Result<void>::error(ErrorCode::InvalidArgument, "protocol service is null");
    service_ = service;
    return Result<void>::success();
}

Result<void> ActiveProtocolGuard::acquire(ProtocolKind kind) {
    return acquire(commandSourceOf(kind));
}

Result<void> ActiveProtocolGuard::acquire(tri::command::CommandSource source) {
    if (service_ == nullptr) return Result<void>::error(ErrorCode::NotInitialized, "active protocol guard is not initialized");
    return service_->acquire(source);
}

void ActiveProtocolGuard::release(ProtocolKind kind) {
    release(commandSourceOf(kind));
}

void ActiveProtocolGuard::release(tri::command::CommandSource source) {
    if (service_ != nullptr) service_->release(source);
}

bool ActiveProtocolGuard::isActive(ProtocolKind kind) const {
    return isActive(commandSourceOf(kind));
}

bool ActiveProtocolGuard::isActive(tri::command::CommandSource source) const {
    return service_ != nullptr && service_->isActive(source);
}

ScopedProtocolOwner::ScopedProtocolOwner(ActiveProtocolGuard* guard, tri::command::CommandSource source)
    : guard_(guard), source_(source) {
    if (guard_ == nullptr) {
        status_ = Result<void>::error(ErrorCode::InvalidArgument, "active protocol guard is null");
        return;
    }
    status_ = guard_->acquire(source_);
}

ScopedProtocolOwner::~ScopedProtocolOwner() {
    if (guard_ != nullptr && status_) guard_->release(source_);
}

} // namespace tri::protocol
