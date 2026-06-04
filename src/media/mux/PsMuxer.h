#pragma once
#include "foundation/error/Result.h"
#include "media/frame/EncodedFrame.h"
#include "media/mux/MuxTypes.h"
namespace tri::media { class PsMuxer final { public: foundation::Result<MuxPacket> mux(const EncodedFrame& frame) { MuxPacket p; p.data = frame.data; p.ptsMs = frame.ptsMs; return foundation::Result<MuxPacket>::ok(std::move(p)); } }; }
