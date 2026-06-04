#pragma once

#include "foundation/error/Result.h"
#include "hardware/serial/SerialFrame.h"
#include <cstdint>
#include <vector>

namespace tri::hardware::serial {

class SerialProtocol final {
public:
    static std::uint16_t checksum16(const std::vector<std::uint8_t>& bytes);
    static SerialFrame appendChecksumBE(std::vector<std::uint8_t> payload);
    static foundation::Result<SerialFrame> parseWithChecksumBE(const std::vector<std::uint8_t>& frame);
};

} // namespace tri::hardware::serial
