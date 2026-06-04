#pragma once

#include "device_control/CompositeSensorControlTypes.h"
#include "foundation/error/Result.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tri::device_control {

class CompositeSensorCommandParser final {
public:
    foundation::Result<CompositeSensorAck> parseAck(const std::vector<std::uint8_t>& bytes,
                                                    CompositeSensorOutputMode expectedMode) const;

    foundation::Result<CompositeSensorAck> parseWriteRegisterAck(const std::vector<std::uint8_t>& bytes) const;
    foundation::Result<CompositeSensorAck> parseReadRegisterAck(const std::vector<std::uint8_t>& bytes) const;
    foundation::Result<CompositeSensorOutputMode> parseModeReport(const std::vector<std::uint8_t>& bytes) const;

    static CompositeSensorOutputMode parseModeText(const std::string& text);
    static std::uint16_t checksumWords(std::uint16_t a, std::uint16_t b, std::uint16_t c);
    static std::uint16_t checksumWords(std::uint16_t a, std::uint16_t b,
                                       std::uint16_t c, std::uint16_t d);

private:
    static std::uint16_t be16(const std::vector<std::uint8_t>& bytes, std::size_t offset);
};

} // namespace tri::device_control
