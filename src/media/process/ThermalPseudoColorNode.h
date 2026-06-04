#pragma once
#include "foundation/error/Result.h"
#include "media/frame/VideoFrame.h"
namespace tri::media { class ThermalPseudoColorNode final { public: foundation::Result<VideoFrame> process(const VideoFrame& frame) { return foundation::Result<VideoFrame>::ok(frame); } }; }
