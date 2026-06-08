#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::atomic_bool g_stop{false};

void onSignal(int) {
    g_stop.store(true);
}

struct Options {
    std::string visibleDevice = "/dev/video3";
    std::string compositeDevice = "/dev/video1";
    std::string host = "192.168.1.153";
    int port = 5004;

    int visibleWidth = 1920;
    int visibleHeight = 1080;
    int compositeWidth = 800;
    int compositeHeight = 600;
    int fps = 30;

    // composite 占比。0.35 表示 visible 65% + composite 35%。
    double alpha = 0.35;

    // 第一版直接使用 videoconvert 做 RGB 转换；后续可替换为 rgaconvert/RGA。
    std::string convertElement = "videoconvert";

    bool verbose = false;
};

struct RgbFrame {
    int width = 0;
    int height = 0;
    GstClockTime pts = GST_CLOCK_TIME_NONE;
    std::vector<std::uint8_t> rgb; // packed RGB, width * height * 3
};

void printUsage(const char* program) {
    std::cout
        << "Usage:\n"
        << "  " << program << " [options]\n\n"
        << "Options:\n"
        << "  --visible-device PATH      visible MJPEG camera, default /dev/video3\n"
        << "  --composite-device PATH    lowlight/thermal YUY2 camera, default /dev/video1\n"
        << "  --host IP                  UDP output host, default 192.168.1.153\n"
        << "  --port PORT                UDP output port, default 5004\n"
        << "  --visible-width N          default 1920\n"
        << "  --visible-height N         default 1080\n"
        << "  --composite-width N        default 800\n"
        << "  --composite-height N       default 600\n"
        << "  --fps N                    default 30\n"
        << "  --alpha FLOAT              composite blend ratio, default 0.35\n"
        << "  --convert ELEMENT          videoconvert or rgaconvert if available, default videoconvert\n"
        << "  --verbose                  print per-frame trace\n\n"
        << "Pipeline:\n"
        << "  visible:   v4l2src(MJPG) -> mppjpegdec -> convert -> RGB -> appsink\n"
        << "  composite: v4l2src(YUY2) -> convert -> RGB -> appsink\n"
        << "  C++:       RGB resize + alpha blend -> NV12\n"
        << "  output:    appsrc(NV12) -> mpph264enc -> h264parse -> mpegtsmux -> udpsink\n";
}

bool readValue(int& i, int argc, char** argv, std::string* out) {
    if (i + 1 >= argc) {
        std::cerr << "[ERROR] missing value after " << argv[i] << "\n";
        return false;
    }
    *out = argv[++i];
    return true;
}

bool readIntValue(int& i, int argc, char** argv, int* out) {
    std::string text;
    if (!readValue(i, argc, argv, &text)) return false;
    try {
        *out = std::stoi(text);
        return true;
    } catch (...) {
        std::cerr << "[ERROR] invalid integer value: " << text << "\n";
        return false;
    }
}

bool readDoubleValue(int& i, int argc, char** argv, double* out) {
    std::string text;
    if (!readValue(i, argc, argv, &text)) return false;
    try {
        *out = std::stod(text);
        return true;
    } catch (...) {
        std::cerr << "[ERROR] invalid float value: " << text << "\n";
        return false;
    }
}

bool parseArgs(int argc, char** argv, Options* opt) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == "--visible-device") {
            if (!readValue(i, argc, argv, &opt->visibleDevice)) return false;
        } else if (arg == "--composite-device") {
            if (!readValue(i, argc, argv, &opt->compositeDevice)) return false;
        } else if (arg == "--host") {
            if (!readValue(i, argc, argv, &opt->host)) return false;
        } else if (arg == "--port") {
            if (!readIntValue(i, argc, argv, &opt->port)) return false;
        } else if (arg == "--visible-width") {
            if (!readIntValue(i, argc, argv, &opt->visibleWidth)) return false;
        } else if (arg == "--visible-height") {
            if (!readIntValue(i, argc, argv, &opt->visibleHeight)) return false;
        } else if (arg == "--composite-width") {
            if (!readIntValue(i, argc, argv, &opt->compositeWidth)) return false;
        } else if (arg == "--composite-height") {
            if (!readIntValue(i, argc, argv, &opt->compositeHeight)) return false;
        } else if (arg == "--fps") {
            if (!readIntValue(i, argc, argv, &opt->fps)) return false;
        } else if (arg == "--alpha") {
            if (!readDoubleValue(i, argc, argv, &opt->alpha)) return false;
        } else if (arg == "--convert") {
            if (!readValue(i, argc, argv, &opt->convertElement)) return false;
        } else if (arg == "--verbose") {
            opt->verbose = true;
        } else {
            std::cerr << "[ERROR] unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return false;
        }
    }

    if (opt->visibleWidth <= 0 || opt->visibleHeight <= 0 ||
        opt->compositeWidth <= 0 || opt->compositeHeight <= 0 || opt->fps <= 0) {
        std::cerr << "[ERROR] invalid size/fps option\n";
        return false;
    }
    if ((opt->visibleWidth % 2) != 0 || (opt->visibleHeight % 2) != 0) {
        std::cerr << "[ERROR] output visible width/height must be even for NV12\n";
        return false;
    }
    opt->alpha = std::max(0.0, std::min(1.0, opt->alpha));
    return true;
}

