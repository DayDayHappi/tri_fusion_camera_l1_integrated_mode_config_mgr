#include "foundation/config/ConfigManager.h"
#include "foundation/log/Logger.h"
#include "hardware/common/HardwareConfig.h"
#include "hardware/serial/SerialPort.h"
#include <iostream>

int main(int argc, char** argv) {
    tri::foundation::LoggerConfig logCfg;
    logCfg.minLevel = tri::foundation::LogLevel::Debug;
    tri::foundation::Logger::instance().init(logCfg);
    std::string configDir = argc > 1 ? argv[1] : "configs";
    auto cfgRet = tri::foundation::ConfigManager::instance().loadAll(configDir);
    if (!cfgRet) { std::cerr << cfgRet.status().describe() << "\n"; return 2; }
    auto portCfg = tri::hardware::makeSerialPortConfig(tri::foundation::ConfigManager::instance().serial());
    if (!portCfg) { std::cerr << portCfg.status().describe() << "\n"; return 3; }
    tri::hardware::serial::SerialPort port;
    auto ret = port.openPort(portCfg.value());
    if (!ret) { std::cerr << ret.status().describe() << "\n"; return 4; }
    std::cout << "serial opened: " << portCfg.value().device << "\n";
    return 0;
}
