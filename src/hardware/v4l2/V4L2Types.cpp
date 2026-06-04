#include "hardware/v4l2/V4L2Types.h"

namespace tri::hardware::v4l2 {

std::string fourccToString(std::uint32_t fourcc) {
    char s[5] = {
        static_cast<char>(fourcc & 0xff),
        static_cast<char>((fourcc >> 8) & 0xff),
        static_cast<char>((fourcc >> 16) & 0xff),
        static_cast<char>((fourcc >> 24) & 0xff),
        0
    };
    return s;
}

bool hasCapability(std::uint32_t caps, std::uint32_t flag) { return (caps & flag) == flag; }

} // namespace tri::hardware::v4l2
