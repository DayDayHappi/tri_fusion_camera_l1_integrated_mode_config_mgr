#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include "foundation/time/Timestamp.h"

namespace tri::foundation {

class Clock final {
public:
    static Timestamp now();
    static std::string nowString();
    static std::int64_t nowMs();
    static SteadyTimePoint steadyNow();
};

} // namespace tri::foundation
