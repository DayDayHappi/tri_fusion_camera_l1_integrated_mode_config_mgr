#pragma once
#include "media/frame/VideoFrame.h"
namespace tri::media {
struct CompositeFrame {
    VideoFrame visible;
    VideoFrame composite;
    bool hasVisible{false};
    bool hasComposite{false};
};
} // namespace tri::media
