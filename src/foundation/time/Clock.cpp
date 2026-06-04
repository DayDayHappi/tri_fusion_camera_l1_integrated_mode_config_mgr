#include "foundation/time/Clock.h"
#include <ctime>
#include <iomanip>
#include <sstream>

namespace tri::foundation {

Timestamp Timestamp::now() { return Clock::now(); }

std::string Timestamp::toString() const {
    std::time_t seconds = static_cast<std::time_t>(unixMs / 1000);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &seconds);
#else
    localtime_r(&seconds, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << (unixMs % 1000);
    return oss.str();
}

Timestamp Clock::now() { return Timestamp{nowMs()}; }

std::int64_t Clock::nowMs() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::string Clock::nowString() { return now().toString(); }

SteadyTimePoint Clock::steadyNow() { return std::chrono::steady_clock::now(); }

} // namespace tri::foundation
