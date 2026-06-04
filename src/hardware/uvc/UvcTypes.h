#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tri::hardware::uvc {

enum class UvcNodeKind {
    Unknown,
    VideoCapture,
    MetadataCapture,
};

struct UvcNodeInfo {
    std::string path;
    UvcNodeKind kind{UvcNodeKind::Unknown};
    std::string driver;
    std::string card;
    std::string busInfo;
    std::uint32_t capabilities{0};
    std::uint32_t deviceCaps{0};
};

struct UvcMetadataPacket {
    std::vector<std::uint8_t> bytes;
    std::int64_t timestampMs{0};
};

} // namespace tri::hardware::uvc
