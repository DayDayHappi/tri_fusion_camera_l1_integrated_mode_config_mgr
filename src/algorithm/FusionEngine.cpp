#include "algorithm/FusionEngine.h"

#include "algorithm/visible_composite/VisibleCompositeFusion.h"
#include "algorithm/visible_lowlight/VisibleLowlightFusion.h"
#include "algorithm/visible_thermal/VisibleThermalFusion.h"
#include "foundation/error/ErrorCode.h"
#include "foundation/log/Logger.h"
#include "foundation/time/Clock.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace tri::algorithm {
using tri::foundation::ErrorCode;
using tri::foundation::LogCategory;
using tri::foundation::Result;
using tri::foundation::Status;

namespace {
std::string normalize(std::string s) {
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) {
        return std::isspace(c) || c == '_' || c == '-' || c == '.';
    }), s.end());
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return s;
}
}

std::string toString(FusionMode mode) {
    switch (mode) {
        case FusionMode::VisibleLowlight: return "VISIBLE_LOWLIGHT_FUSION";
        case FusionMode::VisibleThermal: return "VISIBLE_THERMAL_FUSION";
        case FusionMode::VisibleComposite: return "VISIBLE_COMPOSITE_FUSION";
        case FusionMode::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

FusionMode fusionModeFromString(const std::string& text) {
    const auto s = normalize(text);
    if (s == "VISIBLELOWLIGHTFUSION" || s == "VISIBLELOWLIGHT") return FusionMode::VisibleLowlight;
    if (s == "VISIBLETHERMALFUSION" || s == "VISIBLETHERMAL") return FusionMode::VisibleThermal;
    if (s == "VISIBLECOMPOSITEFUSION" || s == "VISIBLECOMPOSITE" || s == "TRIFUSION") return FusionMode::VisibleComposite;
    return FusionMode::Unknown;
}

FusionEngine::~FusionEngine() { release(); }

Result<void> FusionEngine::init(FusionConfig config) {
    if (initialized_) {
        return Result<void>::error(ErrorCode::AlreadyInitialized, "fusion engine already initialized");
    }
    config_ = std::move(config);
    initialized_ = true;
    stats_ = FusionStats{};
    TRI_LOG_INFO(LogCategory::Media) << "fusion engine initialized";
    return Result<void>::success();
}

Result<void> FusionEngine::registerAlgorithm(std::unique_ptr<FusionAlgorithm> algorithm) {
    if (!initialized_) {
        return Result<void>::error(ErrorCode::NotInitialized, "fusion engine is not initialized");
    }
    if (!algorithm) {
        return Result<void>::error(ErrorCode::InvalidArgument, "fusion algorithm is null");
    }
    const auto mode = algorithm->mode();
    if (mode == FusionMode::Unknown) {
        return Result<void>::error(ErrorCode::InvalidArgument, "cannot register unknown fusion algorithm");
    }
    auto ret = algorithm->init(config_);
    if (!ret) {
        recordError(ret.status());
        return ret;
    }
    algorithms_[mode] = std::move(algorithm);
    if (currentMode_ == FusionMode::Unknown) currentMode_ = mode;
    return Result<void>::success();
}

Result<void> FusionEngine::registerDefaultAlgorithms() {
    auto r1 = registerAlgorithm(std::make_unique<VisibleLowlightFusion>());
    if (!r1) return r1;
    auto r2 = registerAlgorithm(std::make_unique<VisibleThermalFusion>());
    if (!r2) return r2;
    auto r3 = registerAlgorithm(std::make_unique<VisibleCompositeFusion>());
    if (!r3) return r3;
    return Result<void>::success();
}

Result<void> FusionEngine::setMode(FusionMode mode) {
    if (!initialized_) return Result<void>::error(ErrorCode::NotInitialized, "fusion engine is not initialized");
    if (mode == FusionMode::Unknown) return Result<void>::error(ErrorCode::InvalidArgument, "unknown fusion mode");
    if (algorithms_.find(mode) == algorithms_.end()) {
        return Result<void>::error(ErrorCode::Unsupported, "fusion algorithm not registered: " + toString(mode));
    }
    currentMode_ = mode;
    return Result<void>::success();
}

Result<media::VideoFrame> FusionEngine::process(const media::SyncedFrameGroup& input) {
    if (!initialized_) {
        return Result<media::VideoFrame>::error(ErrorCode::NotInitialized, "fusion engine is not initialized");
    }
    auto* algorithm = currentAlgorithm();
    if (algorithm == nullptr) {
        return Result<media::VideoFrame>::error(ErrorCode::Unsupported, "no current fusion algorithm selected");
    }

    const auto begin = tri::foundation::Clock::nowMs();
    auto ret = algorithm->process(input);
    stats_.lastProcessCostMs = tri::foundation::Clock::nowMs() - begin;
    if (!ret) {
        ++stats_.framesDropped;
        recordError(ret.status());
        return ret;
    }
    ++stats_.framesProcessed;
    stats_.lastError.clear();
    return ret;
}

Result<void> FusionEngine::setParam(const std::string& key, const std::string& value) {
    auto* algorithm = currentAlgorithm();
    if (algorithm == nullptr) return Result<void>::error(ErrorCode::Unsupported, "no current fusion algorithm selected");
    return algorithm->setParam(key, value);
}

Result<std::string> FusionEngine::getParam(const std::string& key) const {
    const auto* algorithm = currentAlgorithm();
    if (algorithm == nullptr) return Result<std::string>::error(ErrorCode::Unsupported, "no current fusion algorithm selected");
    return algorithm->getParam(key);
}

void FusionEngine::release() {
    for (auto& kv : algorithms_) {
        if (kv.second) kv.second->release();
    }
    algorithms_.clear();
    currentMode_ = FusionMode::Unknown;
    initialized_ = false;
}

FusionAlgorithm* FusionEngine::currentAlgorithm() noexcept {
    auto it = algorithms_.find(currentMode_);
    return it == algorithms_.end() ? nullptr : it->second.get();
}

const FusionAlgorithm* FusionEngine::currentAlgorithm() const noexcept {
    auto it = algorithms_.find(currentMode_);
    return it == algorithms_.end() ? nullptr : it->second.get();
}

void FusionEngine::recordError(const Status& status) {
    ++stats_.errors;
    stats_.lastError = status.describe();
    TRI_LOG_ERROR(LogCategory::Media) << "fusion engine error: " << stats_.lastError;
}

} // namespace tri::algorithm
