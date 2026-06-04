#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include "foundation/error/Result.h"
#include "foundation/event/EventBus.h"
#include "media/capture/CompositeCaptureNode.h"
#include "media/capture/VisibleCaptureNode.h"
#include "media/decode/Decoder.h"
#include "media/encode/Encoder.h"
#include "media/pipeline/PipelineContext.h"
#include "media/pipeline/PipelineGraph.h"
#include "media/pipeline/PipelineTypes.h"
#include "media/process/FormatConvertNode.h"

namespace tri::media {

class MediaPipeline final {
public:
    MediaPipeline() = default;
    ~MediaPipeline();

    MediaPipeline(const MediaPipeline&) = delete;
    MediaPipeline& operator=(const MediaPipeline&) = delete;

    foundation::Result<void> init(PipelineConfig config, PipelineContext context);
    foundation::Result<void> start();
    void stop();

    PipelineState state() const noexcept { return state_; }
    const PipelineStats& stats() const noexcept { return stats_; }
    const PipelineGraph& graph() const noexcept { return graph_; }

private:
    foundation::Result<void> validateContext() const;
    foundation::Result<void> startCaptureNodes();
    void stopCaptureNodes();
    void runLoop();
    foundation::Result<void> processOneFrame();
    foundation::Result<void> prepareFrameForPipeline(VideoFrame& frame);
    foundation::Result<void> prepareFrameForEncoder(VideoFrame& frame);
    foundation::Result<void> publishFrame(const VideoFrame& frame);
    void recordError(const foundation::Status& status);
    void publishEvent(foundation::EventType type, const char* name);

    PipelineConfig config_{};
    PipelineContext context_{};
    PipelineGraph graph_{};
    PipelineStats stats_{};
    PipelineState state_{PipelineState::Created};
    std::unique_ptr<VisibleCaptureNode> visibleCapture_;
    std::unique_ptr<CompositeCaptureNode> compositeCapture_;
    std::unique_ptr<Decoder> decoder_;
    std::unique_ptr<Encoder> encoder_;
    std::unique_ptr<FormatConvertNode> formatConvert_;
    std::atomic<bool> stopFlag_{false};
    std::thread worker_;
};

} // namespace tri::media
