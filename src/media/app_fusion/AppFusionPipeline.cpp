#include "media/app_fusion/AppFusionPipeline.h"
#include "media/app_fusion/OpenClFusionBackend.h"

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

#if __has_include(<rga/im2d.h>)
#include <rga/im2d.h>
#define TRI_FUSION_APP_HAS_RGA 1
#elif __has_include(<im2d.h>)
#include <im2d.h>
#define TRI_FUSION_APP_HAS_RGA 1
#else
#define TRI_FUSION_APP_HAS_RGA 0
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace tri::media::app_fusion {
namespace {

struct LatestFrameStore {
    std::mutex mutex;
    std::condition_variable cv;
    RgbFrame frame;
    std::uint64_t seq = 0;
    bool hasFrame = false;
};

struct LightStats {
    double meanLuma = 0.0;
    double darkPixelRatio = 0.0;
    double brightPixelRatio = 0.0;
};

enum class LightScene { DAY, DUSK, NIGHT };

const char* lightSceneName(LightScene scene) {
    switch (scene) {
        case LightScene::DAY: return "DAY";
        case LightScene::DUSK: return "DUSK";
        case LightScene::NIGHT: return "NIGHT";
    }
    return "UNKNOWN";
}

struct LightSceneState {
    LightScene current = LightScene::DAY;
    LightScene candidate = LightScene::DAY;
    std::chrono::steady_clock::time_point candidateSince{};
    bool hasCandidateSince = false;
};

double clampDouble(double value, double minValue, double maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

inline std::uint8_t clampToByte(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<std::uint8_t>(value);
}

std::string boolText(bool v) { return v ? "true" : "false"; }

double elapsedMs(std::chrono::steady_clock::time_point begin,
                 std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now()) {
    return static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count()) / 1000.0;
}

struct VisibleBorderPixels {
    int left = 0;
    int right = 0;
    int top = 0;
    int bottom = 0;
};

VisibleBorderPixels splitVisibleShrinkPixels(int horizontalPixels, int verticalPixels) {
    horizontalPixels = std::max(0, horizontalPixels);
    verticalPixels = std::max(0, verticalPixels);

    VisibleBorderPixels border;
    border.left = horizontalPixels / 2;
    border.right = horizontalPixels - border.left;
    border.top = verticalPixels / 2;
    border.bottom = verticalPixels - border.top;
    return border;
}

std::string buildVisiblePipelineDesc(const AppFusionOptions& opt) {
    std::ostringstream ss;
    ss << "v4l2src device=" << opt.visibleDevice
       << " io-mode=mmap do-timestamp=true "
       << "! video/x-raw,format=YUY2,width=" << opt.visibleWidth
       << ",height=" << opt.visibleHeight
       << ",framerate=" << opt.fps << "/1 "
       << "! queue max-size-buffers=2 max-size-bytes=0 max-size-time=0 leaky=downstream "
       << "! videoconvert "
       << "! video/x-raw,format=RGB,width=" << opt.visibleWidth
       << ",height=" << opt.visibleHeight
       << ",framerate=" << opt.fps << "/1 "
       << "! appsink name=visible_sink emit-signals=false sync=false max-buffers=1 drop=true";
    return ss.str();
}

std::string buildCompositePipelineDesc(const AppFusionOptions& opt) {
    std::ostringstream ss;
    ss << "v4l2src device=" << opt.compositeDevice << " io-mode=mmap "
       << "! video/x-raw,format=YUY2,width=" << opt.compositeWidth
       << ",height=" << opt.compositeHeight
       << ",framerate=" << opt.fps << "/1 "
       << "! " << opt.convertElement << " "
       << "! video/x-raw,format=RGB,width=" << opt.compositeWidth
       << ",height=" << opt.compositeHeight
       << ",framerate=" << opt.fps << "/1 "
       << "! appsink name=composite_sink emit-signals=false sync=false max-buffers=1 drop=true";
    return ss.str();
}

std::string buildEncodePipelineDesc(const AppFusionOptions& opt) {
    std::ostringstream ss;
    ss << "appsrc name=fusion_src is-live=true block=false format=time do-timestamp=true "
       << "caps=video/x-raw,format=NV12,width=" << opt.compositeWidth
       << ",height=" << opt.compositeHeight
       << ",framerate=" << opt.fps << "/1 "
       << "! queue max-size-buffers=1 max-size-time=0 max-size-bytes=0 leaky=downstream "
       << "! mpph264enc "
       << "! h264parse config-interval=1 "
       << "! mpegtsmux "
       << "! udpsink host=" << opt.udpHost
       << " port=" << opt.udpPort
       << " sync=false async=false";
    return ss.str();
}

GstElement* parsePipelineOrNull(const std::string& name, const std::string& desc, std::string* errorOut) {
    GError* error = nullptr;
    std::cerr << "[GST][" << name << "] " << desc << "\n";
    GstElement* pipeline = gst_parse_launch(desc.c_str(), &error);
    if (error != nullptr) {
        std::ostringstream ss;
        ss << "gst_parse_launch failed for " << name << ": " << error->message;
        if (errorOut) *errorOut = ss.str();
        std::cerr << "[ERROR][" << name << "] " << ss.str() << "\n";
        g_error_free(error);
        return nullptr;
    }
    if (pipeline == nullptr) {
        if (errorOut) *errorOut = "gst_parse_launch returned null for " + name;
        return nullptr;
    }
    return pipeline;
}

bool setPlaying(const std::string& name, GstElement* pipeline, std::string* errorOut) {
    const GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        if (errorOut) *errorOut = "failed to set PLAYING: " + name;
        std::cerr << "[ERROR][" << name << "] failed to set PLAYING\n";
        return false;
    }
    std::cerr << "[OK][" << name << "] PLAYING\n";
    return true;
}

void setNullAndUnref(const std::string& name, GstElement*& pipeline) {
    if (pipeline != nullptr) {
        std::cerr << "[INFO][" << name << "] set NULL\n";
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }
}

bool checkBus(const std::string& name, GstElement* pipeline, std::string* errorOut) {
    if (pipeline == nullptr) return true;
    GstBus* bus = gst_element_get_bus(pipeline);
    if (bus == nullptr) return true;

    bool ok = true;
    while (true) {
        GstMessage* msg = gst_bus_pop_filtered(
            bus, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
        if (msg == nullptr) break;

        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            GError* err = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_error(msg, &err, &debug);
            std::ostringstream ss;
            ss << "[" << name << "] " << (err ? err->message : "unknown gstreamer error");
            if (errorOut) *errorOut = ss.str();
            std::cerr << "[ERROR]" << ss.str() << "\n";
            if (debug != nullptr) std::cerr << "[ERROR][" << name << "][debug] " << debug << "\n";
            if (err) g_error_free(err);
            if (debug) g_free(debug);
            ok = false;
        } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
            if (errorOut) *errorOut = "EOS received from " + name;
            std::cerr << "[EOS][" << name << "] received EOS\n";
            ok = false;
        }
        gst_message_unref(msg);
    }

    gst_object_unref(bus);
    return ok;
}

