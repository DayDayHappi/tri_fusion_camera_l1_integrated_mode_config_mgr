#pragma once

#include "command/CommandTypes.h"
#include "foundation/config/ConfigTypes.h"
#include "foundation/error/Result.h"

#include <mutex>
#include <string>
#include <unordered_map>

namespace tri::service {

class ProtocolService final {
public:
    tri::foundation::Result<void> init(const tri::foundation::ProtocolConfig& config);

    tri::foundation::Result<void> acquire(tri::command::CommandSource source);
    void release(tri::command::CommandSource source);

    bool isActive(tri::command::CommandSource source) const;
    std::unordered_map<std::string, std::string> status() const;

private:
    static std::string protocolName(tri::command::CommandSource source);

    mutable std::mutex mutex_;
    tri::foundation::ProtocolConfig config_{};
    std::string activeOwner_;
    bool initialized_{false};
};

} // namespace tri::service
