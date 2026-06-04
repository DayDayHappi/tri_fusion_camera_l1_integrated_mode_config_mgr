#pragma once

#include "algorithm/FusionAlgorithm.h"
#include "algorithm/FusionTypes.h"
#include "foundation/error/Result.h"
#include "media/frame/SyncedFrameGroup.h"
#include "media/frame/VideoFrame.h"

#include <memory>
#include <unordered_map>

namespace tri::algorithm {

class FusionEngine final {
public:
    FusionEngine() = default;
    ~FusionEngine();

    FusionEngine(const FusionEngine&) = delete;
    FusionEngine& operator=(const FusionEngine&) = delete;

    foundation::Result<void> init(FusionConfig config = {});
    foundation::Result<void> registerAlgorithm(std::unique_ptr<FusionAlgorithm> algorithm);
    foundation::Result<void> registerDefaultAlgorithms();

    foundation::Result<void> setMode(FusionMode mode);
    FusionMode currentMode() const noexcept { return currentMode_; }

    foundation::Result<media::VideoFrame> process(const media::SyncedFrameGroup& input);
    foundation::Result<void> setParam(const std::string& key, const std::string& value);
    foundation::Result<std::string> getParam(const std::string& key) const;

    const FusionStats& stats() const noexcept { return stats_; }
    void release();
    bool initialized() const noexcept { return initialized_; }

private:
    FusionAlgorithm* currentAlgorithm() noexcept;
    const FusionAlgorithm* currentAlgorithm() const noexcept;
    void recordError(const foundation::Status& status);

    FusionConfig config_{};
    FusionMode currentMode_{FusionMode::Unknown};
    FusionStats stats_{};
    bool initialized_{false};
    std::unordered_map<FusionMode, std::unique_ptr<FusionAlgorithm>> algorithms_;
};

} // namespace tri::algorithm
