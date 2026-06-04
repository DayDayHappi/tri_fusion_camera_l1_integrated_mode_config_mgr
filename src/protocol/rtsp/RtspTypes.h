#pragma once
#include <cstdint>
#include <string>
namespace tri::protocol::rtsp {
struct RtspServerConfig { std::string host{"0.0.0.0"}; std::uint16_t port{8554}; std::string path{"/live/main"}; };
struct RtspSessionInfo { std::string id; std::string peer; bool playing{false}; };
} // namespace tri::protocol::rtsp
