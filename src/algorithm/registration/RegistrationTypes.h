#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace tri::algorithm::registration {

struct ImageSize {
    std::uint32_t width{0};
    std::uint32_t height{0};
};

struct HomographyMatrix {
    std::array<float, 9> values{1.0f, 0.0f, 0.0f,
                                0.0f, 1.0f, 0.0f,
                                0.0f, 0.0f, 1.0f};
};

struct CalibrationProfile {
    std::string name{"default"};
    ImageSize visibleSize{};
    ImageSize compositeSize{};
    HomographyMatrix visibleToComposite{};
    bool valid{false};
};

} // namespace tri::algorithm::registration