std::string buildVisiblePipelineDesc(const Options& opt) {
    std::ostringstream ss;
    ss << "v4l2src device=" << opt.visibleDevice << " io-mode=mmap "
       << "! image/jpeg,width=" << opt.visibleWidth
       << ",height=" << opt.visibleHeight
       << ",framerate=" << opt.fps << "/1 "
       << "! mppjpegdec "
       << "! " << opt.convertElement << " "
       << "! video/x-raw,format=RGB,width=" << opt.visibleWidth
       << ",height=" << opt.visibleHeight
       << ",framerate=" << opt.fps << "/1 "
       << "! appsink name=visible_sink emit-signals=false sync=false max-buffers=2 drop=true";
    return ss.str();
}

std::string buildCompositePipelineDesc(const Options& opt) {
    std::ostringstream ss;
    ss << "v4l2src device=" << opt.compositeDevice << " io-mode=mmap "
       << "! video/x-raw,format=YUY2,width=" << opt.compositeWidth
       << ",height=" << opt.compositeHeight
       << ",framerate=" << opt.fps << "/1 "
       << "! " << opt.convertElement << " "
       << "! video/x-raw,format=RGB,width=" << opt.compositeWidth
       << ",height=" << opt.compositeHeight
       << ",framerate=" << opt.fps << "/1 "
       << "! appsink name=composite_sink emit-signals=false sync=false max-buffers=2 drop=true";
    return ss.str();
}

std::string buildEncodePipelineDesc(const Options& opt) {
    std::ostringstream ss;
    ss << "appsrc name=fusion_src is-live=true block=true format=time do-timestamp=false "
       << "caps=video/x-raw,format=NV12,width=" << opt.visibleWidth
       << ",height=" << opt.visibleHeight
       << ",framerate=" << opt.fps << "/1 "
       << "! queue max-size-buffers=4 leaky=downstream "
       << "! mpph264enc "
       << "! h264parse config-interval=1 "
       << "! mpegtsmux "
       << "! udpsink host=" << opt.host
       << " port=" << opt.port
       << " sync=false async=false";
    return ss.str();
}

GstElement* parsePipelineOrNull(const std::string& name, const std::string& desc) {
    GError* error = nullptr;
    std::cerr << "[GST][" << name << "] " << desc << "\n";
    GstElement* pipeline = gst_parse_launch(desc.c_str(), &error);
    if (error != nullptr) {
        std::cerr << "[ERROR][" << name << "] gst_parse_launch failed: "
                  << error->message << "\n";
        g_error_free(error);
        return nullptr;
    }
    if (pipeline == nullptr) {
        std::cerr << "[ERROR][" << name << "] gst_parse_launch returned null\n";
        return nullptr;
    }
    return pipeline;
}

bool setPlaying(const std::string& name, GstElement* pipeline) {
    const GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "[ERROR][" << name << "] failed to set PLAYING\n";
        return false;
    }
    std::cerr << "[OK][" << name << "] PLAYING\n";
    return true;
}

void setNull(const std::string& name, GstElement* pipeline) {
    if (pipeline != nullptr) {
        std::cerr << "[INFO][" << name << "] set NULL\n";
        gst_element_set_state(pipeline, GST_STATE_NULL);
    }
}

bool checkBus(const std::string& name, GstElement* pipeline) {
    GstBus* bus = gst_element_get_bus(pipeline);
    if (bus == nullptr) return true;

    bool ok = true;
    while (true) {
        GstMessage* msg = gst_bus_pop_filtered(
            bus,
            static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
        if (msg == nullptr) break;

        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            GError* err = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_error(msg, &err, &debug);
            std::cerr << "[ERROR][" << name << "] "
                      << (err ? err->message : "unknown") << "\n";
            if (debug != nullptr) {
                std::cerr << "[ERROR][" << name << "][debug] " << debug << "\n";
            }
            if (err) g_error_free(err);
            if (debug) g_free(debug);
            ok = false;
        } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
            std::cerr << "[EOS][" << name << "] received EOS\n";
            ok = false;
        }
        gst_message_unref(msg);
    }

    gst_object_unref(bus);
    return ok;
}

