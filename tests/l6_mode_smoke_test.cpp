#include "mode/ModeSwitchPlanner.h"
#include "mode/WorkMode.h"

#include <iostream>

namespace {
bool expect(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << msg << '\n';
        return false;
    }
    return true;
}
}

int main() {
    tri::foundation::ModeConfig modeCfg;
    modeCfg.defaultMode = "LOWLIGHT_THERMAL_COMPOSITE";
    modeCfg.enabledModes["visible_only"] = true;
    modeCfg.enabledModes["lowlight_only"] = true;
    modeCfg.enabledModes["thermal_only"] = true;
    modeCfg.enabledModes["lowlight_thermal_composite"] = true;
    modeCfg.enabledModes["visible_lowlight_fusion"] = true;
    modeCfg.enabledModes["visible_thermal_fusion"] = true;
    modeCfg.enabledModes["visible_composite_fusion"] = true;

    modeCfg.compositeOutputs["lowlight_only"] = "LOWLIGHT_ONLY";
    modeCfg.compositeOutputs["thermal_only"] = "THERMAL_ONLY";
    modeCfg.compositeOutputs["lowlight_thermal_composite"] = "LOWLIGHT_THERMAL_COMPOSITE";
    modeCfg.compositeOutputs["visible_lowlight_fusion"] = "LOWLIGHT_ONLY";
    modeCfg.compositeOutputs["visible_thermal_fusion"] = "THERMAL_ONLY";
    modeCfg.compositeOutputs["visible_composite_fusion"] = "LOWLIGHT_THERMAL_COMPOSITE";

    tri::foundation::MediaConfig mediaCfg;
    mediaCfg.codec = "h264";
    mediaCfg.width = 1920;
    mediaCfg.height = 1080;
    mediaCfg.fps = 25;
    mediaCfg.bitrate = 4096;
    mediaCfg.gop = 25;
    mediaCfg.lowLatency = true;

    tri::mode::ModeSwitchPlanner planner;
    auto init = planner.init(modeCfg, mediaCfg);
    if (!init) {
        std::cerr << "planner init failed: " << init.status().describe() << '\n';
        return 1;
    }

    auto visible = planner.buildPlan(tri::mode::WorkMode::VisibleOnly);
    if (!visible) {
        std::cerr << "visible plan failed: " << visible.status().describe() << '\n';
        return 2;
    }
    if (!expect(visible.value().requireVisible, "visible mode should require visible camera")) return 3;
    if (!expect(!visible.value().requireComposite, "visible mode should not require composite camera")) return 4;
    if (!expect(visible.value().pipeline.type == tri::media::PipelineType::VisibleOnly,
                "visible mode should use visible-only pipeline")) return 5;

    auto thermalFusion = planner.buildPlan("VISIBLE_THERMAL_FUSION");
    if (!thermalFusion) {
        std::cerr << "thermal fusion plan failed: " << thermalFusion.status().describe() << '\n';
        return 6;
    }
    if (!expect(thermalFusion.value().requireVisible, "visible thermal fusion should require visible camera")) return 7;
    if (!expect(thermalFusion.value().requireComposite, "visible thermal fusion should require composite camera")) return 8;
    if (!expect(thermalFusion.value().compositeOutput ==
                tri::device_control::CompositeSensorOutputMode::ThermalOnly,
                "visible thermal fusion should switch composite sensor to thermal only")) return 9;
    if (!expect(thermalFusion.value().fusionMode == tri::algorithm::FusionMode::VisibleThermal,
                "visible thermal fusion should select VisibleThermal algorithm")) return 10;
    if (!expect(thermalFusion.value().pipeline.type == tri::media::PipelineType::VisibleCompositeFusion,
                "visible thermal fusion should use visible-composite fusion placeholder pipeline")) return 11;

    auto triFusion = planner.buildPlan("tri_fusion");
    if (!triFusion) {
        std::cerr << "tri fusion alias plan failed: " << triFusion.status().describe() << '\n';
        return 12;
    }
    if (!expect(triFusion.value().targetMode == tri::mode::WorkMode::VisibleCompositeFusion,
                "tri_fusion alias should map to visible composite fusion")) return 13;

    std::cout << "l6 mode smoke test OK, last plan="
              << tri::mode::toString(triFusion.value().targetMode) << '\n';
    return 0;
}
