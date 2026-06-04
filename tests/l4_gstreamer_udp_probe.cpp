#include "media/gstreamer/GStreamerPipeline.h"
#include "media/gstreamer/GStreamerPipelineConfig.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <signal.h>
#include <string>
#include <thread>

namespace {
std::atomic_bool gStopRequested{false};
void onSignal(int) { gStopRequested.store(true); }
}

int main(int argc, char** argv) {
    using tri::media::gstreamer::GStreamerPipeline;
    using tri::media::gstreamer::GStreamerPipelineConfig;

    ::signal(SIGINT, onSignal);
    ::signal(SIGTERM, onSignal);

    GStreamerPipelineConfig cfg;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto need = [&](std::string* out) -> bool {
            if (i + 1 >= argc) return false;
            *out = argv[++i];
            return true;
        };
        if (arg == "--device") need(&cfg.device);
        else if (arg == "--host") need(&cfg.udpHost);
        else if (arg == "--port" && i + 1 < argc) cfg.udpPort = std::stoi(argv[++i]);
        else if (arg == "--width" && i + 1 < argc) cfg.width = std::stoi(argv[++i]);
        else if (arg == "--height" && i + 1 < argc) cfg.height = std::stoi(argv[++i]);
        else if (arg == "--fps" && i + 1 < argc) cfg.fps = std::stoi(argv[++i]);
        else if (arg == "--format") need(&cfg.rawFormat);
        else if (arg == "--codec") need(&cfg.inputCodec);
        else if (arg == "--no-verbose") cfg.verbose = false;
    }

    GStreamerPipeline pipeline(cfg);
    std::cout << "[VIDEO CMD] " << pipeline.commandLine() << "\n";
    if (!pipeline.start()) {
        std::cerr << "[ERROR] start failed: " << pipeline.lastError() << "\n";
        return 1;
    }
    std::cout << "[OK] started. Press Ctrl+C to stop.\n";
    while (!gStopRequested.load()) {
        if (!pipeline.isRunning()) {
            std::cerr << "[ERROR] gst-launch exited.\n";
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    pipeline.stop();
    return 0;
}
