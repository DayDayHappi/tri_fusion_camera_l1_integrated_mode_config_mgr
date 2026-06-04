#pragma once

#include "foundation/error/Result.h"
#include "hardware/mpp/MppTypes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tri::hardware::mpp {

struct MppEncoderConfig {
    MppCodec codec{MppCodec::H264};
    MppPixelFormat inputFormat{MppPixelFormat::Nv12};

    std::uint32_t width{800};
    std::uint32_t height{600};
    std::uint32_t horStride{800};
    std::uint32_t verStride{600};

    std::uint32_t fps{30};
    std::uint32_t gop{30};
    std::uint32_t bitrateKbps{2000};

    bool lowLatency{true};
};

struct MppEncodedPacket {
    MppCodec codec{MppCodec::H264};
    std::vector<std::uint8_t> data;
    std::uint64_t ptsMs{0};
    bool keyFrame{false};
};

class MppEncoder final {
public:
    MppEncoder() = default;
    ~MppEncoder();

    MppEncoder(const MppEncoder&) = delete;
    MppEncoder& operator=(const MppEncoder&) = delete;

    foundation::Result<void> init(const MppEncoderConfig& config);

    foundation::Result<MppEncodedPacket> encodeNv12(const std::uint8_t* data,
                                                    std::size_t size,
                                                    std::uint64_t ptsMs);

    void release();

private:
    MppEncoderConfig config_{};
    void* ctx_{nullptr};
    void* mpi_{nullptr};
    bool initialized_{false};
    std::vector<std::uint8_t> h264ExtraInfo_;
    bool prependExtraInfoOnKeyFrame_{true};
};

} // namespace tri::hardware::mpp
