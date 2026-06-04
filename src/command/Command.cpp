#include "command/Command.h"

#include <utility>

namespace tri::command {
namespace {
std::atomic<std::uint64_t> g_commandId{1};
}

std::string Command::param(const std::string& key, const std::string& fallback) const {
    auto it = params.find(key);
    return it == params.end() ? fallback : it->second;
}

bool Command::hasParam(const std::string& key) const {
    return params.find(key) != params.end();
}

std::uint64_t nextCommandId() {
    return g_commandId.fetch_add(1, std::memory_order_relaxed);
}

Command makeCommand(CommandType type,
                    CommandSource source,
                    CommandParams params,
                    std::string sourceName) {
    Command cmd;
    cmd.id = nextCommandId();
    cmd.type = type;
    cmd.source = source;
    cmd.params = std::move(params);
    cmd.sourceName = std::move(sourceName);
    cmd.timestamp = tri::foundation::Timestamp::now();
    return cmd;
}

} // namespace tri::command
