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

std::string fusionColorToName(FusionColor color) {
    switch (color) {
        case FusionColor::BlackWhite: return "black_white";
        case FusionColor::Forest: return "forest";
        case FusionColor::Snow: return "snow";
        case FusionColor::Ocean: return "ocean";
        case FusionColor::City: return "city";
        case FusionColor::Desert: return "desert";
        case FusionColor::Default: return "default";
    }
    return "unknown";
}

bool fusionColorFromCommandName(const std::string& name, FusionColor* color) {
    if (color == nullptr) return false;
    const auto s = normalize(name);
    if (s == "BLACKWHITE" || s == "BLACKANDWHITE" || s == "BW" || s == "MONO") {
        *color = FusionColor::BlackWhite; return true;
    }
    if (s == "FOREST") { *color = FusionColor::Forest; return true; }
    if (s == "SNOW") { *color = FusionColor::Snow; return true; }
    if (s == "OCEAN") { *color = FusionColor::Ocean; return true; }
    if (s == "CITY") { *color = FusionColor::City; return true; }
    if (s == "DESERT") { *color = FusionColor::Desert; return true; }
    if (s == "DEFAULT" || s == "7") { *color = FusionColor::Default; return true; }
    return false;
}

std::string infraredPolarityToName(InfraredPolarity polarity) {
    switch (polarity) {
        case InfraredPolarity::WhiteHot: return "white_hot";
        case InfraredPolarity::BlackHot: return "black_hot";
    }
    return "unknown";
}

bool infraredPolarityFromCommandName(const std::string& name, InfraredPolarity* polarity) {
    if (polarity == nullptr) return false;
    const auto s = normalize(name);
    if (s == "WHITEHOT" || s == "WHITE") { *polarity = InfraredPolarity::WhiteHot; return true; }
    if (s == "BLACKHOT" || s == "BLACK") { *polarity = InfraredPolarity::BlackHot; return true; }
    return false;
}

std::string contourModeToName(ContourMode mode) {
    switch (mode) {
        case ContourMode::Off: return "off";
        case ContourMode::Red: return "red";
        case ContourMode::Green: return "green";
        case ContourMode::Blue: return "blue";
        case ContourMode::Purple: return "purple";
    }
    return "unknown";
}

bool contourModeFromCommandName(const std::string& name, ContourMode* mode) {
    if (mode == nullptr) return false;
    const auto s = normalize(name);
    if (s == "OFF" || s == "NONE" || s == "DISABLE" || s == "DISABLED" || s == "0") {
        *mode = ContourMode::Off; return true;
    }
    if (s == "RED" || s == "1") { *mode = ContourMode::Red; return true; }
    if (s == "GREEN" || s == "2") { *mode = ContourMode::Green; return true; }
    if (s == "BLUE" || s == "3") { *mode = ContourMode::Blue; return true; }
    if (s == "PURPLE" || s == "4") { *mode = ContourMode::Purple; return true; }
    return false;
}

const std::vector<CompositeRegisterDescriptor>& allCompositeRegisterDescriptors() {
    static const std::vector<CompositeRegisterDescriptor> descriptors{
        {"fusion_mode", CompositeSensorRegister::FusionMode, 0x6500},
        {"infrared_polarity", CompositeSensorRegister::InfraredPolarity, 0x6504},
        {"fusion_color", CompositeSensorRegister::FusionColor, 0x6508},
        {"infrared_correction", CompositeSensorRegister::InfraredCorrection, 0x650C},
        {"lowlight_brightness", CompositeSensorRegister::LowlightBrightness, 0x6514},
        {"lowlight_contrast", CompositeSensorRegister::LowlightContrast, 0x6518},
        {"infrared_brightness", CompositeSensorRegister::InfraredBrightness, 0x651C},
        {"infrared_contrast", CompositeSensorRegister::InfraredContrast, 0x6520},
        {"contour_mode", CompositeSensorRegister::ContourMode, 0x652C},
        {"infrared_registration_zoom", CompositeSensorRegister::InfraredRegistrationZoom, 0x6730},
        {"infrared_registration_offset_x", CompositeSensorRegister::InfraredRegistrationOffsetX, 0x6732},
        {"infrared_registration_offset_y", CompositeSensorRegister::InfraredRegistrationOffsetY, 0x6734},
        {"lowlight_registration_zoom", CompositeSensorRegister::LowlightRegistrationZoom, 0x6736},
        {"lowlight_registration_offset_x", CompositeSensorRegister::LowlightRegistrationOffsetX, 0x6738},
        {"lowlight_registration_offset_y", CompositeSensorRegister::LowlightRegistrationOffsetY, 0x673A},
    };
    return descriptors;
}

const std::vector<CompositeRegisterDescriptor>& keyCompositeConfigRegisterDescriptors() {
    static const std::vector<CompositeRegisterDescriptor> descriptors{
        {"fusion_mode", CompositeSensorRegister::FusionMode, 0x6500},
        {"infrared_polarity", CompositeSensorRegister::InfraredPolarity, 0x6504},
        {"fusion_color", CompositeSensorRegister::FusionColor, 0x6508},
        {"contour_mode", CompositeSensorRegister::ContourMode, 0x652C},
        {"lowlight_brightness", CompositeSensorRegister::LowlightBrightness, 0x6514},
        {"lowlight_contrast", CompositeSensorRegister::LowlightContrast, 0x6518},
        {"infrared_brightness", CompositeSensorRegister::InfraredBrightness, 0x651C},
        {"infrared_contrast", CompositeSensorRegister::InfraredContrast, 0x6520},
        {"infrared_registration_zoom", CompositeSensorRegister::InfraredRegistrationZoom, 0x6730},
        {"infrared_registration_offset_x", CompositeSensorRegister::InfraredRegistrationOffsetX, 0x6732},
        {"infrared_registration_offset_y", CompositeSensorRegister::InfraredRegistrationOffsetY, 0x6734},
        {"lowlight_registration_zoom", CompositeSensorRegister::LowlightRegistrationZoom, 0x6736},
        {"lowlight_registration_offset_x", CompositeSensorRegister::LowlightRegistrationOffsetX, 0x6738},
        {"lowlight_registration_offset_y", CompositeSensorRegister::LowlightRegistrationOffsetY, 0x673A},
    };
    return descriptors;
}

} // namespace tri::device_control
