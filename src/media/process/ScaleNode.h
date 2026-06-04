#pragma once
#include "foundation/error/Result.h"
#include "media/frame/VideoFrame.h"
namespace tri::media {
class ScaleNode final { public: foundation::Result<VideoFrame> process(const VideoFrame& frame, std::uint32_t, std::uint32_t) { return foundation::Result<VideoFrame>::ok(frame); } };
} // namespace tri::media
