#include "foundation/config/ConfigManager.h"
#include "foundation/log/Logger.h"
#include "device/CameraManager.h"

#include <cassert>
#include <iostream>

int main() {
    tri::foundation::LoggerConfig logCfg;
    logCfg.minLevel = tri::foundation::LogLevel::Debug;
    assert(tri::foundation::Logger::instance().init(logCfg).ok());

    auto cfgRet = tri::foundation::ConfigManager::instance().loadAll("../configs");
    if (!cfgRet) cfgRet = tri::foundation::ConfigManager::instance().loadAll("configs");
    assert(cfgRet.ok());

    tri::device::CameraManager manager;
    auto initRet = manager.init(tri::foundation::ConfigManager::instance().camera());
    assert(initRet.ok());

    auto statuses = manager.queryStatus();
    assert(statuses.size() == 2);
    assert(manager.visibleCamera() != nullptr);
    assert(manager.compositeCamera() != nullptr);
    assert(manager.visibleCamera()->status().configured);
    assert(manager.compositeCamera()->status().configured);
    assert(manager.compositeCamera()->capability().nodes.metadataNode == "/dev/video1");

    std::cout << "L2 device abstraction smoke test passed\n";
    return 0;
}
