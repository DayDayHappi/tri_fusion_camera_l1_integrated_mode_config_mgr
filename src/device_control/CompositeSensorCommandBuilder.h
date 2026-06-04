#pragma once

#include "device_control/CompositeSensorControlTypes.h"
#include "foundation/config/ConfigTypes.h"
#include "foundation/error/Result.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tri::device_control {

class CompositeSensorCommandBuilder final {
public:
    foundation::Result<void> init(const foundation::SerialConfig& config);

    foundation::Result<std::vector<std::uint8_t>> buildSetOutputMode(CompositeSensorOutputMode mode) const;
    foundation::Result<std::vector<std::uint8_t>> buildQueryOutputMode() const;

    foundation::Result<std::vector<std::uint8_t>> buildReadRegister(std::uint16_t address) const;
    foundation::Result<std::vector<std::uint8_t>> buildWriteRegister(std::uint16_t address,
                                                                     std::uint16_t value) const;
    foundation::Result<std::vector<std::uint8_t>> buildReadRegister(CompositeSensorRegister reg) const;
    foundation::Result<std::vector<std::uint8_t>> buildWriteRegister(CompositeSensorRegister reg,
                                                                     std::uint16_t value) const;

    static foundation::Result<std::vector<std::uint8_t>> parseHexCommand(const std::string& text);
    static std::uint16_t checksumWords(std::uint16_t a, std::uint16_t b, std::uint16_t c);
    static std::uint16_t checksumWords(std::uint16_t a, std::uint16_t b,
                                       std::uint16_t c, std::uint16_t d);

private:
    foundation::Result<std::vector<std::uint8_t>> commandByKey(const std::string& key) const;
    static void appendBe16(std::vector<std::uint8_t>& out, std::uint16_t value);
    const foundation::SerialConfig* config_{nullptr};
};

} // namespace tri::device_control
