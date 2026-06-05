#include "media/gstreamer/GStreamerPipelineBuilder.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>
namespace tri::media::gstreamer {

namespace {

std::string lowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

} // namespace

std::vector<std::string> GStreamerPipelineBuilder::buildUdpMpegTsH264Args(
    const GStreamerPipelineConfig& config) {
    std::vector<std::string> args;

    std::cerr << "[VIDEO][PATH] stage[01] V4L2 capture: "
              << config.device
              << " io-mode=" << config.ioMode
              << " source=" << config.sourceName
              << "\n";

    args.emplace_back(config.gstLaunchPath);

    if (config.eosOnStop) {
        args.emplace_back("-e");
    }

    if (config.verbose) {
        args.emplace_back("-v");
    }

    args.emplace_back("v4l2src");
    args.emplace_back("device=" + config.device);
    args.emplace_back("io-mode=" + config.ioMode);
    args.emplace_back("!");

    if (isMjpegInput(config)) {
        std::ostringstream caps;
        caps << "image/jpeg"
             << ",width=" << config.width
             << ",height=" << config.height
             << ",framerate=" << config.fps << "/1";
        args.emplace_back(caps.str());
        std::cerr << "[VIDEO][PATH] stage[02] input caps: "
          << caps.str() << "\n";
        std::cerr << "[VIDEO][PATH] stage[03] decode: mppjpegdec, MJPEG -> raw video by Rockchip MPP\n";
        // The visible camera outputs MJPEG at high frame rates. Use Rockchip MPP JPEG
        // hardware decoder directly. Do not insert jpegparse here: this UVC camera emits
        // APP1 data that jpegparse reports as invalid, while mppjpegdec can consume the
        // image/jpeg stream directly.
        args.emplace_back("!");
        args.emplace_back("mppjpegdec");
        std::cerr << "[VIDEO][PATH] stage[04] encode: "
          << config.encoder
          << ", raw video -> H.264\n";
    } else {
        std::ostringstream caps;
        caps << "video/x-raw"
             << ",format=" << normalizeRawFormat(config.rawFormat)
             << ",width=" << config.width
             << ",height=" << config.height
             << ",framerate=" << config.fps << "/1";
        args.emplace_back(caps.str());
    }

    args.emplace_back("!");
    args.emplace_back(config.encoder);

    args.emplace_back("!");
    args.emplace_back("h264parse");
    args.emplace_back("config-interval=" + std::to_string(config.h264ConfigInterval));
    std::cerr << "[VIDEO][PATH] stage[05] parse: h264parse config-interval="
          << config.h264ConfigInterval << "\n";
    args.emplace_back("!");
    args.emplace_back(config.muxer);
    std::cerr << "[VIDEO][PATH] stage[06] mux: "
          << config.muxer
          << ", H.264 -> MPEG-TS\n";
    args.emplace_back("!");
    args.emplace_back("udpsink");
    args.emplace_back("host=" + config.udpHost);
    args.emplace_back("port=" + std::to_string(config.udpPort));
    args.emplace_back("sync=" + boolToGst(config.udpSync));
    args.emplace_back("async=" + boolToGst(config.udpAsync));
    std::cerr << "[VIDEO][PATH] stage[07] output: udpsink host="
          << config.udpHost
          << " port=" << config.udpPort
          << " sync=" << boolToGst(config.udpSync)
          << " async=" << boolToGst(config.udpAsync)
          << "\n";
    return args;
}

std::vector<std::string> GStreamerPipelineBuilder::buildUdpMpegTsYuyvH264Args(
    const GStreamerPipelineConfig& config) {
    return buildUdpMpegTsH264Args(config);
}

std::string GStreamerPipelineBuilder::toShellCommand(const std::vector<std::string>& args) {
    std::ostringstream oss;

    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i != 0) {
            oss << ' ';
        }
        oss << shellQuote(args[i]);
    }

    return oss.str();
}

bool GStreamerPipelineBuilder::isMjpegInput(const GStreamerPipelineConfig& config) {
    const std::string codec = lowerCopy(config.inputCodec);
    const std::string format = lowerCopy(config.rawFormat);
    return codec == "mjpeg" || codec == "mjpg" || codec == "jpeg" || codec == "image/jpeg" ||
           format == "mjpeg" || format == "mjpg" || format == "jpeg" || format == "image/jpeg" ||
           format == "mjpg";
}

std::string GStreamerPipelineBuilder::normalizeRawFormat(const std::string& format) {
    const std::string lower = lowerCopy(format);
    if (lower == "yuyv" || lower == "yuyv422" || lower == "yuy2" || lower == "yuyv 4:2:2") {
        return "YUY2";
    }
    if (lower == "uyvy" || lower == "uyvy422") {
        return "UYVY";
    }
    if (lower == "nv12") {
        return "NV12";
    }
    return format.empty() ? "YUY2" : format;
}

std::string GStreamerPipelineBuilder::boolToGst(bool value) {
    return value ? "true" : "false";
}

std::string GStreamerPipelineBuilder::shellQuote(const std::string& value) {
    if (value.empty()) {
        return "''";
    }

    bool needQuote = false;
    for (char c : value) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) ||
              c == '_' || c == '-' || c == '/' || c == '.' || c == '=')) {
            needQuote = true;
            break;
        }
    }

    if (!needQuote) {
        return value;
    }

    std::string out = "'";
    for (char c : value) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('\'');
    return out;
}

} // namespace tri::media::gstreamer
