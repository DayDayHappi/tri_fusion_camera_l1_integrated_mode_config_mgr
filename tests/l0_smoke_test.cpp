#include "foundation/config/ConfigManager.h"
#include "foundation/event/EventBus.h"
#include "foundation/log/Logger.h"
#include "foundation/thread/ThreadPool.h"
#include "foundation/utils/ByteUtils.h"

#include <cassert>
#include <iostream>

using namespace tri::foundation;

int main() {
    LoggerConfig logCfg;
    logCfg.minLevel = LogLevel::Debug;
    auto logRet = Logger::instance().init(logCfg);
    assert(logRet.ok());
    TRI_LOG_INFO(LogCategory::App) << "L0 smoke test started";

    auto cfgRet = ConfigManager::instance().loadAll("../configs");
    if (!cfgRet) {
        cfgRet = ConfigManager::instance().loadAll("configs");
    }
    assert(cfgRet.ok());
    const auto& cam = ConfigManager::instance().camera();
    assert(cam.compositeLowThermal.videoNode == "/dev/tri_composite_video");
    assert(cam.compositeLowThermal.metadataNode == "/dev/tri_composite_meta");

    EventBus bus;
    assert(bus.init(false).ok());
    bool received = false;
    bus.subscribe(EventType::SystemStarted, [&](const Event& ev) {
        received = true;
        TRI_LOG_INFO(LogCategory::Event) << "event received: " << ev.name;
    });
    bus.publish(Event{EventType::SystemStarted, "system_started"});
    assert(received);

    ThreadPool pool;
    assert(pool.start(2).ok());
    auto fut = pool.submit([] { return 42; });
    assert(fut.get() == 42);
    pool.stop();

    std::vector<std::uint8_t> frame{0x00, 0x08, 0x5a, 0xf9, 0x65, 0x10, 0x00, 0x01};
    auto sum = bytes::checksum16(frame);
    TRI_LOG_INFO(LogCategory::App) << "checksum16=" << std::hex << sum;

    TRI_LOG_INFO(LogCategory::App) << "L0 smoke test passed";
    return 0;
}
