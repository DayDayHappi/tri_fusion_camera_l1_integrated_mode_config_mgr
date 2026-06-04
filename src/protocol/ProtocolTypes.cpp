#include "protocol/ProtocolTypes.h"

#include "foundation/error/ErrorCode.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace tri::protocol {
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

ProtocolResponse ProtocolResponse::ok(std::unordered_map<std::string, std::string> out) {
    ProtocolResponse r;
    r.code = 200;
    r.message = "OK";
    r.fields = std::move(out);
    return r;
}

ProtocolResponse ProtocolResponse::error(int code, std::string message) {
    ProtocolResponse r;
    r.code = code;
    r.message = std::move(message);
    return r;
}

std::string toString(ProtocolKind kind) {
    switch (kind) {
        case ProtocolKind::Gb28181: return "gb28181";
        case ProtocolKind::Onvif: return "onvif";
        case ProtocolKind::PrivateApi: return "private";
        case ProtocolKind::Rtsp: return "rtsp";
        case ProtocolKind::Unknown: return "unknown";
    }
    return "unknown";
}

std::string toString(ProtocolEndpointState state) {
    switch (state) {
        case ProtocolEndpointState::Stopped: return "STOPPED";
        case ProtocolEndpointState::Running: return "RUNNING";
        case ProtocolEndpointState::Error: return "ERROR";
    }
    return "UNKNOWN";
}

ProtocolKind protocolKindFromString(const std::string& text) {
    const auto s = normalize(text);
    if (s == "GB28181" || s == "GB") return ProtocolKind::Gb28181;
    if (s == "ONVIF") return ProtocolKind::Onvif;
    if (s == "PRIVATE" || s == "PRIVATEAPI" || s == "HTTP") return ProtocolKind::PrivateApi;
    if (s == "RTSP") return ProtocolKind::Rtsp;
    return ProtocolKind::Unknown;
}

tri::command::CommandSource commandSourceOf(ProtocolKind kind) {
    switch (kind) {
        case ProtocolKind::Gb28181: return tri::command::CommandSource::Gb28181;
        case ProtocolKind::Onvif: return tri::command::CommandSource::Onvif;
        case ProtocolKind::PrivateApi: return tri::command::CommandSource::PrivateApi;
        case ProtocolKind::Rtsp: return tri::command::CommandSource::Rtsp;
        case ProtocolKind::Unknown: return tri::command::CommandSource::Unknown;
    }
    return tri::command::CommandSource::Unknown;
}

ProtocolKind protocolKindOf(tri::command::CommandSource source) {
    switch (source) {
        case tri::command::CommandSource::Gb28181: return ProtocolKind::Gb28181;
        case tri::command::CommandSource::Onvif: return ProtocolKind::Onvif;
        case tri::command::CommandSource::PrivateApi: return ProtocolKind::PrivateApi;
        case tri::command::CommandSource::Rtsp: return ProtocolKind::Rtsp;
        case tri::command::CommandSource::LocalCli:
        case tri::command::CommandSource::Internal:
        case tri::command::CommandSource::Unknown: return ProtocolKind::Unknown;
    }
    return ProtocolKind::Unknown;
}

int protocolCodeFromError(tri::foundation::ErrorCode code) {
    using tri::foundation::ErrorCode;
    switch (code) {
        case ErrorCode::Ok: return 200;
        case ErrorCode::InvalidArgument:
        case ErrorCode::InvalidCommand:
        case ErrorCode::UnsupportedWorkMode: return 400;
        case ErrorCode::ProtocolNotActive: return 403;
        case ErrorCode::NotFound: return 404;
        case ErrorCode::ProtocolBusy:
        case ErrorCode::Busy: return 409;
        case ErrorCode::Timeout:
        case ErrorCode::SerialReadTimeout: return 504;
        case ErrorCode::Unsupported: return 501;
        default: return 500;
    }
}

ProtocolResponse fromCommandResult(const tri::command::CommandResult& result) {
    if (result.accepted && result.status.ok()) {
        return ProtocolResponse::ok(result.fields);
    }
    return ProtocolResponse::error(protocolCodeFromError(result.status.code()), result.status.describe());
}

} // namespace tri::protocol
