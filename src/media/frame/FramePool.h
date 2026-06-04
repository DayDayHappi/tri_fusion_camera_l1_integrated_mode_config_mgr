#pragma once
#include "media/frame/VideoFrame.h"
namespace tri::media {
class FramePool final {
public:
    VideoFrame acquire() { return VideoFrame{}; }
    void release(VideoFrame&&) {}
};
} // namespace tri::media
