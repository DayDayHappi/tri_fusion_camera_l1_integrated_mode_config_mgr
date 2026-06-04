#pragma once

#include "foundation/error/ErrorCode.h"
#include "foundation/error/Result.h"

#include <string>
#include <unordered_map>
#include <utility>

namespace tri::command {

struct CommandResult {
    bool accepted{false};
    tri::foundation::Status status{tri::foundation::Status::success()};
    std::unordered_map<std::string, std::string> fields;

    static CommandResult ok(std::unordered_map<std::string, std::string> out = {}) {
        CommandResult r;
        r.accepted = true;
        r.status = tri::foundation::Status::success();
        r.fields = std::move(out);
        return r;
    }

    static CommandResult error(tri::foundation::ErrorCode code, std::string message = {}) {
        CommandResult r;
        r.accepted = false;
        r.status = tri::foundation::Status::error(code, std::move(message));
        return r;
    }
};

} // namespace tri::command
