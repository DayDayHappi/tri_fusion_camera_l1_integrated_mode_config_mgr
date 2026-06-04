#pragma once

#include "command/CommandBus.h"
#include "command/CommandResult.h"
#include "command/CommandTypes.h"
#include "service/ProtocolService.h"
#include "service/StreamService.h"
#include "foundation/error/Result.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace tri::protocol {

enum class ProtocolKind : std::uint8_t {
    Unknown = 0,
    Gb28181,
    Onvif,
    PrivateApi,
    Rtsp,
};

enum class ProtocolEndpointState : std::uint8_t {
    Stopped = 0,
    Running,
    Error,
};

struct ProtocolRuntimeContext {
    tri::command::CommandBus* commandBus{nullptr};
    tri::service::ProtocolService* protocolService{nullptr};
    tri::service::StreamService* streamService{nullptr};
};

struct ProtocolRequest {
    std::string method;
    std::string path;
    std::string body;
    std::unordered_map<std::string, std::string> params;
};

struct ProtocolResponse {
    int code{200};
    std::string message{"OK"};
    std::unordered_map<std::string, std::string> fields;

    static ProtocolResponse ok(std::unordered_map<std::string, std::string> out = {});
    static ProtocolResponse error(int code, std::string message);
};

std::string toString(ProtocolKind kind);
std::string toString(ProtocolEndpointState state);
ProtocolKind protocolKindFromString(const std::string& text);
tri::command::CommandSource commandSourceOf(ProtocolKind kind);
ProtocolKind protocolKindOf(tri::command::CommandSource source);
int protocolCodeFromError(tri::foundation::ErrorCode code);
ProtocolResponse fromCommandResult(const tri::command::CommandResult& result);

} // namespace tri::protocol
