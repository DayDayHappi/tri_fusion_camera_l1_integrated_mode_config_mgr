#pragma once
#include <cstdint>
#include <string>
#include "hardware/mpp/MppTypes.h"
namespace tri::media {
enum class StreamState { Stopped, Running, Error };
struct StreamProfile {
    hardware::mpp::MppCodec codec{hardware::mpp::MppCodec::H264};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t fps{25};
    std::uint32_t bitrateKbps{4096};
    std::uint32_t gop{25};
    bool lowLatency{true};
    std::string name{"main"};
};
struct StreamStats {
    std::uint64_t framesIn{0};
    std::uint64_t framesDropped{0};
    std::int64_t lastPtsMs{0};
};
} // namespace tri::media
