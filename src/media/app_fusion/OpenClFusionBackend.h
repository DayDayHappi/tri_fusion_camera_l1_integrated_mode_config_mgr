#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tri::media::app_fusion {

struct RgbFrame {
    int width = 0;
    int height = 0;
    std::uint64_t pts = UINT64_MAX;
    std::vector<std::uint8_t> rgb;
};

struct GpuFusionParams {
    int outputWidth = 0;
    int outputHeight = 0;

    double visibleWeight = 0.5;

    int visibleOffsetX = 0;
    int visibleOffsetY = 0;

    bool enableBilinearResize = false;
};

class OpenClFusionBackend {
public:
    OpenClFusionBackend();
    ~OpenClFusionBackend();

    OpenClFusionBackend(const OpenClFusionBackend&) = delete;
    OpenClFusionBackend& operator=(const OpenClFusionBackend&) = delete;

    bool init(int maxVisibleWidth,
              int maxVisibleHeight,
              int outputWidth,
              int outputHeight,
              std::string* errorOut);

    bool isReady() const;

    bool fuseToNv12(const RgbFrame& visible,
                    const RgbFrame& composite,
                    const GpuFusionParams& params,
                    std::vector<std::uint8_t>* outNv12,
                    std::string* errorOut);

    std::string description() const;

private:
    class Impl;
    Impl* impl_ = nullptr;
};

} // namespace tri::media::app_fusion
