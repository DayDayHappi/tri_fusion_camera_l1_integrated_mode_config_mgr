#pragma once

#include "foundation/error/Result.h"
#include "hardware/uvc/UvcTypes.h"
#include <string>

namespace tri::hardware::uvc {

class UvcDevice final {
public:
    static foundation::Result<UvcNodeInfo> inspectNode(const std::string& path);
    static bool isVideoCaptureNode(const UvcNodeInfo& info) noexcept;
    static bool isMetadataNode(const UvcNodeInfo& info) noexcept;
};

} // namespace tri::hardware::uvc
