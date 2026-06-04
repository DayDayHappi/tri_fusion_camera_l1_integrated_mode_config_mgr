#pragma once

#include "foundation/error/Result.h"
#include "hardware/uvc/UvcTypes.h"
#include <string>
#include <vector>

namespace tri::hardware::uvc {

class UvcMetadataReader final {
public:
    ~UvcMetadataReader();
    foundation::Result<void> openNode(const std::string& path);
    void closeNode();
    bool isOpen() const noexcept { return fd_ >= 0; }
    foundation::Result<UvcMetadataPacket> readPacket(int timeoutMs, std::size_t maxBytes = 4096);
private:
    int fd_{-1};
    std::string path_;
};

} // namespace tri::hardware::uvc
