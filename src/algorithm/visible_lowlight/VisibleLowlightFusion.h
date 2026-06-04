#pragma once

#include "algorithm/FusionAlgorithm.h"
#include "algorithm/registration/CalibrationManager.h"
#include "algorithm/registration/ImageAligner.h"
#include "algorithm/visible_lowlight/VisibleLowlightFusionTypes.h"

#include <unordered_map>

namespace tri::algorithm {

class VisibleLowlightFusion final : public FusionAlgorithm {
public:
    FusionMode mode() const noexcept override { return FusionMode::VisibleLowlight; }
    const char* name() const noexcept override { return "VisibleLowlightFusion"; }

    foundation::Result<void> init(const FusionConfig& config) override;
    foundation::Result<media::VideoFrame> process(const media::SyncedFrameGroup& input) override;
    foundation::Result<void> setParam(const std::string& key, const std::string& value) override;
    foundation::Result<std::string> getParam(const std::string& key) const override;
    void release() override;

private:
    media::VideoFrame makePlaceholderOutput(const media::SyncedFrameGroup& input) const;

    FusionConfig config_{};
    visible_lowlight::VisibleLowlightFusionParams params_{};
    registration::CalibrationManager calibration_{};
    registration::ImageAligner aligner_{};
    std::unordered_map<std::string, std::string> runtimeParams_;
    bool initialized_{false};
};

} // namespace tri::algorithm
