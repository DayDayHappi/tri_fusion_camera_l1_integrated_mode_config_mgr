#pragma once
#include <cstdint>
#include <vector>

namespace tri::hardware::serial {

struct SerialFrame {
    std::vector<std::uint8_t> payload;
};

} // namespace tri::hardware::serial