bool pullRgbFrame(GstElement* appsink, const std::string& name, RgbFrame* out, int timeoutMs) {
    if (appsink == nullptr || out == nullptr) return false;

    GstSample* sample = gst_app_sink_try_pull_sample(
        GST_APP_SINK(appsink), static_cast<GstClockTime>(timeoutMs) * GST_MSECOND);
    if (sample == nullptr) return false;

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
    const auto* src = static_cast<const std::uint8_t*>(GST_VIDEO_FRAME_PLANE_DATA(&frame, 0));

    out->width = width;
    out->height = height;
    out->pts = GST_BUFFER_PTS(buffer);
    out->rgb.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);

    for (int y = 0; y < height; ++y) {
        const std::uint8_t* srcRow = src + static_cast<std::size_t>(y) * stride;
        std::uint8_t* dstRow = out->rgb.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 3;
        std::memcpy(dstRow, srcRow, static_cast<std::size_t>(width) * 3);
    }

    gst_video_frame_unmap(&frame);
    gst_sample_unref(sample);
    return true;
}

void storeLatestFrame(LatestFrameStore* store, RgbFrame&& frame) {
    if (store == nullptr) return;
    {
        std::lock_guard<std::mutex> lock(store->mutex);
        store->frame = std::move(frame);
        ++store->seq;
        store->hasFrame = true;
    }
    store->cv.notify_all();
}

bool loadLatestFrame(LatestFrameStore* store, RgbFrame* out, std::uint64_t* seqOut) {
    if (store == nullptr || out == nullptr) return false;
    std::lock_guard<std::mutex> lock(store->mutex);
    if (!store->hasFrame) return false;
    *out = store->frame;
    if (seqOut != nullptr) *seqOut = store->seq;
    return true;
}

LightStats computeLightStatsRgb(const RgbFrame& frame) {
    LightStats stats;
    if (frame.width <= 0 || frame.height <= 0 || frame.rgb.empty()) return stats;

    constexpr int sampleStep = 8;
    constexpr int darkThreshold = 25;
    constexpr int brightThreshold = 180;
    std::uint64_t lumaSum = 0;
    std::uint64_t sampleCount = 0;
    std::uint64_t darkCount = 0;
    std::uint64_t brightCount = 0;

    for (int y = 0; y < frame.height; y += sampleStep) {
        const std::uint8_t* row = frame.rgb.data() +
            static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) * 3;
        for (int x = 0; x < frame.width; x += sampleStep) {
            const std::uint8_t* p = row + static_cast<std::size_t>(x) * 3;
            const int r = p[0];
            const int g = p[1];
            const int b = p[2];
            const int luma = (77 * r + 150 * g + 29 * b) >> 8;
            lumaSum += static_cast<std::uint64_t>(luma);
            ++sampleCount;
            if (luma < darkThreshold) ++darkCount;
            if (luma > brightThreshold) ++brightCount;
        }
    }

    if (sampleCount == 0) return stats;
    stats.meanLuma = static_cast<double>(lumaSum) / static_cast<double>(sampleCount);
    stats.darkPixelRatio = static_cast<double>(darkCount) / static_cast<double>(sampleCount);
    stats.brightPixelRatio = static_cast<double>(brightCount) / static_cast<double>(sampleCount);
    return stats;
}

LightScene classifyLightSceneCandidate(const LightStats& stats) {
    if (stats.meanLuma < 25.0 && stats.darkPixelRatio > 0.75) return LightScene::NIGHT;
    if (stats.meanLuma > 100.0 && stats.darkPixelRatio < 0.30) return LightScene::DAY;
    return LightScene::DUSK;
}

void updateLightSceneState(LightSceneState* state, LightScene candidate, std::chrono::steady_clock::time_point now) {
    if (state == nullptr) return;
    constexpr auto stableDuration = std::chrono::seconds(3);
    if (!state->hasCandidateSince || state->candidate != candidate) {
        state->candidate = candidate;
        state->candidateSince = now;
        state->hasCandidateSince = true;
        return;
    }
    if (state->current != candidate && now - state->candidateSince >= stableDuration) {
        std::cerr << "[LIGHT][STATE] " << lightSceneName(state->current)
                  << " -> " << lightSceneName(candidate)
                  << " after stable_ms="
                  << std::chrono::duration_cast<std::chrono::milliseconds>(now - state->candidateSince).count()
                  << "\n";
        state->current = candidate;
    }
}

double computeTargetVisibleWeight(const LightStats& stats, LightScene scene, double maxVisibleWeight) {
    maxVisibleWeight = clampDouble(maxVisibleWeight, 0.0, 1.0);
    if (scene == LightScene::NIGHT) return 0.0;
    if (scene == LightScene::DAY) return maxVisibleWeight;

    constexpr double minUsefulLuma = 25.0;
    constexpr double dayLikeLuma = 100.0;
    double t = (stats.meanLuma - minUsefulLuma) / (dayLikeLuma - minUsefulLuma);
    t = clampDouble(t, 0.0, 1.0);
    if (stats.darkPixelRatio > 0.70) t *= 0.35;
    else if (stats.darkPixelRatio > 0.50) t *= 0.65;
    if (stats.brightPixelRatio > 0.08 && stats.meanLuma < 50.0) t *= 0.60;
    return clampDouble(t * maxVisibleWeight, 0.0, maxVisibleWeight);
}

double smoothVisibleWeight(double previousWeight, double targetWeight) {
    constexpr double smoothFactor = 0.05;
    previousWeight = clampDouble(previousWeight, 0.0, 1.0);
    targetWeight = clampDouble(targetWeight, 0.0, 1.0);
    return previousWeight * (1.0 - smoothFactor) + targetWeight * smoothFactor;
}

