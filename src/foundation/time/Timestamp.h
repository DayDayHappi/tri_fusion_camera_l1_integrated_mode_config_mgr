#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace tri::foundation {

struct Timestamp {
    std::int64_t unixMs{0};
    static Timestamp now();
    std::string toString() const;
};

using SteadyTimePoint = std::chrono::steady_clock::time_point;

} // namespace tri::foundation
