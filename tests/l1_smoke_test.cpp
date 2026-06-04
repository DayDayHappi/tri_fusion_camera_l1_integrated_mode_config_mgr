#include "foundation/config/ConfigManager.h"
#include "foundation/log/Logger.h"
#include "hardware/common/HardwareConfig.h"
#include "hardware/serial/SerialProtocol.h"
#include "hardware/v4l2/V4L2Types.h"
#include <cassert>
#include <iostream>

int main() {
    tri::foundation::LoggerConfig logCfg;
    logCfg.minLevel = tri::foundation::LogLevel::Debug;
    assert(tri::foundation::Logger::instance().init(logCfg).ok());

    auto cfgRet = tri::foundation::ConfigManager::instance().loadAll("../configs");
    if (!cfgRet) cfgRet = tri::foundation::ConfigManager::instance().loadAll("configs");
    assert(cfgRet.ok());

    const auto& cam = tri::foundation::ConfigManager::instance().camera().compositeLowThermal;
    auto cap = tri::hardware::makeCaptureFormat(cam);
    assert(cap.ok());
    assert(cap.value().width == 800);
    assert(cap.value().height == 600);

    const auto& ser = tri::foundation::ConfigManager::instance().serial();
    auto sc = tri::hardware::makeSerialPortConfig(ser);
    assert(sc.ok());
    assert(sc.value().baudrate == 115200);

    std::vector<std::uint8_t> frame{0x00,0x08,0x5a,0xf9,0x65,0x10,0x00,0x01};
    auto checksum = tri::hardware::serial::SerialProtocol::checksum16(frame);
    auto withSum = tri::hardware::serial::SerialProtocol::appendChecksumBE(frame).payload;
    auto parsed = tri::hardware::serial::SerialProtocol::parseWithChecksumBE(withSum);
    assert(parsed.ok());
    assert(checksum == tri::hardware::serial::SerialProtocol::checksum16(parsed.value().payload));

    std::cout << "L1 smoke test passed\n";
    return 0;
}
