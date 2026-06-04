#pragma once
#include "foundation/time/Clock.h"
#include "media/frame/VideoFrame.h"
namespace tri::media { class TimestampNode final { public: void stamp(VideoFrame& frame) const { if (frame.ptsMs == 0) frame.ptsMs = foundation::Clock::nowMs(); } }; }
