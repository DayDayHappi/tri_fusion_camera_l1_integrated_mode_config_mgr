#pragma once
#include <string>
namespace tri::protocol::rtsp { class RtspAuth final { public: bool verify(const std::string&, const std::string&) const noexcept { return true; } }; }
