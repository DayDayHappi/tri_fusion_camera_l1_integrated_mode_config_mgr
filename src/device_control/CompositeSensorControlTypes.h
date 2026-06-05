#pragma once

#include <cstdint>
#include <string>

namespace tri::device_control {

enum class CompositeSensorOutputMode : std::uint8_t {
    Unknown = 0,
    LowlightOnly,
    ThermalOnly,
    LowlightThermalComposite,
};

enum class CompositeSensorAckType : std::uint8_t {
    Unknown = 0,
    Ack,
    Nack,
    ModeReport,
};

enum class CompositeSensorCommandCode : std::uint16_t {
    ReadRegister = 0x5AF8,
    WriteRegister = 0x5AF9,
};

enum class CompositeSensorStatusWord : std::uint16_t {
    Unknown = 0x0000,
    Executable = 0x5858,
    Rejected = 0x4B4B,
};

enum class CompositeSensorRegister : std::uint16_t {
    FusionMode = 0x6500,
    InfraredPolarity = 0x6504,
    FusionColor = 0x6508,
    InfraredCorrection = 0x650C,
    LowlightBrightness = 0x6514,
    LowlightContrast = 0x6518,
    InfraredBrightness = 0x651C,
    InfraredContrast = 0x6520,
    InfraredRegistrationZoom = 0x6730,
    InfraredRegistrationOffsetX = 0x6732,
    InfraredRegistrationOffsetY = 0x6734,
    LowlightRegistrationZoom = 0x6736,
    LowlightRegistrationOffsetX = 0x6738,
    LowlightRegistrationOffsetY = 0x673A,
    SaveConfig = 0xFFFF, // 如果厂家文档后续给出明确地址，请替换为真实地址。
};

enum class FusionColor : std::uint16_t {
    BlackWhite = 1,
    Forest = 2,
    Snow = 3,
    Ocean = 4,
    City = 5,
    Desert = 6,
};

enum class InfraredPolarity : std::uint16_t {
    WhiteHot = 0,
    BlackHot = 1,
};

enum class SaveConfigAction : std::uint16_t {
    SaveCurrent = 1,
    RestoreDefault = 2,
};

struct CompositeSensorAck {
    CompositeSensorAckType type{CompositeSensorAckType::Unknown};
    CompositeSensorOutputMode mode{CompositeSensorOutputMode::Unknown};
    CompositeSensorCommandCode originalCommand{CompositeSensorCommandCode::WriteRegister};
    CompositeSensorStatusWord status{CompositeSensorStatusWord::Unknown};
    std::uint16_t data{0};
    std::uint16_t checksum{0};
    std::uint16_t expectedChecksum{0};
    std::string message;
};

std::string toString(CompositeSensorOutputMode mode);
CompositeSensorOutputMode compositeSensorOutputModeFromString(const std::string& text);
std::uint16_t outputModeRegisterValue(CompositeSensorOutputMode mode);
CompositeSensorOutputMode outputModeFromRegisterValue(std::uint16_t value);
std::string toString(CompositeSensorStatusWord status);
bool isExecutable(CompositeSensorStatusWord status) noexcept;

} // namespace tri::device_control