bool cpuResizeRgbNearest(const RgbFrame& src, int dstWidth, int dstHeight, RgbFrame* dst) {
    if (dst == nullptr || src.width <= 0 || src.height <= 0 || dstWidth <= 0 || dstHeight <= 0) return false;
    if (src.rgb.size() < static_cast<std::size_t>(src.width) * static_cast<std::size_t>(src.height) * 3) return false;
    dst->width = dstWidth;
    dst->height = dstHeight;
    dst->pts = src.pts;
    dst->rgb.assign(static_cast<std::size_t>(dstWidth) * static_cast<std::size_t>(dstHeight) * 3, 0);
    for (int y = 0; y < dstHeight; ++y) {
        const int sy = std::min(src.height - 1, static_cast<int>((static_cast<long long>(y) * src.height) / dstHeight));
        for (int x = 0; x < dstWidth; ++x) {
            const int sx = std::min(src.width - 1, static_cast<int>((static_cast<long long>(x) * src.width) / dstWidth));
            const std::size_t si = (static_cast<std::size_t>(sy) * src.width + sx) * 3;
            const std::size_t di = (static_cast<std::size_t>(y) * dstWidth + x) * 3;
            dst->rgb[di + 0] = src.rgb[si + 0];
            dst->rgb[di + 1] = src.rgb[si + 1];
            dst->rgb[di + 2] = src.rgb[si + 2];
        }
    }
    return true;
}

bool cropRgb888(const RgbFrame& src, int left, int right, int top, int bottom, RgbFrame* dst) {
    if (dst == nullptr || src.width <= 0 || src.height <= 0) return false;

    left = std::max(0, left);
    right = std::max(0, right);
    top = std::max(0, top);
    bottom = std::max(0, bottom);

    const int cropWidth = src.width - left - right;
    const int cropHeight = src.height - top - bottom;
    if (cropWidth <= 0 || cropHeight <= 0) return false;

    const std::size_t srcSize = static_cast<std::size_t>(src.width) * static_cast<std::size_t>(src.height) * 3;
    if (src.rgb.size() < srcSize) return false;

    dst->width = cropWidth;
    dst->height = cropHeight;
    dst->pts = src.pts;
    dst->rgb.assign(static_cast<std::size_t>(cropWidth) * static_cast<std::size_t>(cropHeight) * 3, 0);

    for (int y = 0; y < cropHeight; ++y) {
        const int srcY = top + y;
        const std::size_t srcOffset =
            (static_cast<std::size_t>(srcY) * static_cast<std::size_t>(src.width) +
             static_cast<std::size_t>(left)) * 3;
        const std::size_t dstOffset =
            static_cast<std::size_t>(y) * static_cast<std::size_t>(cropWidth) * 3;

        std::memcpy(dst->rgb.data() + dstOffset,
                    src.rgb.data() + srcOffset,
                    static_cast<std::size_t>(cropWidth) * 3);
    }

    return true;
}

bool rgaResizeRgb888(const RgbFrame& src, int dstWidth, int dstHeight, RgbFrame* dst) {
    if (dst == nullptr || src.width <= 0 || src.height <= 0 || dstWidth <= 0 || dstHeight <= 0) return false;
    const std::size_t srcSize = static_cast<std::size_t>(src.width) * static_cast<std::size_t>(src.height) * 3;
    if (src.rgb.size() < srcSize) return false;
    dst->width = dstWidth;
    dst->height = dstHeight;
    dst->pts = src.pts;
    dst->rgb.assign(static_cast<std::size_t>(dstWidth) * static_cast<std::size_t>(dstHeight) * 3, 0);
#if TRI_FUSION_APP_HAS_RGA
    rga_buffer_t srcBuf = wrapbuffer_virtualaddr(const_cast<std::uint8_t*>(src.rgb.data()), src.width, src.height,
                                                 RK_FORMAT_RGB_888, src.width, src.height);
    rga_buffer_t dstBuf = wrapbuffer_virtualaddr(dst->rgb.data(), dstWidth, dstHeight,
                                                 RK_FORMAT_RGB_888, dstWidth, dstHeight);
    IM_STATUS ret = imresize(srcBuf, dstBuf);
    if (ret != IM_STATUS_SUCCESS) {
        std::cerr << "[WARN][RGA_RESIZE] imresize RGB888 " << src.width << "x" << src.height
                  << " -> " << dstWidth << "x" << dstHeight << " failed: " << imStrError(ret)
                  << ", fallback to CPU resize\n";
        return cpuResizeRgbNearest(src, dstWidth, dstHeight, dst);
    }
    static bool logged = false;
    if (!logged) {
        std::cerr << "[OK][RGA_RESIZE] RGB888 resize active, " << src.width << "x" << src.height
                  << " -> " << dstWidth << "x" << dstHeight << "\n";
        logged = true;
    }
    return true;
#else
    static bool warned = false;
    if (!warned) {
        std::cerr << "[WARN][RGA_RESIZE] RGA headers not found, using CPU resize fallback\n";
        warned = true;
    }
    return cpuResizeRgbNearest(src, dstWidth, dstHeight, dst);
#endif
}

void fuseRgbSameSizeAdaptive(const RgbFrame& visibleResized,
                             const RgbFrame& composite,
                             double visibleWeight,
                             std::vector<std::uint8_t>* out) {
    if (out == nullptr) return;
    out->clear();
    if (visibleResized.width <= 0 || visibleResized.height <= 0 ||
        visibleResized.width != composite.width || visibleResized.height != composite.height) return;
    const std::size_t pixelCount = static_cast<std::size_t>(composite.width) * static_cast<std::size_t>(composite.height);
    const std::size_t bytes = pixelCount * 3;
    if (visibleResized.rgb.size() < bytes || composite.rgb.size() < bytes) return;
    out->resize(bytes);
    const double visibleRatio = clampDouble(visibleWeight, 0.0, 1.0);
    const double compositeRatio = 1.0 - visibleRatio;
    for (std::size_t i = 0; i < bytes; i += 3) {
        (*out)[i + 0] = clampToByte(static_cast<int>(visibleResized.rgb[i + 0] * visibleRatio + composite.rgb[i + 0] * compositeRatio));
        (*out)[i + 1] = clampToByte(static_cast<int>(visibleResized.rgb[i + 1] * visibleRatio + composite.rgb[i + 1] * compositeRatio));
        (*out)[i + 2] = clampToByte(static_cast<int>(visibleResized.rgb[i + 2] * visibleRatio + composite.rgb[i + 2] * compositeRatio));
    }
}

