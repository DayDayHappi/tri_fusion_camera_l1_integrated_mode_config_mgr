#pragma once
#include <cstddef>

namespace tri::hardware::v4l2 {

struct MappedBuffer {
    void* start{nullptr};
    std::size_t length{0};
};

} // namespace tri::hardware::v4l2
