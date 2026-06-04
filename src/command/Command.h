#pragma once

#include "command/CommandTypes.h"
#include "foundation/time/Timestamp.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace tri::command {

using CommandParams = std::unordered_map<std::string, std::string>;

struct Command {
    std::uint64_t id{0};
    CommandType type{CommandType::Unknown};
    CommandSource source{CommandSource::Unknown};
    std::string sourceName;
    CommandParams params;
    tri::foundation::Timestamp timestamp{tri::foundation::Timestamp::now()};

    std::string param(const std::string& key, const std::string& fallback = {}) const;
    bool hasParam(const std::string& key) const;
};

std::uint64_t nextCommandId();
Command makeCommand(CommandType type,
                    CommandSource source = CommandSource::Internal,
                    CommandParams params = {},
                    std::string sourceName = {});

} // namespace tri::command
