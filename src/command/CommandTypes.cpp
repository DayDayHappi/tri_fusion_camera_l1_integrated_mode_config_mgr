#include "command/CommandTypes.h"

#include <algorithm>
#include <cctype>

namespace tri::command {
namespace {
std::string normalize(std::string s) {
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) {
        return std::isspace(c) || c == '_' || c == '-' || c == '.';
    }), s.end());
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return s;
}
} // namespace

std::string toString(CommandType type) {
    switch (type) {
        case CommandType::SetWorkMode: return "SET_WORK_MODE";
        case CommandType::GetWorkMode: return "GET_WORK_MODE";
        case CommandType::StartStream: return "START_STREAM";
        case CommandType::StopStream: return "STOP_STREAM";
        case CommandType::QueryStatus: return "QUERY_STATUS";
        case CommandType::SetVideoParam: return "SET_VIDEO_PARAM";
        case CommandType::SetFusionParam: return "SET_FUSION_PARAM";
        case CommandType::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

std::string toString(CommandSource source) {
    switch (source) {
        case CommandSource::Gb28181: return "GB28181";
        case CommandSource::Onvif: return "ONVIF";
        case CommandSource::PrivateApi: return "PRIVATE_API";
        case CommandSource::Rtsp: return "RTSP";
        case CommandSource::LocalCli: return "LOCAL_CLI";
        case CommandSource::Internal: return "INTERNAL";
        case CommandSource::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

CommandType commandTypeFromString(const std::string& text) {
    const auto s = normalize(text);
    if (s == "SETWORKMODE" || s == "SWITCHMODE") return CommandType::SetWorkMode;
    if (s == "GETWORKMODE" || s == "CURRENTMODE") return CommandType::GetWorkMode;
    if (s == "STARTSTREAM" || s == "STARTMAINSTREAM") return CommandType::StartStream;
    if (s == "STOPSTREAM" || s == "STOPMAINSTREAM") return CommandType::StopStream;
    if (s == "QUERYSTATUS" || s == "STATUS") return CommandType::QueryStatus;
    if (s == "SETVIDEOPARAM") return CommandType::SetVideoParam;
    if (s == "SETFUSIONPARAM") return CommandType::SetFusionParam;
    return CommandType::Unknown;
}

CommandSource commandSourceFromString(const std::string& text) {
    const auto s = normalize(text);
    if (s == "GB28181" || s == "GB") return CommandSource::Gb28181;
    if (s == "ONVIF") return CommandSource::Onvif;
    if (s == "PRIVATEAPI" || s == "PRIVATE") return CommandSource::PrivateApi;
    if (s == "RTSP") return CommandSource::Rtsp;
    if (s == "LOCALCLI" || s == "CLI") return CommandSource::LocalCli;
    if (s == "INTERNAL") return CommandSource::Internal;
    return CommandSource::Unknown;
}

} // namespace tri::command
