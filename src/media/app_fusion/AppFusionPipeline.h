#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

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

    std::string lastError() const;
    std::string description() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tri::media::app_fusion