bool pullRgbFrame(GstElement* appsink, const std::string& name, RgbFrame* out) {
    if (appsink == nullptr || out == nullptr) return false;

    GstSample* sample = gst_app_sink_try_pull_sample(
        GST_APP_SINK(appsink), 1000 * GST_MSECOND);
    if (sample == nullptr) {
        std::cerr << "[WARN][" << name << "] appsink timeout\n";
        return false;
    }

    GstCaps* caps = gst_sample_get_caps(sample);
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (caps == nullptr || buffer == nullptr) {
        std::cerr << "[ERROR][" << name << "] sample has no caps or buffer\n";
        gst_sample_unref(sample);
        return false;
    }

    GstVideoInfo info;
    if (!gst_video_info_from_caps(&info, caps)) {
        std::cerr << "[ERROR][" << name << "] failed to parse video info from caps\n";
        gst_sample_unref(sample);
        return false;
    }

    if (GST_VIDEO_INFO_FORMAT(&info) != GST_VIDEO_FORMAT_RGB) {
        std::cerr << "[ERROR][" << name << "] expected RGB but got "
                  << gst_video_format_to_string(GST_VIDEO_INFO_FORMAT(&info)) << "\n";
        gst_sample_unref(sample);
        return false;
    }

    GstVideoFrame frame;
    if (!gst_video_frame_map(&frame, &info, buffer, GST_MAP_READ)) {
        std::cerr << "[ERROR][" << name << "] failed to map video frame\n";
        gst_sample_unref(sample);
        return false;
    }

    const int width = static_cast<int>(GST_VIDEO_INFO_WIDTH(&info));
    const int height = static_cast<int>(GST_VIDEO_INFO_HEIGHT(&info));
    const int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0);
    const std::uint8_t* src = static_cast<const std::uint8_t*>(GST_VIDEO_FRAME_PLANE_DATA(&frame, 0));

    out->width = width;
    out->height = height;
    out->pts = GST_BUFFER_PTS(buffer);
    out->rgb.resize(static_cast<std::size_t>(width) * height * 3);

    for (int y = 0; y < height; ++y) {
        const std::uint8_t* srcRow = src + static_cast<std::size_t>(y) * stride;
        std::uint8_t* dstRow = out->rgb.data() + static_cast<std::size_t>(y) * width * 3;
        std::memcpy(dstRow, srcRow, static_cast<std::size_t>(width) * 3);
    }

    gst_video_frame_unmap(&frame);
    gst_sample_unref(sample);
    return true;
}

inline std::uint8_t clampToByte(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return static_cast<std::uint8_t>(v);
}

std::vector<std::uint8_t> fuseRgbResizeNearest(
    const RgbFrame& visible,
    const RgbFrame& composite,
    double alpha) {
    std::vector<std::uint8_t> out;
    if (visible.width <= 0 || visible.height <= 0 ||
        composite.width <= 0 || composite.height <= 0) {
        return out;
    }

    out.resize(static_cast<std::size_t>(visible.width) * visible.height * 3);
    const double visibleRatio = 1.0 - alpha;
    const double compositeRatio = alpha;

    for (int y = 0; y < visible.height; ++y) {
        const int cy = std::min(composite.height - 1,
                                static_cast<int>((static_cast<long long>(y) * composite.height) / visible.height));
        for (int x = 0; x < visible.width; ++x) {
            const int cx = std::min(composite.width - 1,
                                    static_cast<int>((static_cast<long long>(x) * composite.width) / visible.width));

            const std::size_t vi = (static_cast<std::size_t>(y) * visible.width + x) * 3;
            const std::size_t ci = (static_cast<std::size_t>(cy) * composite.width + cx) * 3;

            out[vi + 0] = clampToByte(static_cast<int>(visible.rgb[vi + 0] * visibleRatio + composite.rgb[ci + 0] * compositeRatio));
            out[vi + 1] = clampToByte(static_cast<int>(visible.rgb[vi + 1] * visibleRatio + composite.rgb[ci + 1] * compositeRatio));
            out[vi + 2] = clampToByte(static_cast<int>(visible.rgb[vi + 2] * visibleRatio + composite.rgb[ci + 2] * compositeRatio));
        }
    }

    return out;
}

