#include "foundation/config/ConfigManager.h"
#include "foundation/log/Logger.h"
#include "hardware/common/HardwareConfig.h"
#include "hardware/uvc/UvcCapability.h"
#include "hardware/uvc/UvcDevice.h"
#include "hardware/v4l2/V4L2Device.h"
#include "hardware/v4l2/V4L2Types.h"

#include <cstdlib>
#include <iostream>
#include <linux/videodev2.h>

using namespace tri;

int main(int argc, char** argv) {
    foundation::LoggerConfig logCfg;
    logCfg.minLevel = foundation::LogLevel::Debug;
    foundation::Logger::instance().init(logCfg);

    std::string configDir = argc > 1 ? argv[1] : "configs";
    auto cfgRet = foundation::ConfigManager::instance().loadAll(configDir);
    if (!cfgRet) {
        std::cerr << "config load failed: " << cfgRet.status().describe() << "\n";
        return 2;
    }

    auto endpoint = foundation::ConfigManager::instance().camera().compositeLowThermal;
    if (argc > 2) endpoint.videoNode = argv[2];
    if (argc > 3) endpoint.width = std::atoi(argv[3]);
    if (argc > 4) endpoint.height = std::atoi(argv[4]);
    if (argc > 5) endpoint.fps = std::atoi(argv[5]);

    auto nodeInfo = hardware::uvc::UvcDevice::inspectNode(endpoint.videoNode);
    if (!nodeInfo) {
        std::cerr << "inspect failed: " << nodeInfo.status().describe() << "\n";
        return 3;
    }
    std::cout << "node=" << nodeInfo.value().path << " kind=" << hardware::uvc::toString(nodeInfo.value().kind)
              << " card=" << nodeInfo.value().card << "\n";

    hardware::v4l2::V4L2Device dev;
    auto ret = dev.openDevice(endpoint.videoNode);
    if (!ret) { std::cerr << ret.status().describe() << "\n"; return 4; }

    auto caps = dev.queryCapability();
    if (caps) std::cout << "driver=" << caps.value().driver << " card=" << caps.value().card << "\n";

    auto formats = dev.enumFormats();
    if (formats) {
        for (const auto& f : formats.value()) {
            std::cout << "format " << hardware::v4l2::fourccToString(f.pixelformat) << " : " << f.description << "\n";
        }
    }

    auto fmt = hardware::makeCaptureFormat(endpoint);
    if (!fmt) { std::cerr << fmt.status().describe() << "\n"; return 5; }
    ret = dev.setFormat(fmt.value());
    if (!ret) { std::cerr << ret.status().describe() << "\n"; return 6; }
    ret = dev.requestBuffers(4);
    if (!ret) { std::cerr << ret.status().describe() << "\n"; return 7; }
    ret = dev.startStream();
    if (!ret) { std::cerr << ret.status().describe() << "\n"; return 8; }

    for (int i = 0; i < 10; ++i) {
        auto frame = dev.dequeueBuffer(2000);
        if (!frame) { std::cerr << frame.status().describe() << "\n"; break; }
        std::cout << "frame " << i << " index=" << frame.value().index << " bytes=" << frame.value().bytesUsed << "\n";
        dev.enqueueBuffer(frame.value().index);
    }
    dev.stopStream();
    return 0;
}
