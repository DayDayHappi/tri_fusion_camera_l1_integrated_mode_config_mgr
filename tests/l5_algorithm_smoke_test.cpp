#include "algorithm/FusionEngine.h"
#include "device/CameraTypes.h"
#include "media/frame/SyncedFrameGroup.h"

#include <cstdint>
#include <iostream>

int main() {
    tri::algorithm::FusionEngine engine;
    auto initRet = engine.init();
    if (!initRet) {
        std::cerr << "engine init failed: " << initRet.status().describe() << '\n';
        return 1;
    }

    auto registerRet = engine.registerDefaultAlgorithms();
    if (!registerRet) {
        std::cerr << "register algorithms failed: " << registerRet.status().describe() << '\n';
        return 2;
    }

    auto modeRet = engine.setMode(tri::algorithm::FusionMode::VisibleComposite);
    if (!modeRet) {
        std::cerr << "set mode failed: " << modeRet.status().describe() << '\n';
        return 3;
    }

    tri::media::SyncedFrameGroup group;
    group.visible.source = tri::device::CameraId::Visible;
    group.visible.sequence = 1;
    group.visible.width = 1920;
    group.visible.height = 1080;
    group.visible.format = tri::media::VideoPixelFormat::Nv12;
    group.visible.ptsMs = 1000;
    group.visible.data.assign(16, static_cast<std::uint8_t>(0x11));

    group.composite.source = tri::device::CameraId::CompositeLowThermal;
    group.composite.sequence = 2;
    group.composite.width = 800;
    group.composite.height = 600;
    group.composite.format = tri::media::VideoPixelFormat::Nv12;
    group.composite.ptsMs = 1002;
    group.composite.data.assign(16, static_cast<std::uint8_t>(0x22));
    group.deltaMs = 2;

    auto out = engine.process(group);
    if (!out) {
        std::cerr << "process failed: " << out.status().describe() << '\n';
        return 4;
    }

    if (out.value().data.empty()) {
        std::cerr << "output frame is empty\n";
        return 5;
    }

    std::cout << "l5 algorithm smoke test OK, mode="
              << tri::algorithm::toString(engine.currentMode())
              << ", frames=" << engine.stats().framesProcessed << '\n';
    return 0;
}
