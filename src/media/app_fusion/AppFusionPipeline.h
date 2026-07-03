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

    // Input sizes. In the new visible-reference fusion mode, visible is the
    // reference frame and is expected to be fixed at 1600x1200.
    int visibleWidth{1600};
    int visibleHeight{1200};
    int compositeWidth{800};
    int compositeHeight{600};

    // Output size. The fusion output is now the visible coordinate system.
    int outputWidth{1600};
    int outputHeight{1200};

    int fps{30};

    double compositeAlpha{0.35};

    // Runtime composite image placement offset in visible/output coordinates.
    // Positive X moves composite image right; negative X moves it left.
    // Positive Y moves composite image down; negative Y moves it up.
    int compositeOffsetX{0};
    int compositeOffsetY{0};

    // Runtime composite shrink size before fusion, expressed as border pixels
    // in the visible/output coordinate system.
    // Example with 1600x1200 output:
    //   horizontal=8  -> L=4,R=4, composite target width=1592
    //   vertical=6    -> T=3,B=3, composite target height=1194
    int compositeCropLeft{0};
    int compositeCropRight{0};
    int compositeCropTop{0};
    int compositeCropBottom{0};

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

    // Runtime composite image placement offset in fusion output coordinates.
    // Positive X moves composite image right; negative X moves it left.
    // Positive Y moves composite image down; negative Y moves it up.
    bool setCompositePositionOffset(int offsetX, int offsetY);
    bool moveCompositePositionOffset(int deltaX, int deltaY);
    std::pair<int, int> compositePositionOffset() const;

    // Runtime composite shrink size before fusion.
    // horizontalPixels is total width compression, split to left/right border.
    // verticalPixels is total height compression, split to top/bottom border.
    // Example: horizontal=8 -> L=4,R=4; vertical=6 -> T=3,B=3.
    bool setCompositeShrinkPixels(int horizontalPixels, int verticalPixels);
    std::pair<int, int> compositeShrinkPixels() const;

    std::string lastError() const;
    std::string description() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tri::media::app_fusion
