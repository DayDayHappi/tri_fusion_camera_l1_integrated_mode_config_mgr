#include "device_control/CompositeSensorControlTypes.h"
#include <algorithm>
#include <cctype>

namespace tri::device_control {
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

std::string toString(CompositeSensorOutputMode mode) {
    switch (mode) {
        case CompositeSensorOutputMode::LowlightOnly: return "LOWLIGHT_ONLY";
        case CompositeSensorOutputMode::ThermalOnly: return "THERMAL_ONLY";
        case CompositeSensorOutputMode::LowlightThermalComposite: return "LOWLIGHT_THERMAL_COMPOSITE";
        case CompositeSensorOutputMode::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

std::string toString(FusionColor color) {
    switch (color) {
        case FusionColor::BlackWhite: return "BLACK_WHITE";
        case FusionColor::Forest: return "FOREST";
        case FusionColor::Snow: return "SNOW";
        case FusionColor::Ocean: return "OCEAN";
        case FusionColor::City: return "CITY";
        case FusionColor::Desert: return "DESERT";
        case FusionColor::Default: return "DEFAULT";
    }
    return "UNKNOWN";
}

std::string toString(ContourMode mode) {
    switch (mode) {
        case ContourMode::Off: return "OFF";
        case ContourMode::Red: return "RED";
        case ContourMode::Green: return "GREEN";
        case ContourMode::Blue: return "BLUE";
        case ContourMode::Purple: return "PURPLE";
    }
    return "UNKNOWN";
}

std::string toString(InfraredPolarity polarity) {
    switch (polarity) {
        case InfraredPolarity::WhiteHot: return "WHITE_HOT";
        case InfraredPolarity::BlackHot: return "BLACK_HOT";
    }
    return "UNKNOWN";
}

CompositeSensorOutputMode compositeSensorOutputModeFromString(const std::string& text) {
    const auto s = normalize(text);
    if (s == "LOWLIGHTONLY" || s == "LOWLIGHT" || s == "MICROLIGHTONLY" || s == "MICROLIGHT") {
        return CompositeSensorOutputMode::LowlightOnly;
    }
    if (s == "THERMALONLY" || s == "THERMAL" || s == "INFRAREDONLY" || s == "INFRARED") {
        return CompositeSensorOutputMode::ThermalOnly;
    }
    if (s == "LOWLIGHTTHERMALCOMPOSITE" || s == "LOWLIGHTTHERMAL" ||
        s == "COMPOSITE" || s == "FUSION" || s == "LOWLIGHTTHERMALFUSION") {
        return CompositeSensorOutputMode::LowlightThermalComposite;
    }
    return CompositeSensorOutputMode::Unknown;
}

std::uint16_t outputModeRegisterValue(CompositeSensorOutputMode mode) {
    switch (mode) {
        case CompositeSensorOutputMode::ThermalOnly: return 1;                 // 红外
        case CompositeSensorOutputMode::LowlightOnly: return 2;                // 微光
        case CompositeSensorOutputMode::LowlightThermalComposite: return 3;    // 融合
        case CompositeSensorOutputMode::Unknown: return 0;
    }
    return 0;
}

CompositeSensorOutputMode outputModeFromRegisterValue(std::uint16_t value) {
    switch (value) {
        case 1: return CompositeSensorOutputMode::ThermalOnly;
        case 2: return CompositeSensorOutputMode::LowlightOnly;
        case 3: return CompositeSensorOutputMode::LowlightThermalComposite;
        default: return CompositeSensorOutputMode::Unknown;
    }
}

std::string toString(CompositeSensorStatusWord status) {
    switch (status) {
        case CompositeSensorStatusWord::Executable: return "EXECUTABLE";
        case CompositeSensorStatusWord::Rejected: return "REJECTED";
        case CompositeSensorStatusWord::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

bool isExecutable(CompositeSensorStatusWord status) noexcept {
    return status == CompositeSensorStatusWord::Executable;
}

} // namespace tri::device_control
