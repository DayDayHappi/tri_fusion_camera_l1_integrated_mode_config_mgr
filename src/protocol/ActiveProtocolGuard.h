#pragma once

#include "command/CommandTypes.h"
#include "foundation/error/Result.h"
#include "protocol/ProtocolTypes.h"
#include "service/ProtocolService.h"

namespace tri::protocol {

class ActiveProtocolGuard final {
public:
    ActiveProtocolGuard() = default;
    explicit ActiveProtocolGuard(tri::service::ProtocolService* service) : service_(service) {}

    tri::foundation::Result<void> init(tri::service::ProtocolService* service);
    tri::foundation::Result<void> acquire(ProtocolKind kind);
    tri::foundation::Result<void> acquire(tri::command::CommandSource source);
    void release(ProtocolKind kind);
    void release(tri::command::CommandSource source);
    bool isActive(ProtocolKind kind) const;
    bool isActive(tri::command::CommandSource source) const;

private:
    tri::service::ProtocolService* service_{nullptr};
};

class ScopedProtocolOwner final {
public:
    ScopedProtocolOwner(ActiveProtocolGuard* guard, tri::command::CommandSource source);
    ~ScopedProtocolOwner();

    ScopedProtocolOwner(const ScopedProtocolOwner&) = delete;
    ScopedProtocolOwner& operator=(const ScopedProtocolOwner&) = delete;

    tri::foundation::Result<void> status() const { return status_; }
    bool ok() const noexcept { return status_.ok(); }

private:
    ActiveProtocolGuard* guard_{nullptr};
    tri::command::CommandSource source_{tri::command::CommandSource::Unknown};
    tri::foundation::Result<void> status_{};
};

} // namespace tri::protocol
