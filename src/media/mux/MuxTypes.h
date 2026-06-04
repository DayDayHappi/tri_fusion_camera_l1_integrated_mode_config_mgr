#pragma once
#include <cstdint>
#include <vector>
namespace tri::media { struct MuxPacket { std::vector<std::uint8_t> data; std::int64_t ptsMs{0}; }; }
