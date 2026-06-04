#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace tri::algorithm {

enum class FusionMode : std::uint8_t {
    Unknown = 0,
    VisibleLowlight,
    VisibleThermal,
    VisibleComposite,
};

enum class FusionQualityPreset : std::uint8_t {
    LowLatency = 0,
    Balanced,
    Quality,
};

struct FusionConfig {
    bool enabled{true};
    bool enableRegistration{true};
    FusionQualityPreset quality{FusionQualityPreset::LowLatency};
    std::string calibrationFile;
    std::unordered_map<std::string, std::string> params;
};

struct FusionStats {
    std::uint64_t framesProcessed{0};
    std::uint64_t framesDropped{0};
    std::uint64_t errors{0};
    std::int64_t lastProcessCostMs{0};
    std::string lastError;
};

std::string toString(FusionMode mode);
FusionMode fusionModeFromString(const std::string& text);

} // namespace tri::algorithm
