#pragma once

#include <cstdint>

namespace tri::algorithm::visible_lowlight {

struct VisibleLowlightFusionParams {
    float visibleWeight{0.70f};
    float lowlightWeight{0.30f};
    std::uint32_t reserved{0};
};

} // namespace tri::algorithm::visible_lowlight
