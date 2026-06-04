#pragma once

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace tri::foundation::bytes {

inline std::uint16_t be16(const std::uint8_t* p) { return static_cast<std::uint16_t>((p[0] << 8) | p[1]); }
inline void appendBe16(std::vector<std::uint8_t>& out, std::uint16_t v) { out.push_back(static_cast<std::uint8_t>(v >> 8)); out.push_back(static_cast<std::uint8_t>(v & 0xff)); }

inline std::string toHex(const std::vector<std::uint8_t>& data, bool spaced = true) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (spaced && i) oss << ' ';
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return oss.str();
}

inline std::uint16_t checksum16(const std::vector<std::uint8_t>& data) {
    std::uint32_t sum = 0;
    for (std::size_t i = 0; i + 1 < data.size(); i += 2) sum += be16(&data[i]);
    if (data.size() % 2) sum += static_cast<std::uint16_t>(data.back() << 8);
    return static_cast<std::uint16_t>(sum & 0xffff);
}

} // namespace tri::foundation::bytes
