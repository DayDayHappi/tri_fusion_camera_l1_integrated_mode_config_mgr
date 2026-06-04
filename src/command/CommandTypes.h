#pragma once

#include <cstdint>
#include <string>

namespace tri::command {

enum class CommandType : std::uint8_t {
    Unknown = 0,
    SetWorkMode,
    GetWorkMode,
    StartStream,
    StopStream,
    QueryStatus,
    SetVideoParam,
    SetFusionParam,
};

enum class CommandSource : std::uint8_t {
    Unknown = 0,
    Gb28181,
    Onvif,
    PrivateApi,
    Rtsp,
    LocalCli,
    Internal,
};

std::string toString(CommandType type);
std::string toString(CommandSource source);
CommandType commandTypeFromString(const std::string& text);
CommandSource commandSourceFromString(const std::string& text);

} // namespace tri::command
