#pragma once

#include <cstdint>

namespace tri::algorithm::visible_thermal {

struct VisibleThermalFusionParams {
    float visibleWeight{0.75f};
    float thermalWeight{0.25f};
    bool enablePseudoColor{true};
    std::uint32_t reserved{0};
};

} // namespace tri::algorithm::visible_thermal