void rgbToNv12(const std::vector<std::uint8_t>& rgb,
               int width,
               int height,
               std::vector<std::uint8_t>* nv12) {
    const std::size_t ySize = static_cast<std::size_t>(width) * height;
    const std::size_t uvSize = ySize / 2;
    nv12->assign(ySize + uvSize, 0);

    std::uint8_t* yPlane = nv12->data();
    std::uint8_t* uvPlane = nv12->data() + ySize;

    // Y plane
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t idx = (static_cast<std::size_t>(y) * width + x) * 3;
            const int r = rgb[idx + 0];
            const int g = rgb[idx + 1];
            const int b = rgb[idx + 2];
            const int yy = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
            yPlane[static_cast<std::size_t>(y) * width + x] = clampToByte(yy);
        }
    }

    // Interleaved UV plane, one U/V pair for each 2x2 block.
    for (int y = 0; y < height; y += 2) {
        for (int x = 0; x < width; x += 2) {
            int uSum = 0;
            int vSum = 0;
            int count = 0;

            for (int dy = 0; dy < 2; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    const int px = std::min(width - 1, x + dx);
                    const int py = std::min(height - 1, y + dy);
                    const std::size_t idx = (static_cast<std::size_t>(py) * width + px) * 3;
                    const int r = rgb[idx + 0];
                    const int g = rgb[idx + 1];
                    const int b = rgb[idx + 2];
                    const int u = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
                    const int v = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
                    uSum += u;
                    vSum += v;
                    ++count;
                }
            }

            const std::size_t uvIndex = static_cast<std::size_t>(y / 2) * width + x;
            uvPlane[uvIndex + 0] = clampToByte(uSum / count);
            uvPlane[uvIndex + 1] = clampToByte(vSum / count);
        }
    }
}