void fuseRgbShrinkVisibleBlackBorderAdaptive(const RgbFrame& visible,
                                            const RgbFrame& composite,
                                            double visibleWeight,
                                            int visibleOffsetX,
                                            int visibleOffsetY,
                                            int cropLeft,
                                            int cropRight,
                                            int cropTop,
                                            int cropBottom,
                                            std::vector<std::uint8_t>* out) {
    if (out == nullptr) return;
    out->clear();
    if (visible.width <= 0 || visible.height <= 0 || composite.width <= 0 || composite.height <= 0) return;

    const std::size_t compositeBytes = static_cast<std::size_t>(composite.width) *
                                      static_cast<std::size_t>(composite.height) * 3;
    const std::size_t visibleBytes = static_cast<std::size_t>(visible.width) *
                                    static_cast<std::size_t>(visible.height) * 3;
    if (composite.rgb.size() < compositeBytes || visible.rgb.size() < visibleBytes) return;

    cropLeft = std::max(0, cropLeft);
    cropRight = std::max(0, cropRight);
    cropTop = std::max(0, cropTop);
    cropBottom = std::max(0, cropBottom);

    cropLeft = std::min(cropLeft, composite.width - 1);
    cropRight = std::min(cropRight, composite.width - 1);
    cropTop = std::min(cropTop, composite.height - 1);
    cropBottom = std::min(cropBottom, composite.height - 1);

    const int dstW = composite.width - cropLeft - cropRight;
    const int dstH = composite.height - cropTop - cropBottom;
    const int dstX0 = cropLeft + visibleOffsetX;
    const int dstY0 = cropTop + visibleOffsetY;
    const int dstX1 = dstX0 + dstW - 1;
    const int dstY1 = dstY0 + dstH - 1;

    out->resize(compositeBytes);
    const double visibleRatio = clampDouble(visibleWeight, 0.0, 1.0);
    const double compositeRatio = 1.0 - visibleRatio;

    for (int y = 0; y < composite.height; ++y) {
        for (int x = 0; x < composite.width; ++x) {
            const std::size_t ci = (static_cast<std::size_t>(y) * composite.width + x) * 3;
            const int cr = composite.rgb[ci + 0];
            const int cg = composite.rgb[ci + 1];
            const int cb = composite.rgb[ci + 2];

            int vr = 0;
            int vg = 0;
            int vb = 0;

            if (dstW > 0 && dstH > 0 &&
                x >= dstX0 && x <= dstX1 &&
                y >= dstY0 && y <= dstY1) {
                const int localX = x - dstX0;
                const int localY = y - dstY0;

                int vx = static_cast<int>((static_cast<long long>(localX) * visible.width) / dstW);
                int vy = static_cast<int>((static_cast<long long>(localY) * visible.height) / dstH);
                vx = std::max(0, std::min(vx, visible.width - 1));
                vy = std::max(0, std::min(vy, visible.height - 1));

                const std::size_t vi = (static_cast<std::size_t>(vy) * visible.width + vx) * 3;
                vr = visible.rgb[vi + 0];
                vg = visible.rgb[vi + 1];
                vb = visible.rgb[vi + 2];
            }

            (*out)[ci + 0] = clampToByte(static_cast<int>(vr * visibleRatio + cr * compositeRatio));
            (*out)[ci + 1] = clampToByte(static_cast<int>(vg * visibleRatio + cg * compositeRatio));
            (*out)[ci + 2] = clampToByte(static_cast<int>(vb * visibleRatio + cb * compositeRatio));
        }
    }
}

