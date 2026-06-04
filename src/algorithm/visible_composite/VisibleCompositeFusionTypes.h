#pragma once

#include <cstdint>

namespace tri::algorithm::visible_composite {

struct VisibleCompositeFusionParams {
    float visibleWeight{0.60f};
    float compositeWeight{0.40f};
    bool preferCompositeTargetDetail{true};
    std::uint32_t reserved{0};
};

} // namespace tri::algorithm::visible_composite
