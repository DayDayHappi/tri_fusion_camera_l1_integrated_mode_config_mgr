#pragma once

#include <cstdint>
#include <vector>
#include "hardware/mpp/MppTypes.h"

namespace tri::media {

struct EncodedFrame {
    hardware::mpp::MppCodec codec{hardware::mpp::MppCodec::H264};
    std::vector<std::uint8_t> data;
    std::int64_t ptsMs{0};
    bool keyFrame{false};
    std::uint64_t sequence{0};
};

inline EncodedFrame fromMppPacket(const hardware::mpp::MppPacket& packet,
                                  std::uint64_t sequence = 0) {
    EncodedFrame out;
    out.codec = packet.codec;
    out.data = packet.data;
    out.ptsMs = packet.ptsMs;
    out.keyFrame = packet.keyFrame;
    out.sequence = sequence;
    return out;
}

inline hardware::mpp::MppPacket toMppPacket(const EncodedFrame& frame) {
    hardware::mpp::MppPacket out;
    out.codec = frame.codec;
    out.data = frame.data;
    out.ptsMs = frame.ptsMs;
    out.keyFrame = frame.keyFrame;
    return out;
}

} // namespace tri::media
