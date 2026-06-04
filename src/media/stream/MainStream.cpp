#include "media/stream/MainStream.h"
#include <chrono>

namespace tri::media {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

namespace {
hardware::mpp::MppCodec codecFromString(const std::string& codec) {
    if (codec == "h265" || codec == "H265" || codec == "hevc" || codec == "HEVC") {
        return hardware::mpp::MppCodec::H265;
    }
    if (codec == "mjpeg" || codec == "MJPEG") {
        return hardware::mpp::MppCodec::Mjpeg;
    }
    return hardware::mpp::MppCodec::H264;
}
}

Result<void> MainStream::init(const foundation::MediaConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    profile_.codec = codecFromString(config.codec);
    profile_.width = static_cast<std::uint32_t>(config.width);
    profile_.height = static_cast<std::uint32_t>(config.height);
    profile_.fps = static_cast<std::uint32_t>(config.fps);
    profile_.bitrateKbps = static_cast<std::uint32_t>(config.bitrate);
    profile_.gop = static_cast<std::uint32_t>(config.gop);
    profile_.lowLatency = config.lowLatency;

    /*
     * GStreamer 等效设计：
     * queue leaky=downstream max-size-buffers=1
     *
     * 低延时模式下，MainStream 只保留最新 1 帧。
     * 下游 UDP/RTSP/GB28181 如果短暂卡顿，不允许旧帧堆积。
     */
    if (profile_.lowLatency) {
        capacity_ = 1;
    } else {
        capacity_ = profile_.fps == 0 ? 30 : profile_.fps;
    }

    stats_ = StreamStats{};
    frames_.clear();
    state_ = StreamState::Stopped;

    return Result<void>::success();
}

void MainStream::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = StreamState::Running;
}

void MainStream::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = StreamState::Stopped;
    cv_.notify_all();
}

void MainStream::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    frames_.clear();
}

Result<void> MainStream::push(EncodedFrame frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != StreamState::Running) {
        return Result<void>::error(ErrorCode::MainStreamNotReady,
                                   "main stream is not running");
    }

    /*
     * leaky downstream:
     * 队列满时丢旧帧，保留即将写入的新帧。
     */
    while (frames_.size() >= capacity_) {
        frames_.pop_front();
        ++stats_.framesDropped;
    }

    stats_.lastPtsMs = frame.ptsMs;
    ++stats_.framesIn;

    frames_.push_back(std::move(frame));
    cv_.notify_all();

    return Result<void>::success();
}

std::optional<EncodedFrame> MainStream::waitFrame(int timeoutMs) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (timeoutMs < 0) {
        cv_.wait(lock, [&] {
            return state_ != StreamState::Running || !frames_.empty();
        });
    } else {
        cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] {
            return state_ != StreamState::Running || !frames_.empty();
        });
    }

    if (frames_.empty()) {
        return std::nullopt;
    }

    /*
     * 保险策略：
     * 即使非低延时模式下曾经积压多帧，低延时模式也只取最新帧。
     */
    if (profile_.lowLatency && frames_.size() > 1) {
        const auto dropped = frames_.size() - 1;
        while (frames_.size() > 1) {
            frames_.pop_front();
        }
        stats_.framesDropped += dropped;
    }

    auto frame = frames_.front();
    frames_.pop_front();

    return frame;
}

std::optional<EncodedFrame> MainStream::latestFrame() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (frames_.empty()) {
        return std::nullopt;
    }

    return frames_.back();
}

} // namespace tri::media
