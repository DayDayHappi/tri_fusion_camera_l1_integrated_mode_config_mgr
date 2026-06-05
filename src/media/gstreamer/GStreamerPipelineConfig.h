#pragma once

#include <cstdint>
#include <string>

namespace tri::media::gstreamer {

struct GStreamerPipelineConfig {
    std::string gstLaunchPath{"gst-launch-1.0"};

    bool eosOnStop{true};
    bool verbose{true};
    std::int32_t stopTimeoutMs{2000};

    // Source identity. Used for logs/status only.
    std::string sourceName{"composite"};

    // V4L2 input.
    std::string device{"/dev/video1"};
    std::string ioMode{"mmap"};

    // Supported values:
    //   raw / yuy2 / yuyv / yuyv422  -> video/x-raw ! mpph264enc
    //   mjpeg / mjpg / jpeg          -> image/jpeg ! jpegdec ! videoconvert ! mpph264enc
    std::string inputCodec{"raw"};
    std::string rawFormat{"YUY2"};
    std::int32_t width{800};
    std::int32_t height{600};
    std::int32_t fps{30};

    std::string encoder{"mpph264enc"};
    std::int32_t h264ConfigInterval{1};

    std::string muxer{"mpegtsmux"};

    std::string udpHost{"192.168.1.153"};
    std::int32_t udpPort{5004};
    bool udpSync{false};
    bool udpAsync{false};
};

} // namespace tri::media::gstreamer
