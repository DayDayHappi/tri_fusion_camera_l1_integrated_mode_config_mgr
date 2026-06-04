#pragma once
#include "media/stream/MainStream.h"
namespace tri::media {
class StreamManager final {
public:
    MainStream& mainStream() noexcept { return main_; }
    const MainStream& mainStream() const noexcept { return main_; }
private:
    MainStream main_;
};
} // namespace tri::media
