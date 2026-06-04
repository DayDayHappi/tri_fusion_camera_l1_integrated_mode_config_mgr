#include "mode/WorkMode.h"

#include <algorithm>
#include <cctype>

namespace tri::mode {
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
} // namespace

std::string toString(WorkMode mode) {
    switch (mode) {
        case WorkMode::VisibleOnly: return "VISIBLE_ONLY";
        case WorkMode::LowlightOnly: return "LOWLIGHT_ONLY";
        case WorkMode::ThermalOnly: return "THERMAL_ONLY";
        case WorkMode::LowlightThermalComposite: return "LOWLIGHT_THERMAL_COMPOSITE";
        case WorkMode::VisibleLowlightFusion: return "VISIBLE_LOWLIGHT_FUSION";
        case WorkMode::VisibleThermalFusion: return "VISIBLE_THERMAL_FUSION";
        case WorkMode::VisibleCompositeFusion: return "VISIBLE_COMPOSITE_FUSION";
        case WorkMode::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

std::string configKey(WorkMode mode) {
    switch (mode) {
        case WorkMode::VisibleOnly: return "visible_only";
        case WorkMode::LowlightOnly: return "lowlight_only";
        case WorkMode::ThermalOnly: return "thermal_only";
        case WorkMode::LowlightThermalComposite: return "lowlight_thermal_composite";
        case WorkMode::VisibleLowlightFusion: return "visible_lowlight_fusion";
        case WorkMode::VisibleThermalFusion: return "visible_thermal_fusion";
        case WorkMode::VisibleCompositeFusion: return "visible_composite_fusion";
        case WorkMode::Unknown: return "unknown";
    }
    return "unknown";
}

WorkMode workModeFromString(const std::string& text) {
    const auto s = normalize(text);
    if (s == "VISIBLEONLY" || s == "VISIBLE") return WorkMode::VisibleOnly;
    if (s == "LOWLIGHTONLY" || s == "LOWLIGHT" || s == "MICROLIGHT") return WorkMode::LowlightOnly;
    if (s == "THERMALONLY" || s == "THERMAL" || s == "INFRARED") return WorkMode::ThermalOnly;
    if (s == "LOWLIGHTTHERMALCOMPOSITE" || s == "LOWLIGHTTHERMAL" ||
        s == "COMPOSITE" || s == "FUSION") {
        return WorkMode::LowlightThermalComposite;
    }
    if (s == "VISIBLELOWLIGHTFUSION" || s == "VISIBLELOWLIGHT") return WorkMode::VisibleLowlightFusion;
    if (s == "VISIBLETHERMALFUSION" || s == "VISIBLETHERMAL") return WorkMode::VisibleThermalFusion;
    if (s == "VISIBLECOMPOSITEFUSION" || s == "VISIBLECOMPOSITE" || s == "TRIFUSION") {
        return WorkMode::VisibleCompositeFusion;
    }
    return WorkMode::Unknown;
}

bool requiresVisibleCamera(WorkMode mode) noexcept {
    return mode == WorkMode::VisibleOnly ||
           mode == WorkMode::VisibleLowlightFusion ||
           mode == WorkMode::VisibleThermalFusion ||
           mode == WorkMode::VisibleCompositeFusion;
}

bool requiresCompositeCamera(WorkMode mode) noexcept {
    return mode == WorkMode::LowlightOnly ||
           mode == WorkMode::ThermalOnly ||
           mode == WorkMode::LowlightThermalComposite ||
           mode == WorkMode::VisibleLowlightFusion ||
           mode == WorkMode::VisibleThermalFusion ||
           mode == WorkMode::VisibleCompositeFusion;
}

bool requiresCompositeSensorSwitch(WorkMode mode) noexcept {
    return requiresCompositeCamera(mode);
}

bool isFusionMode(WorkMode mode) noexcept {
    return mode == WorkMode::VisibleLowlightFusion ||
           mode == WorkMode::VisibleThermalFusion ||
           mode == WorkMode::VisibleCompositeFusion;
}

} // namespace tri::mode