void cpuRgbToNv12(const std::vector<std::uint8_t>& rgb, int width, int height, std::vector<std::uint8_t>* nv12) {
    const std::size_t ySize = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    const std::size_t uvSize = ySize / 2;
    nv12->assign(ySize + uvSize, 0);
    std::uint8_t* yPlane = nv12->data();
    std::uint8_t* uvPlane = nv12->data() + ySize;
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

bool rgaRgbToNv12(const std::vector<std::uint8_t>& rgb, int width, int height, std::vector<std::uint8_t>* nv12) {
    if (nv12 == nullptr) return false;
    if (width <= 0 || height <= 0 || (width % 2) != 0 || (height % 2) != 0) return false;
    const std::size_t expectedRgbSize = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3;
    if (rgb.size() < expectedRgbSize) return false;
    const std::size_t ySize = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    nv12->assign(ySize + ySize / 2, 0);
#if TRI_FUSION_APP_HAS_RGA
    rga_buffer_t src = wrapbuffer_virtualaddr(const_cast<std::uint8_t*>(rgb.data()), width, height,
                                              RK_FORMAT_RGB_888, width, height);
    rga_buffer_t dst = wrapbuffer_virtualaddr(nv12->data(), width, height,
                                              RK_FORMAT_YCbCr_420_SP, width, height);
    IM_STATUS ret = imcvtcolor(src, dst, RK_FORMAT_RGB_888, RK_FORMAT_YCbCr_420_SP);
    if (ret != IM_STATUS_SUCCESS) {
        std::cerr << "[WARN][RGA_CVT] imcvtcolor RGB888->NV12 failed: " << imStrError(ret)
                  << ", fallback to CPU rgbToNv12\n";
        cpuRgbToNv12(rgb, width, height, nv12);
        return true;
    }
    static bool logged = false;
    if (!logged) {
        std::cerr << "[OK][RGA_CVT] RGB888 -> NV12 conversion active, size=" << width << "x" << height << "\n";
        logged = true;
    }
    return true;
#else
    static bool warned = false;
    if (!warned) {
        std::cerr << "[WARN][RGA_CVT] RGA headers not found, using CPU rgbToNv12 fallback\n";
        warned = true;
    }
    cpuRgbToNv12(rgb, width, height, nv12);
    return true;
#endif
}

bool pushNv12Frame(GstElement* appsrc, const std::vector<std::uint8_t>& nv12, GstClockTime pts, GstClockTime duration) {
    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, nv12.size(), nullptr);
    if (buffer == nullptr) return false;
    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
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

class AppFusionPipeline::Impl {
public:
    bool start(const AppFusionOptions& options) {
        if (running_.load()) {
            lastError_ = "AppFusionPipeline is already running";
            return false;
        }
        options_ = options;
        options_.compositeAlpha = clampDouble(options_.compositeAlpha, 0.0, 1.0);

        const auto initialBorder = splitVisibleShrinkPixels(
            options_.visibleCropLeft + options_.visibleCropRight,
            options_.visibleCropTop + options_.visibleCropBottom);
        options_.visibleCropLeft = initialBorder.left;
        options_.visibleCropRight = initialBorder.right;
        options_.visibleCropTop = initialBorder.top;
        options_.visibleCropBottom = initialBorder.bottom;

        visiblePositionOffsetX_.store(options_.visibleOffsetX);
        visiblePositionOffsetY_.store(options_.visibleOffsetY);
        visibleCropLeft_.store(options_.visibleCropLeft);
        visibleCropRight_.store(options_.visibleCropRight);
        visibleCropTop_.store(options_.visibleCropTop);
        visibleCropBottom_.store(options_.visibleCropBottom);
        if (options_.fps <= 0 || options_.visibleWidth <= 0 || options_.visibleHeight <= 0 ||
            options_.compositeWidth <= 0 || options_.compositeHeight <= 0 ||
            (options_.compositeWidth % 2) != 0 || (options_.compositeHeight % 2) != 0) {
            lastError_ = "invalid app fusion video size or fps";
            return false;
        }

        static std::once_flag gstInitFlag;
        std::call_once(gstInitFlag, []() { gst_init(nullptr, nullptr); });

        stopRequested_.store(false);
        failed_.store(false);
        lastError_.clear();
        resetStore(&visibleStore_);
        resetStore(&compositeStore_);
        gpuFusionReady_ = false;
        gpuFusionFrames_ = 0;
        fallbackFusionFrames_ = 0;
        gpuFusionTotalMs_ = 0.0;
        fallbackFusionTotalMs_ = 0.0;
        totalFusionTotalMs_ = 0.0;
        pushNv12TotalMs_ = 0.0;

        visibleDesc_ = buildVisiblePipelineDesc(options_);
        compositeDesc_ = buildCompositePipelineDesc(options_);
        encodeDesc_ = buildEncodePipelineDesc(options_);

        visiblePipeline_ = parsePipelineOrNull("VISIBLE_IN", visibleDesc_, &lastError_);
        compositePipeline_ = parsePipelineOrNull("COMPOSITE_IN", compositeDesc_, &lastError_);
        encodePipeline_ = parsePipelineOrNull("FUSION_ENC", encodeDesc_, &lastError_);
        if (!visiblePipeline_ || !compositePipeline_ || !encodePipeline_) {
            cleanupPipelines();
            return false;
        }

        visibleSink_ = gst_bin_get_by_name(GST_BIN(visiblePipeline_), "visible_sink");
        compositeSink_ = gst_bin_get_by_name(GST_BIN(compositePipeline_), "composite_sink");
        appsrc_ = gst_bin_get_by_name(GST_BIN(encodePipeline_), "fusion_src");
        if (!visibleSink_ || !compositeSink_ || !appsrc_) {
            lastError_ = "failed to find visible_sink/composite_sink/fusion_src";
            cleanupObjects();
            cleanupPipelines();
            return false;
        }

        std::cerr << "========== Threaded RGB App Fusion Pipeline ==========" << "\n";
        std::cerr << "[CONFIG] visible=" << options_.visibleDevice << " MJPG "
                  << options_.visibleWidth << "x" << options_.visibleHeight << "@" << options_.fps << "\n";
        std::cerr << "[CONFIG] composite=" << options_.compositeDevice << " YUY2 "
                  << options_.compositeWidth << "x" << options_.compositeHeight << "@" << options_.fps << "\n";
        std::cerr << "[CONFIG] output=NV12 " << options_.compositeWidth << "x" << options_.compositeHeight
                  << " compute=OpenCL(optional resize+fusion+RGB2NV12) fallback=RGA/CPU "
                  << " -> mpph264enc -> udp "
                  << options_.udpHost << ":" << options_.udpPort << "\n";
        std::cerr << "[CONFIG] visible_crop=L" << options_.visibleCropLeft
                  << " R" << options_.visibleCropRight
                  << " T" << options_.visibleCropTop
                  << " B" << options_.visibleCropBottom
                  << " mode=shrink-black-border"
                  << " inner="
                  << (options_.visibleWidth - options_.visibleCropLeft - options_.visibleCropRight)
                  << "x"
                  << (options_.visibleHeight - options_.visibleCropTop - options_.visibleCropBottom)
                  << " black_border=enabled\n";
        std::cerr << "[CONFIG] low_latency: appsink max-buffers=1 drop=true, appsrc block=false do-timestamp=true\n";

        gpuFusion_ = std::make_unique<OpenClFusionBackend>();
        std::string gpuError;
        gpuFusionReady_ = gpuFusion_->init(options_.visibleWidth,
                                           options_.visibleHeight,
                                           options_.compositeWidth,
                                           options_.compositeHeight,
                                           &gpuError);
        if (gpuFusionReady_) {
            std::cerr << "[OK][GPU_FUSION] OpenCL backend active: " << gpuFusion_->description() << "\n";
        } else {
            std::cerr << "[WARN][GPU_FUSION] OpenCL init failed, fallback to RGA/CPU: " << gpuError << "\n";
        }

        if (!setPlaying("FUSION_ENC", encodePipeline_, &lastError_) ||
            !setPlaying("VISIBLE_IN", visiblePipeline_, &lastError_) ||
            !setPlaying("COMPOSITE_IN", compositePipeline_, &lastError_)) {
            cleanupObjects();
            cleanupPipelines();
            return false;
        }

        running_.store(true);
        visibleThread_ = std::thread(&Impl::captureThreadMain, this, "VISIBLE_IN", visiblePipeline_, visibleSink_, &visibleStore_);
        compositeThread_ = std::thread(&Impl::captureThreadMain, this, "COMPOSITE_IN", compositePipeline_, compositeSink_, &compositeStore_);
        fusionThread_ = std::thread(&Impl::fusionThreadMain, this);
        return true;
    }

    bool stop() {
        stopRequested_.store(true);
        if (visibleThread_.joinable()) visibleThread_.join();
        if (compositeThread_.joinable()) compositeThread_.join();
        if (fusionThread_.joinable()) fusionThread_.join();
        cleanupObjects();
        cleanupPipelines();
        gpuFusionReady_ = false;
        gpuFusion_.reset();
        running_.store(false);
        return true;
    }

    bool isRunning() const { return running_.load() && !failed_.load(); }

    bool setVisiblePositionOffset(int offsetX, int offsetY) {
        visiblePositionOffsetX_.store(offsetX);
        visiblePositionOffsetY_.store(offsetY);
        std::cerr << "[CONTROL][FUSION] visible_position offset_x=" << offsetX
                  << " offset_y=" << offsetY << "\n";
        return true;
    }

    bool moveVisiblePositionOffset(int deltaX, int deltaY) {
        const int newX = visiblePositionOffsetX_.load() + deltaX;
        const int newY = visiblePositionOffsetY_.load() + deltaY;
        return setVisiblePositionOffset(newX, newY);
    }

    std::pair<int, int> visiblePositionOffset() const {
        return {visiblePositionOffsetX_.load(), visiblePositionOffsetY_.load()};
    }

    bool setVisibleShrinkPixels(int horizontalPixels, int verticalPixels) {
        if (horizontalPixels < 0 || verticalPixels < 0) {
            lastError_ = "visible shrink pixels must be non-negative";
            return false;
        }
        if (options_.compositeWidth > 0 && horizontalPixels >= options_.compositeWidth) {
            lastError_ = "visible horizontal shrink must be smaller than output width";
            return false;
        }
        if (options_.compositeHeight > 0 && verticalPixels >= options_.compositeHeight) {
            lastError_ = "visible vertical shrink must be smaller than output height";
            return false;
        }

        const auto border = splitVisibleShrinkPixels(horizontalPixels, verticalPixels);
        visibleCropLeft_.store(border.left);
        visibleCropRight_.store(border.right);
        visibleCropTop_.store(border.top);
        visibleCropBottom_.store(border.bottom);

        std::cerr << "[CONTROL][FUSION] visible_shrink horizontal=" << horizontalPixels
                  << " vertical=" << verticalPixels
                  << " border=L" << border.left
                  << " R" << border.right
                  << " T" << border.top
                  << " B" << border.bottom
                  << " inner=" << (options_.compositeWidth - border.left - border.right)
                  << "x" << (options_.compositeHeight - border.top - border.bottom)
                  << " mode=shrink-black-border\n";
        return true;
    }

    std::pair<int, int> visibleShrinkPixels() const {
        return {visibleCropLeft_.load() + visibleCropRight_.load(),
                visibleCropTop_.load() + visibleCropBottom_.load()};
    }

    std::string lastError() const { return lastError_; }

    std::string description() const {
        std::ostringstream ss;
        ss << "ThreadedAppFusion visible=" << options_.visibleDevice << " "
           << options_.visibleWidth << "x" << options_.visibleHeight
           << " composite=" << options_.compositeDevice << " "
           << options_.compositeWidth << "x" << options_.compositeHeight
           << " udp=" << options_.udpHost << ":" << options_.udpPort;
        return ss.str();
    }

private:
    void markFailed(const std::string& error) {
        lastError_ = error;
        failed_.store(true);
        stopRequested_.store(true);
    }

    void resetStore(LatestFrameStore* store) {
        if (store == nullptr) return;
        std::lock_guard<std::mutex> lock(store->mutex);
        store->frame = RgbFrame{};
        store->seq = 0;
        store->hasFrame = false;
    }

    void captureThreadMain(const std::string& name, GstElement* pipeline, GstElement* sink, LatestFrameStore* store) {
        std::uint64_t captured = 0;
        std::uint64_t timeouts = 0;
        while (!stopRequested_.load()) {
            if (!checkBus(name, pipeline, &lastError_)) {
                failed_.store(true);
                stopRequested_.store(true);
                break;
            }
            RgbFrame frame;
            if (!pullRgbFrame(sink, name, &frame, 100)) {
                ++timeouts;
                if ((timeouts % 100) == 0) {
                    std::cerr << "[WARN][" << name << "] appsink timeout count=" << timeouts << "\n";
                }
                continue;
            }
            storeLatestFrame(store, std::move(frame));
            ++captured;
            if ((captured % 150) == 0) {
                std::cerr << "[CAPTURE][" << name << "] captured=" << captured << "\n";
            }
        }
        std::cerr << "[CAPTURE][" << name << "] exit captured=" << captured << " timeouts=" << timeouts << "\n";
    }

    bool waitForInitialFrames(int timeoutMs) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (!stopRequested_.load() && std::chrono::steady_clock::now() < deadline) {
            bool visibleReady = false;
            bool compositeReady = false;
            {
                std::lock_guard<std::mutex> lock(visibleStore_.mutex);
                visibleReady = visibleStore_.hasFrame;
            }
            {
                std::lock_guard<std::mutex> lock(compositeStore_.mutex);
                compositeReady = compositeStore_.hasFrame;
            }
            if (visibleReady && compositeReady) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }

    bool fallbackRgaCpuFuseToNv12(const RgbFrame& visible,
                                  const RgbFrame& composite,
                                  double visibleWeight,
                                  RgbFrame* resizedVisible,
                                  std::vector<std::uint8_t>* fusedRgb,
                                  std::vector<std::uint8_t>* fusedNv12) {
        (void)resizedVisible;
        if (fusedRgb == nullptr || fusedNv12 == nullptr) return false;

        // Shrink-and-black-border mode:
        // compress the full visible frame into the inner valid area, for example
        // 800x600 -> 792x594 at x=4..795/y=3..596.
        // The border area is filled with black visible pixels before normal fusion.
        fuseRgbShrinkVisibleBlackBorderAdaptive(visible,
                                          composite,
                                          visibleWeight,
                                          visiblePositionOffsetX_.load(),
                                          visiblePositionOffsetY_.load(),
                                          visibleCropLeft_.load(),
                                          visibleCropRight_.load(),
                                          visibleCropTop_.load(),
                                          visibleCropBottom_.load(),
                                          fusedRgb);
        if (fusedRgb->empty()) {
            std::cerr << "[ERROR][FUSION] fused RGB is empty"
                      << " visible=" << visible.width << "x" << visible.height
                      << " composite=" << composite.width << "x" << composite.height
                      << " shrink_border=L" << visibleCropLeft_.load()
                      << " R" << visibleCropRight_.load()
                      << " T" << visibleCropTop_.load()
                      << " B" << visibleCropBottom_.load() << "\n";
            return false;
        }
        if (!rgaRgbToNv12(*fusedRgb, composite.width, composite.height, fusedNv12)) {
            std::cerr << "[ERROR][FUSION] RGB->NV12 failed\n";
            return false;
        }
        return true;
    }

    void fusionThreadMain() {
        if (!waitForInitialFrames(5000)) {
            markFailed("timeout waiting for initial visible/composite frames");
            return;
        }

        const GstClockTime frameDuration = gst_util_uint64_scale_int(1, GST_SECOND, options_.fps);
        GstClockTime outPts = 0;
        std::uint64_t fusedFrames = 0;
        std::uint64_t repeatedVisible = 0;
        std::uint64_t repeatedComposite = 0;
        std::uint64_t syncWarnCount = 0;
        std::uint64_t lastVisibleSeq = 0;
        std::uint64_t lastCompositeSeq = 0;
        bool haveLastSeq = false;
        auto startTime = std::chrono::steady_clock::now();
        auto nextTick = std::chrono::steady_clock::now();

        LightSceneState lightState;
        double visibleWeight = 1.0 - options_.compositeAlpha;
        const double maxVisibleWeight = 1.0 - options_.compositeAlpha;

        RgbFrame visible;
        RgbFrame composite;
        RgbFrame resizedVisible;
        std::vector<std::uint8_t> fusedRgb;
        std::vector<std::uint8_t> fusedNv12;

        while (!stopRequested_.load()) {
            if (!checkBus("FUSION_ENC", encodePipeline_, &lastError_)) {
                failed_.store(true);
                stopRequested_.store(true);
                break;
            }

            std::uint64_t visibleSeq = 0;
            std::uint64_t compositeSeq = 0;
            if (!loadLatestFrame(&visibleStore_, &visible, &visibleSeq) ||
                !loadLatestFrame(&compositeStore_, &composite, &compositeSeq)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            if (haveLastSeq) {
                if (visibleSeq == lastVisibleSeq) ++repeatedVisible;
                if (compositeSeq == lastCompositeSeq) ++repeatedComposite;
            }
            haveLastSeq = true;
            lastVisibleSeq = visibleSeq;
            lastCompositeSeq = compositeSeq;

            gint64 rawDiffMs = 0;
            if (visible.pts != GST_CLOCK_TIME_NONE && composite.pts != GST_CLOCK_TIME_NONE) {
                rawDiffMs = (static_cast<gint64>(visible.pts) - static_cast<gint64>(composite.pts)) /
                            static_cast<gint64>(GST_MSECOND);
                if (std::llabs(rawDiffMs) > 120) {
                    ++syncWarnCount;
                    if ((syncWarnCount % 30) == 1) {
                        std::cerr << "[SYNC][WARN] raw_diff_ms=" << rawDiffMs
                                  << " keep_output=true sync_warn_count=" << syncWarnCount << "\n";
                    }
                }
            }

            const LightStats stats = computeLightStatsRgb(visible);
            const LightScene candidate = classifyLightSceneCandidate(stats);
            const auto now = std::chrono::steady_clock::now();
            updateLightSceneState(&lightState, candidate, now);
            const double targetVisibleWeight = computeTargetVisibleWeight(stats, lightState.current, maxVisibleWeight);
            visibleWeight = smoothVisibleWeight(visibleWeight, targetVisibleWeight);

            bool usedGpuFusion = false;
            const auto fusionBegin = std::chrono::steady_clock::now();

            if (gpuFusionReady_ && gpuFusion_) {
                GpuFusionParams gpuParams;
                gpuParams.outputWidth = composite.width;
                gpuParams.outputHeight = composite.height;
                gpuParams.visibleWeight = visibleWeight;
                gpuParams.visibleOffsetX = visiblePositionOffsetX_.load();
                gpuParams.visibleOffsetY = visiblePositionOffsetY_.load();
                gpuParams.visibleCropLeft = visibleCropLeft_.load();
                gpuParams.visibleCropRight = visibleCropRight_.load();
                gpuParams.visibleCropTop = visibleCropTop_.load();
                gpuParams.visibleCropBottom = visibleCropBottom_.load();
                gpuParams.enableBilinearResize = options_.gpuBilinearResize;

                std::string gpuError;
                const auto gpuBegin = std::chrono::steady_clock::now();
                if (gpuFusion_->fuseToNv12(visible, composite, gpuParams, &fusedNv12, &gpuError)) {
                    gpuFusionTotalMs_ += elapsedMs(gpuBegin);
                    ++gpuFusionFrames_;
                    usedGpuFusion = true;
                } else {
                    std::cerr << "[WARN][GPU_FUSION] frame failed, fallback to RGA/CPU and disable GPU path: "
                              << gpuError << "\n";
                    gpuFusionReady_ = false;
                }
            }

            if (!usedGpuFusion) {
                const auto fallbackBegin = std::chrono::steady_clock::now();
                if (!fallbackRgaCpuFuseToNv12(visible, composite, visibleWeight, &resizedVisible, &fusedRgb, &fusedNv12)) {
                    continue;
                }
                fallbackFusionTotalMs_ += elapsedMs(fallbackBegin);
                ++fallbackFusionFrames_;
            }

            totalFusionTotalMs_ += elapsedMs(fusionBegin);

            const auto pushBegin = std::chrono::steady_clock::now();
            if (!pushNv12Frame(appsrc_, fusedNv12, outPts, frameDuration)) {
                markFailed("push fused NV12 frame failed");
                break;
            }
            pushNv12TotalMs_ += elapsedMs(pushBegin);
            outPts += frameDuration;
            ++fusedFrames;

            if ((fusedFrames % static_cast<std::uint64_t>(options_.fps)) == 0) {
                const double compositeWeight = 1.0 - visibleWeight;
                std::cerr << std::fixed << std::setprecision(2)
                          << "[LIGHT] frame=" << fusedFrames
                          << " meanLuma=" << stats.meanLuma
                          << " darkPixelRatio=" << stats.darkPixelRatio
                          << " brightPixelRatio=" << stats.brightPixelRatio
                          << " scene=" << lightSceneName(lightState.current)
                          << " candidate=" << lightSceneName(candidate)
                          << " targetVisibleWeight=" << targetVisibleWeight
                          << " visibleWeight=" << visibleWeight
                          << " compositeWeight=" << compositeWeight
                          << " visible_seq=" << visibleSeq
                          << " composite_seq=" << compositeSeq
                          << " raw_diff_ms=" << rawDiffMs
                          << " visible_position_offset=" << visiblePositionOffsetX_.load()
                          << "," << visiblePositionOffsetY_.load()
                          << " visible_shrink=" << (visibleCropLeft_.load() + visibleCropRight_.load())
                          << "," << (visibleCropTop_.load() + visibleCropBottom_.load())
                          << " border=L" << visibleCropLeft_.load()
                          << " R" << visibleCropRight_.load()
                          << " T" << visibleCropTop_.load()
                          << " B" << visibleCropBottom_.load()
                          << " output=" << composite.width << "x" << composite.height << "\n";
            }
            if ((fusedFrames % (static_cast<std::uint64_t>(options_.fps) * 5)) == 0) {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime).count();
                const double avgFps = elapsed > 0 ? (static_cast<double>(fusedFrames) * 1000.0 / elapsed) : 0.0;
                std::cerr << std::fixed << std::setprecision(2)
                          << "[STATS] fused_frames=" << fusedFrames
                          << " avg_fps=" << avgFps
                          << " repeated_visible=" << repeatedVisible
                          << " repeated_composite=" << repeatedComposite
                          << " sync_warn_count=" << syncWarnCount << "\n";
            }

            if ((fusedFrames % 150) == 0) {
                const double avgGpuMs = gpuFusionFrames_ > 0
                    ? gpuFusionTotalMs_ / static_cast<double>(gpuFusionFrames_) : 0.0;
                const double avgFallbackMs = fallbackFusionFrames_ > 0
                    ? fallbackFusionTotalMs_ / static_cast<double>(fallbackFusionFrames_) : 0.0;
                const double avgTotalFusionMs = fusedFrames > 0
                    ? totalFusionTotalMs_ / static_cast<double>(fusedFrames) : 0.0;
                const double avgPushMs = fusedFrames > 0
                    ? pushNv12TotalMs_ / static_cast<double>(fusedFrames) : 0.0;
                std::cerr << std::fixed << std::setprecision(2)
                          << "[GPU_FUSION][STATS] frames=" << fusedFrames
                          << " gpu_frames=" << gpuFusionFrames_
                          << " fallback_frames=" << fallbackFusionFrames_
                          << " avg_gpu_ms=" << avgGpuMs
                          << " avg_fallback_ms=" << avgFallbackMs
                          << " avg_total_fusion_ms=" << avgTotalFusionMs
                          << " avg_push_ms=" << avgPushMs
                          << " output=" << composite.width << "x" << composite.height << "\n";
            }

            nextTick += std::chrono::nanoseconds(static_cast<long long>(frameDuration));
            std::this_thread::sleep_until(nextTick);
            const auto current = std::chrono::steady_clock::now();
            if (nextTick < current - std::chrono::milliseconds(200)) nextTick = current;
        }
        std::cerr << "[FUSION] exit\n";
    }

    void cleanupObjects() {
        if (visibleSink_) { gst_object_unref(visibleSink_); visibleSink_ = nullptr; }
        if (compositeSink_) { gst_object_unref(compositeSink_); compositeSink_ = nullptr; }
        if (appsrc_) { gst_object_unref(appsrc_); appsrc_ = nullptr; }
    }

    void cleanupPipelines() {
        setNullAndUnref("VISIBLE_IN", visiblePipeline_);
        setNullAndUnref("COMPOSITE_IN", compositePipeline_);
        setNullAndUnref("FUSION_ENC", encodePipeline_);
    }

private:
    AppFusionOptions options_;
    std::atomic_bool running_{false};
    std::atomic_bool stopRequested_{false};
    std::atomic_bool failed_{false};
    std::string lastError_;

    std::string visibleDesc_;
    std::string compositeDesc_;
    std::string encodeDesc_;

    GstElement* visiblePipeline_ = nullptr;
    GstElement* compositePipeline_ = nullptr;
    GstElement* encodePipeline_ = nullptr;
    GstElement* visibleSink_ = nullptr;
    GstElement* compositeSink_ = nullptr;
    GstElement* appsrc_ = nullptr;

    std::unique_ptr<OpenClFusionBackend> gpuFusion_;
    bool gpuFusionReady_ = false;
    std::uint64_t gpuFusionFrames_ = 0;
    std::uint64_t fallbackFusionFrames_ = 0;
    double gpuFusionTotalMs_ = 0.0;
    double fallbackFusionTotalMs_ = 0.0;
    double totalFusionTotalMs_ = 0.0;
    double pushNv12TotalMs_ = 0.0;

    std::atomic_int visiblePositionOffsetX_{0};
    std::atomic_int visiblePositionOffsetY_{0};
    std::atomic_int visibleCropLeft_{4};
    std::atomic_int visibleCropRight_{4};
    std::atomic_int visibleCropTop_{3};
    std::atomic_int visibleCropBottom_{3};

    LatestFrameStore visibleStore_;
    LatestFrameStore compositeStore_;
    std::thread visibleThread_;
    std::thread compositeThread_;
    std::thread fusionThread_;
};

AppFusionPipeline::AppFusionPipeline() : impl_(std::make_unique<Impl>()) {}
AppFusionPipeline::~AppFusionPipeline() { stop(); }
bool AppFusionPipeline::start(const AppFusionOptions& options) { return impl_->start(options); }
bool AppFusionPipeline::stop() { return impl_->stop(); }
bool AppFusionPipeline::isRunning() const { return impl_->isRunning(); }
bool AppFusionPipeline::setVisiblePositionOffset(int offsetX, int offsetY) {
    return impl_->setVisiblePositionOffset(offsetX, offsetY);
}
bool AppFusionPipeline::moveVisiblePositionOffset(int deltaX, int deltaY) {
    return impl_->moveVisiblePositionOffset(deltaX, deltaY);
}
std::pair<int, int> AppFusionPipeline::visiblePositionOffset() const {
    return impl_->visiblePositionOffset();
}
bool AppFusionPipeline::setVisibleShrinkPixels(int horizontalPixels, int verticalPixels) {
    return impl_->setVisibleShrinkPixels(horizontalPixels, verticalPixels);
}
std::pair<int, int> AppFusionPipeline::visibleShrinkPixels() const {
    return impl_->visibleShrinkPixels();
}
std::string AppFusionPipeline::lastError() const { return impl_->lastError(); }
std::string AppFusionPipeline::description() const { return impl_->description(); }

} // namespace tri::media::app_fusion