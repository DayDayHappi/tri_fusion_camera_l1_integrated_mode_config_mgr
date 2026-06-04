#include "algorithm/visible_lowlight/VisibleLowlightFusion.h"
#include "foundation/error/ErrorCode.h"

namespace tri::algorithm {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> VisibleLowlightFusion::init(const FusionConfig& config) {
    config_ = config;
    runtimeParams_ = config.params;
    if (!config.calibrationFile.empty()) {
        (void)calibration_.load(config.calibrationFile);
    }
    auto ret = aligner_.init(calibration_.profile());
    if (!ret) return ret;
    initialized_ = true;
    return Result<void>::success();
}

Result<media::VideoFrame> VisibleLowlightFusion::process(const media::SyncedFrameGroup& input) {
    if (!initialized_) return Result<media::VideoFrame>::error(ErrorCode::NotInitialized, "VisibleLowlightFusion is not initialized");
    auto aligned = aligner_.align(input);
    if (!aligned) return Result<media::VideoFrame>::error(aligned.status().code(), aligned.status().describe());

    // 空实现：此处后续接入“可见光 + 微光”真实融合算法。
    return Result<media::VideoFrame>::ok(makePlaceholderOutput(aligned.value()));
}

Result<void> VisibleLowlightFusion::setParam(const std::string& key, const std::string& value) {
    if (key.empty()) return Result<void>::error(ErrorCode::InvalidArgument, "fusion param key is empty");
    runtimeParams_[key] = value;
    return Result<void>::success();
}

Result<std::string> VisibleLowlightFusion::getParam(const std::string& key) const {
    auto it = runtimeParams_.find(key);
    if (it == runtimeParams_.end()) return Result<std::string>::error(ErrorCode::NotFound, "fusion param not found: " + key);
    return Result<std::string>::ok(it->second);
}

void VisibleLowlightFusion::release() {
    runtimeParams_.clear();
    calibration_.clear();
    initialized_ = false;
}

media::VideoFrame VisibleLowlightFusion::makePlaceholderOutput(const media::SyncedFrameGroup& input) const {
    if (!input.visible.data.empty()) return input.visible;
    return input.composite;
}

} // namespace tri::algorithm
