#pragma once

#include "foundation/error/Result.h"
#include "hardware/rga/RgaTypes.h"

namespace tri::hardware::rga {

class RgaConverter final {
public:
    foundation::Result<void> convert(const RgaImageDesc& src, const RgaImageDesc& dst) const;
};

} // namespace tri::hardware::rga
