#include "device_control/CompositeSensorController.h"
#include "device_control/CompositeSensorControlTypes.h"
#include "foundation/config/ConfigManager.h"
#include "foundation/log/Logger.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

tri::device_control::CompositeSensorOutputMode parseMode(const std::string& s) {
    using tri::device_control::CompositeSensorOutputMode;
    if (s == "lowlight") return CompositeSensorOutputMode::LowlightOnly;
    if (s == "thermal") return CompositeSensorOutputMode::ThermalOnly;
    if (s == "composite") return CompositeSensorOutputMode::LowlightThermalComposite;
    throw std::runtime_error("mode must be: lowlight | thermal | composite");
}

std::uint16_t parseU16(const std::string& s) {
    return static_cast<std::uint16_t>(std::stoul(s, nullptr, 0));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << " ./configs mode lowlight|thermal|composite\n"
                  << "  " << argv[0] << " ./configs read 0x6500\n"
                  << "  " << argv[0] << " ./configs write 0x651C 50\n"
                  << "  " << argv[0] << " ./configs color 1..6\n"
                  << "  " << argv[0] << " ./configs ir_brightness 0..100\n"
                  << "  " << argv[0] << " ./configs lowlight_brightness 0..100\n";
        return 1;
    }

    tri::foundation::LoggerConfig logCfg;
    logCfg.minLevel = tri::foundation::LogLevel::Debug;
    auto logRet = tri::foundation::Logger::instance().init(logCfg);
    if (!logRet) {
        std::cerr << "logger init failed: " << logRet.status().describe() << "\n";
        return 2;
    }

    auto loadRet = tri::foundation::ConfigManager::instance().loadAll(argv[1]);
    if (!loadRet) {
        std::cerr << "load config failed: " << loadRet.status().describe() << "\n";
        return 3;
    }

    tri::device_control::CompositeSensorController controller;
    auto initRet = controller.init(tri::foundation::ConfigManager::instance().serial());
    if (!initRet) {
        std::cerr << "controller init failed: " << initRet.status().describe() << "\n";
        return 4;
    }
    controller.setAckRequired(true);

    const std::string op = argv[2];
    if (op == "mode") {
        if (argc < 4) { std::cerr << "missing mode\n"; return 5; }
        auto mode = parseMode(argv[3]);
        auto ret = controller.setOutputMode(mode);
        if (!ret) { std::cerr << "set mode failed: " << ret.status().describe() << "\n"; return 6; }
        std::cout << "set mode success: " << tri::device_control::toString(mode) << "\n";
        return 0;
    }

    if (op == "read") {
        if (argc < 4) { std::cerr << "missing address\n"; return 5; }
        auto ret = controller.readRegister(parseU16(argv[3]));
        if (!ret) { std::cerr << "read failed: " << ret.status().describe() << "\n"; return 6; }
        std::cout << "read value: " << ret.value() << " (0x" << std::hex << ret.value() << std::dec << ")\n";
        return 0;
    }

    if (op == "write") {
        if (argc < 5) { std::cerr << "missing address/value\n"; return 5; }
        auto ret = controller.writeRegister(parseU16(argv[3]), parseU16(argv[4]));
        if (!ret) { std::cerr << "write failed: " << ret.status().describe() << "\n"; return 6; }
        std::cout << "write success\n";
        return 0;
    }

    if (op == "color") {
        if (argc < 4) { std::cerr << "missing color value\n"; return 5; }
        auto ret = controller.setFusionColor(static_cast<tri::device_control::FusionColor>(parseU16(argv[3])));
        if (!ret) { std::cerr << "set color failed: " << ret.status().describe() << "\n"; return 6; }
        std::cout << "set color success\n";
        return 0;
    }

    if (op == "ir_brightness") {
        if (argc < 4) { std::cerr << "missing value\n"; return 5; }
        auto ret = controller.setInfraredBrightness(parseU16(argv[3]));
        if (!ret) { std::cerr << "set infrared brightness failed: " << ret.status().describe() << "\n"; return 6; }
        std::cout << "set infrared brightness success\n";
        return 0;
    }

    if (op == "lowlight_brightness") {
        if (argc < 4) { std::cerr << "missing value\n"; return 5; }
        auto ret = controller.setLowlightBrightness(parseU16(argv[3]));
        if (!ret) { std::cerr << "set lowlight brightness failed: " << ret.status().describe() << "\n"; return 6; }
        std::cout << "set lowlight brightness success\n";
        return 0;
    }

    std::cerr << "unknown op: " << op << "\n";
    return 5;
}
