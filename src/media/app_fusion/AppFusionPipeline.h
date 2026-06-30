#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace tri::media::app_fusion {

struct AppFusionOptions {
    std::string visibleDevice{"/dev/video3"};
    std::string compositeDevice{"/dev/video1"};
    std::string udpHost{"192.168.1.153"};
    int udpPort{5004};

    int visibleWidth{1600};
    int visibleHeight{1200};
    int compositeWidth{800};
    int compositeHeight{600};
    int fps{30};

    double compositeAlpha{0.35};
    int visibleOffsetX{0};
    int visibleOffsetY{0};

    // Fusion-only visible shrink/pad before alignment/fusion.
    // Default: compress visible 800x600 to inner 792x594 and fill L/R=4, T/B=3 border with black visible pixels.
    int visibleCropLeft{4};
    int visibleCropRight{4};
    int visibleCropTop{3};
    int visibleCropBottom{3};

    bool gpuBilinearResize{false};
    std::string convertElement{"videoconvert"};
    bool verbose{false};
};

class AppFusionPipeline {
public:
    AppFusionPipeline();
    ~AppFusionPipeline();

    AppFusionPipeline(const AppFusionPipeline&) = delete;
    AppFusionPipeline& operator=(const AppFusionPipeline&) = delete;

    bool start(const AppFusionOptions& options);
    bool stop();
    bool isRunning() const;

    // Runtime visible image placement offset in fusion output coordinates.
    // Positive X moves visible image right; negative X moves it left.
    // Positive Y moves visible image down; negative Y moves it up.
    bool setVisiblePositionOffset(int offsetX, int offsetY);
    bool moveVisiblePositionOffset(int deltaX, int deltaY);
    std::pair<int, int> visiblePositionOffset() const;

    // Runtime visible shrink size before fusion.
    // horizontalPixels is total width compression, split to left/right border.
    // verticalPixels is total height compression, split to top/bottom border.
    // Example: horizontal=8 -> L=4,R=4; vertical=6 -> T=3,B=3.
    bool setVisibleShrinkPixels(int horizontalPixels, int verticalPixels);
    std::pair<int, int> visibleShrinkPixels() const;

    std::string lastError() const;
    std::string description() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tri::media::app_fusion