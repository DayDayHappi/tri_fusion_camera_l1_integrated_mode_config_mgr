#pragma once
#include <utility>
#include "protocol/rtsp/RtspTypes.h"
namespace tri::protocol::rtsp {
class RtspSession final { public: explicit RtspSession(RtspSessionInfo info = {}) : info_(std::move(info)) {} void play() { info_.playing = true; } void teardown() { info_.playing = false; } const RtspSessionInfo& info() const noexcept { return info_; } private: RtspSessionInfo info_{}; };
} // namespace tri::protocol::rtsp