bool pushNv12Frame(GstElement* appsrc,
                   const std::vector<std::uint8_t>& nv12,
                   GstClockTime pts,
                   GstClockTime duration) {
    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, nv12.size(), nullptr);
    if (buffer == nullptr) {
        std::cerr << "[ERROR][APP_SRC] failed to allocate buffer\n";
        return false;
    }

    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
        std::cerr << "[ERROR][APP_SRC] failed to map buffer\n";
        gst_buffer_unref(buffer);
        return false;
    }

    std::memcpy(map.data, nv12.data(), nv12.size());
    gst_buffer_unmap(buffer, &map);

    GST_BUFFER_PTS(buffer) = pts;
    GST_BUFFER_DTS(buffer) = pts;
    GST_BUFFER_DURATION(buffer) = duration;

    const GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);
    if (ret != GST_FLOW_OK) {
        std::cerr << "[ERROR][APP_SRC] push buffer failed, flow=" << ret << "\n";
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!parseArgs(argc, argv, &opt)) {
        return 2;
    }

    ::signal(SIGINT, onSignal);
    ::signal(SIGTERM, onSignal);

    gst_init(&argc, &argv);

    std::cerr << "========== GStreamer App Fusion Probe ==========" << "\n";
    std::cerr << "[CONFIG] visible=" << opt.visibleDevice
              << " MJPG " << opt.visibleWidth << "x" << opt.visibleHeight
              << "@" << opt.fps << "\n";
    std::cerr << "[CONFIG] composite=" << opt.compositeDevice
              << " YUY2 " << opt.compositeWidth << "x" << opt.compositeHeight
              << "@" << opt.fps << "\n";
    std::cerr << "[CONFIG] fusion=RGB alpha_blend alpha=" << opt.alpha
              << " output=NV12 " << opt.visibleWidth << "x" << opt.visibleHeight
              << " -> mpph264enc -> udp " << opt.host << ":" << opt.port << "\n";

    GstElement* visiblePipeline = parsePipelineOrNull("VISIBLE_IN", buildVisiblePipelineDesc(opt));
    GstElement* compositePipeline = parsePipelineOrNull("COMPOSITE_IN", buildCompositePipelineDesc(opt));
    GstElement* encodePipeline = parsePipelineOrNull("FUSION_ENC", buildEncodePipelineDesc(opt));

    if (visiblePipeline == nullptr || compositePipeline == nullptr || encodePipeline == nullptr) {
        if (visiblePipeline) gst_object_unref(visiblePipeline);
        if (compositePipeline) gst_object_unref(compositePipeline);
        if (encodePipeline) gst_object_unref(encodePipeline);
        return 1;
    }

    GstElement* visibleSink = gst_bin_get_by_name(GST_BIN(visiblePipeline), "visible_sink");
    GstElement* compositeSink = gst_bin_get_by_name(GST_BIN(compositePipeline), "composite_sink");
    GstElement* fusionSrc = gst_bin_get_by_name(GST_BIN(encodePipeline), "fusion_src");

    if (visibleSink == nullptr || compositeSink == nullptr || fusionSrc == nullptr) {
        std::cerr << "[ERROR] failed to get appsink/appsrc elements\n";
        setNull("VISIBLE_IN", visiblePipeline);
        setNull("COMPOSITE_IN", compositePipeline);
        setNull("FUSION_ENC", encodePipeline);
        gst_object_unref(visiblePipeline);
        gst_object_unref(compositePipeline);
        gst_object_unref(encodePipeline);
        return 1;
    }

    const GstClockTime frameDuration = gst_util_uint64_scale_int(1, GST_SECOND, opt.fps);

    if (!setPlaying("FUSION_ENC", encodePipeline) ||
        !setPlaying("VISIBLE_IN", visiblePipeline) ||
        !setPlaying("COMPOSITE_IN", compositePipeline)) {
        setNull("VISIBLE_IN", visiblePipeline);
        setNull("COMPOSITE_IN", compositePipeline);
        setNull("FUSION_ENC", encodePipeline);
        gst_object_unref(visibleSink);
        gst_object_unref(compositeSink);
        gst_object_unref(fusionSrc);
        gst_object_unref(visiblePipeline);
        gst_object_unref(compositePipeline);
        gst_object_unref(encodePipeline);
        return 1;
    }

    std::uint64_t frameIndex = 0;
    const auto startTime = std::chrono::steady_clock::now();
    auto lastReport = startTime;

    while (!g_stop.load()) {
        if (!checkBus("VISIBLE_IN", visiblePipeline) ||
            !checkBus("COMPOSITE_IN", compositePipeline) ||
            !checkBus("FUSION_ENC", encodePipeline)) {
            break;
        }

        RgbFrame visible;
        RgbFrame composite;

        if (!pullRgbFrame(visibleSink, "VISIBLE_IN", &visible)) {
            continue;
        }
        if (!pullRgbFrame(compositeSink, "COMPOSITE_IN", &composite)) {
            continue;
        }

        std::vector<std::uint8_t> fusedRgb = fuseRgbResizeNearest(visible, composite, opt.alpha);
        if (fusedRgb.empty()) {
            std::cerr << "[ERROR][FUSION] failed to fuse RGB frames\n";
            break;
        }

        std::vector<std::uint8_t> fusedNv12;
        rgbToNv12(fusedRgb, visible.width, visible.height, &fusedNv12);

        const GstClockTime pts = frameIndex * frameDuration;
        if (!pushNv12Frame(fusionSrc, fusedNv12, pts, frameDuration)) {
            break;
        }

        ++frameIndex;

        if (opt.verbose && (frameIndex % static_cast<std::uint64_t>(std::max(1, opt.fps))) == 0) {
            std::cerr << "[FRAME] pushed=" << frameIndex
                      << " visible=" << visible.width << "x" << visible.height
                      << " composite=" << composite.width << "x" << composite.height
                      << " fused_nv12=" << fusedNv12.size() << " bytes\n";
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - lastReport >= std::chrono::seconds(5)) {
            const double seconds = std::chrono::duration<double>(now - startTime).count();
            const double fps = seconds > 0.0 ? static_cast<double>(frameIndex) / seconds : 0.0;
            std::cerr << "[STATS] fused_frames=" << frameIndex
                      << " avg_fps=" << std::fixed << std::setprecision(2) << fps << "\n";
            lastReport = now;
        }
    }

    std::cerr << "[INFO] stopping, total fused frames=" << frameIndex << "\n";

    gst_app_src_end_of_stream(GST_APP_SRC(fusionSrc));
    setNull("VISIBLE_IN", visiblePipeline);
    setNull("COMPOSITE_IN", compositePipeline);
    setNull("FUSION_ENC", encodePipeline);

    gst_object_unref(visibleSink);
    gst_object_unref(compositeSink);
    gst_object_unref(fusionSrc);
    gst_object_unref(visiblePipeline);
    gst_object_unref(compositePipeline);
    gst_object_unref(encodePipeline);

    std::cerr << "[OK] stopped\n";
    return 0;
}
