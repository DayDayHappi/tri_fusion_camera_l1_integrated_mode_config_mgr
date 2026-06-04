#pragma once
#include <cstdint>
#include <string>
namespace tri::media {
struct StreamSession {
    std::uint64_t id{0};
    std::string owner;
    bool active{false};
};
} // namespace tri::media
