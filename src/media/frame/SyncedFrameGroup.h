#pragma once
#include "media/frame/VideoFrame.h"
namespace tri::media {
struct SyncedFrameGroup {
    VideoFrame visible;
    VideoFrame composite;
    std::int64_t deltaMs{0};
};
} // namespace tri::media
