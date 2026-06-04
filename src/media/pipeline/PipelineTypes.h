#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "device/CameraTypes.h"
#include "foundation/config/ConfigTypes.h"
#include "hardware/mpp/MppTypes.h"
#include "media/frame/VideoFrame.h"

namespace tri::media {

enum class PipelineType {
    VisibleOnly,
    CompositeOnly,
    VisibleCompositeFusion,
};

enum class PipelineState {
    Created,
    Initialized,
    Running,
    Stopped,
    Error,
};

struct PipelineConfig {
    PipelineType type{PipelineType::CompositeOnly};

    bool requireVisible{false};
    bool requireComposite{true};

    int captureTimeoutMs{1000};
    int frameQueueSize{8};

    // MJPG 输入时才需要 decoder。
    bool useDecoder{false};

    // YUYV / NV12 输入要编码成 H264 时打开。
    bool useEncoder{false};

    // useEncoder=true 时，是否先做格式转换。
    // YUYV422 -> NV12 必须为 true。
    bool useFormatConvert{false};

    // MPP H264 encoder 期望输入格式。
    VideoPixelFormat encoderInputFormat{VideoPixelFormat::Nv12};

    // 只有输入本来就是编码帧时才允许 passthrough。
    // YUYV 不能 passthrough。
    bool passthroughEncoded{false};

    foundation::MediaConfig media{};
};

struct PipelineStats {
    std::uint64_t capturedFrames{0};
    std::uint64_t decodedFrames{0};
    std::uint64_t encodedFrames{0};
    std::uint64_t droppedFrames{0};
    std::uint64_t errors{0};
    std::string lastError;
};

inline std::string toString(PipelineType type) {
    switch (type) {
        case PipelineType::VisibleOnly: return "visible_only";
        case PipelineType::CompositeOnly: return "composite_only";
        case PipelineType::VisibleCompositeFusion: return "visible_composite_fusion";
    }
    return "unknown";
}

} // namespace tri::media
