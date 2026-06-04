#include "device/CameraManager.h"
#include "device/CameraTypes.h"
#include "foundation/config/ConfigManager.h"
#include "foundation/log/Logger.h"

#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const std::string configDir = argc >= 2 ? argv[1] : "./configs";
    const int frameCount = argc >= 3 ? std::stoi(argv[2]) : 30;

    tri::foundation::LoggerConfig logCfg;
    logCfg.minLevel = tri::foundation::LogLevel::Debug;
    auto logRet = tri::foundation::Logger::instance().init(logCfg);
    if (!logRet) {
        std::cerr << "logger init failed: " << logRet.status().describe() << "\n";
        return 1;
    }

    auto loadRet = tri::foundation::ConfigManager::instance().loadAll(configDir);
    if (!loadRet) {
        std::cerr << "load config failed: " << loadRet.status().describe() << "\n";
        return 2;
    }

    tri::device::CameraManager manager;

    auto initRet = manager.init(tri::foundation::ConfigManager::instance().camera());
    if (!initRet) {
        std::cerr << "camera manager init failed: " << initRet.status().describe() << "\n";
        return 3;
    }

    auto* visible = manager.visibleCamera();
    if (visible == nullptr) {
        std::cerr << "visible camera object is null\n";
        return 4;
    }

    auto openRet = visible->open();
    if (!openRet) {
        std::cerr << "visible open failed: " << openRet.status().describe() << "\n";
        return 5;
    }

    auto startRet = visible->start();
    if (!startRet) {
        std::cerr << "visible start failed: " << startRet.status().describe() << "\n";
        visible->close();
        return 6;
    }

    std::cout << "visible camera started\n";

    std::ofstream out("visible_l2_capture.mjpg", std::ios::binary);
    if (!out) {
        std::cerr << "open output file failed\n";
        visible->stop();
        visible->close();
        return 7;
    }

    int okFrames = 0;

    for (int i = 0; i < frameCount; ++i) {
        auto frameRet = visible->readFrame(2000);
        if (!frameRet) {
            std::cerr << "read frame failed at index " << i
                      << ": " << frameRet.status().describe() << "\n";
            continue;
        }

        const auto& frame = frameRet.value();

        std::cout << "frame[" << i << "]"
                  << " bytes=" << frame.bytes.size()
                  << " width=" << frame.width
                  << " height=" << frame.height
                  << " timestamp=" << frame.receiveTimestampMs
                  << "\n";

        if (!frame.bytes.empty()) {
            out.write(reinterpret_cast<const char*>(frame.bytes.data()),
                      static_cast<std::streamsize>(frame.bytes.size()));
            ++okFrames;
        }
    }

    visible->stop();
    visible->close();

    std::cout << "visible capture finished, okFrames=" << okFrames
              << ", output=visible_l2_capture.mjpg\n";

    return okFrames > 0 ? 0 : 8;
}
