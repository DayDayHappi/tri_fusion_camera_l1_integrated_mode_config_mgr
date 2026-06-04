#include "protocol/ProtocolCommandAdapter.h"

#include "foundation/error/ErrorCode.h"
#include "foundation/log/Logger.h"

namespace tri::protocol {
using tri::foundation::ErrorCode;
using tri::foundation::LogCategory;
using tri::foundation::Result;

Result<void> ProtocolCommandAdapter::init(ProtocolKind kind,
                                          tri::command::CommandBus* bus,
                                          ActiveProtocolGuard* guard) {
    if (kind == ProtocolKind::Unknown) return Result<void>::error(ErrorCode::InvalidArgument, "invalid protocol kind");
    if (bus == nullptr) return Result<void>::error(ErrorCode::InvalidArgument, "command bus is null");
    kind_ = kind;
    source_ = commandSourceOf(kind);
    bus_ = bus;
    guard_ = guard;
    return Result<void>::success();
}

Result<ProtocolResponse> ProtocolCommandAdapter::submit(tri::command::CommandType type,
                                                        tri::command::CommandParams params) const {
    if (bus_ == nullptr) return Result<ProtocolResponse>::error(ErrorCode::NotInitialized, "protocol command adapter is not initialized");
    if (source_ == tri::command::CommandSource::Unknown) return Result<ProtocolResponse>::error(ErrorCode::InvalidArgument, "invalid protocol command source");

    if (guard_ != nullptr) {
        ScopedProtocolOwner owner(guard_, source_);
        if (!owner.ok()) return Result<ProtocolResponse>::error(owner.status().status().code(), owner.status().status().describe());
        auto ret = bus_->submit(tri::command::makeCommand(type, source_, std::move(params), toString(kind_)));
        if (!ret) return Result<ProtocolResponse>::error(ret.status().code(), ret.status().describe());
        return Result<ProtocolResponse>::ok(fromCommandResult(ret.value()));
    }

    auto ret = bus_->submit(tri::command::makeCommand(type, source_, std::move(params), toString(kind_)));
    if (!ret) return Result<ProtocolResponse>::error(ret.status().code(), ret.status().describe());
    return Result<ProtocolResponse>::ok(fromCommandResult(ret.value()));
}

Result<ProtocolResponse> ProtocolCommandAdapter::setWorkMode(const std::string& mode) const {
    return submit(tri::command::CommandType::SetWorkMode, {{"mode", mode}});
}

Result<ProtocolResponse> ProtocolCommandAdapter::getWorkMode() const {
    return submit(tri::command::CommandType::GetWorkMode);
}

Result<ProtocolResponse> ProtocolCommandAdapter::startStream() const {
    return submit(tri::command::CommandType::StartStream);
}

Result<ProtocolResponse> ProtocolCommandAdapter::stopStream() const {
    return submit(tri::command::CommandType::StopStream);
}

Result<ProtocolResponse> ProtocolCommandAdapter::queryStatus() const {
    return submit(tri::command::CommandType::QueryStatus);
}

} // namespace tri::protocol
