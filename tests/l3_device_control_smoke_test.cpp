#include "device_control/CompositeSensorCommandBuilder.h"
#include "device_control/CompositeSensorCommandParser.h"
#include "device_control/CompositeSensorControlTypes.h"
#include "foundation/config/ConfigTypes.h"
#include "foundation/log/Logger.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

static std::uint16_t be16(const std::vector<std::uint8_t>& v, std::size_t off) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(v[off]) << 8) | v[off + 1]);
}

int main() {
    tri::foundation::LoggerConfig logCfg;
    logCfg.minLevel = tri::foundation::LogLevel::Debug;
    assert(tri::foundation::Logger::instance().init(logCfg).ok());

    tri::foundation::SerialConfig cfg;
    cfg.enable = true;
    cfg.dev = "/dev/null";
    cfg.timeoutMs = 50;
    cfg.retryCount = 1;

    tri::device_control::CompositeSensorCommandBuilder builder;
    assert(builder.init(cfg).ok());

    auto thermal = builder.buildSetOutputMode(tri::device_control::CompositeSensorOutputMode::ThermalOnly);
    assert(thermal.ok());
    assert(thermal.value().size() == 12);
    assert(be16(thermal.value(), 0) == 0x55AA);
    assert(be16(thermal.value(), 2) == 0x0008);
    assert(be16(thermal.value(), 4) == 0x5AF9);
    assert(be16(thermal.value(), 6) == 0x6500);
    assert(be16(thermal.value(), 8) == 0x0001);
    assert(be16(thermal.value(), 10) == 0xC002);

    auto lowlight = builder.buildSetOutputMode(tri::device_control::CompositeSensorOutputMode::LowlightOnly);
    assert(lowlight.ok());
    assert(be16(lowlight.value(), 8) == 0x0002);
    assert(be16(lowlight.value(), 10) == 0xC003);

    auto composite = builder.buildSetOutputMode(tri::device_control::CompositeSensorOutputMode::LowlightThermalComposite);
    assert(composite.ok());
    assert(be16(composite.value(), 8) == 0x0003);
    assert(be16(composite.value(), 10) == 0xC004);

    auto readFusionMode = builder.buildQueryOutputMode();
    assert(readFusionMode.ok());
    assert(readFusionMode.value().size() == 10);
    assert(be16(readFusionMode.value(), 0) == 0x55AA);
    assert(be16(readFusionMode.value(), 2) == 0x0006);
    assert(be16(readFusionMode.value(), 4) == 0x5AF8);
    assert(be16(readFusionMode.value(), 6) == 0x6500);
    assert(be16(readFusionMode.value(), 8) == 0xBFFE);

    auto writeBrightness = builder.buildWriteRegister(
        static_cast<std::uint16_t>(tri::device_control::CompositeSensorRegister::InfraredBrightness), 50);
    assert(writeBrightness.ok());
    assert(be16(writeBrightness.value(), 6) == 0x651C);
    assert(be16(writeBrightness.value(), 8) == 0x0032);
    assert(be16(writeBrightness.value(), 10) == 0xC04F);

    auto badHex = tri::device_control::CompositeSensorCommandBuilder::parseHexCommand("55 AA 0");
    assert(!badHex.ok());

    tri::device_control::CompositeSensorCommandParser parser;

    std::vector<std::uint8_t> writeOk{0x55, 0xAA, 0x00, 0x06, 0x5A, 0xF9, 0x58, 0x58, 0xB3, 0x57};
    auto writeAck = parser.parseWriteRegisterAck(writeOk);
    assert(writeAck.ok());
    assert(writeAck.value().status == tri::device_control::CompositeSensorStatusWord::Executable);

    std::vector<std::uint8_t> writeRejected{0x55, 0xAA, 0x00, 0x06, 0x5A, 0xF9, 0x4B, 0x4B, 0xA6, 0x50};
    auto rejectedAck = parser.parseWriteRegisterAck(writeRejected);
    assert(!rejectedAck.ok());

    std::vector<std::uint8_t> readModeComposite{0x55, 0xAA, 0x00, 0x08, 0x5A, 0xF8, 0x58, 0x58, 0x00, 0x03, 0xB3, 0x5B};
    auto readAck = parser.parseReadRegisterAck(readModeComposite);
    assert(readAck.ok());
    assert(readAck.value().data == 3);

    auto mode = parser.parseModeReport(readModeComposite);
    assert(mode.ok());
    assert(mode.value() == tri::device_control::CompositeSensorOutputMode::LowlightThermalComposite);

    std::cout << "L3 device control smoke test passed\n";
    return 0;
}
