#!/usr/bin/env bash
set -e

H="src/protocol/udp/UdpMpegTsPublisher.h"
CPP="src/protocol/udp/UdpMpegTsPublisher.cpp"
TS="$(date +%Y%m%d_%H%M%S)"

if [ ! -f "$H" ] || [ ! -f "$CPP" ]; then
    echo "[ERROR] missing UdpMpegTsPublisher files"
    exit 1
fi

cp "$H" "${H}.bak_ffmpeg_h264_diag_${TS}"
cp "$CPP" "${CPP}.bak_ffmpeg_h264_diag_${TS}"

echo "[BACKUP] $H -> ${H}.bak_ffmpeg_h264_diag_${TS}"
echo "[BACKUP] $CPP -> ${CPP}.bak_ffmpeg_h264_diag_${TS}"

cat > "$H" <<'HPP'
#pragma once

#include "foundation/error/Result.h"
#include "service/StreamService.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace tri::protocol::udp {

enum class FfmpegPipeInputFormat {
    Mjpeg,
    RawYuyv422,
    H264,
};

struct UdpMpegTsPublisherConfig {
    std::string udpUrl{"udp://192.168.1.35:5004?pkt_size=1316"};

    // 当前 L4/L8 联调链路里 MainStream 已经是 MPP H264。
    // 保留这个字段兼容测试程序，但实际 ffmpeg 输入按 H264 处理。
    FfmpegPipeInputFormat inputFormat{FfmpegPipeInputFormat::H264};

    std::uint32_t width{800};
    std::uint32_t height{600};
    std::uint32_t fps{30};

    std::string encoder{"copy"};
    std::string preset{"ultrafast"};
    std::string tune{"zerolatency"};

    std::string ffmpegPath{"ffmpeg"};
    std::string logLevel{"info"};
};

struct UdpMpegTsPublisherStats {
    std::uint64_t framesPulled{0};
    std::uint64_t framesWritten{0};
    std::uint64_t bytesWritten{0};
    std::uint64_t writeErrors{0};
};

class UdpMpegTsPublisher final {
public:
    UdpMpegTsPublisher() = default;
    ~UdpMpegTsPublisher();

    UdpMpegTsPublisher(const UdpMpegTsPublisher&) = delete;
    UdpMpegTsPublisher& operator=(const UdpMpegTsPublisher&) = delete;

    tri::foundation::Result<void> start(const UdpMpegTsPublisherConfig& config,
                                        tri::service::StreamService* streamService);

    void stop();

    bool running() const noexcept { return running_; }
    UdpMpegTsPublisherStats stats() const noexcept { return stats_; }

private:
    tri::foundation::Result<void> startFfmpeg();
    std::vector<std::string> buildFfmpegArgs() const;

    void senderLoop();

    bool writeFrameToFfmpeg(const std::vector<std::uint8_t>& bytes);
    bool writeAll(const std::uint8_t* data, std::size_t size);

    void closeWritePipe();
    void waitChildIfExited();

private:
    UdpMpegTsPublisherConfig config_{};
    tri::service::StreamService* streamService_{nullptr};

    std::atomic<bool> running_{false};
    std::thread senderThread_;

    int writeFd_{-1};
    int childPid_{-1};

    UdpMpegTsPublisherStats stats_{};
};

} // namespace tri::protocol::udp
HPP

cat > "$CPP" <<'CPP'
#include "protocol/udp/UdpMpegTsPublisher.h"

#include "foundation/error/ErrorCode.h"
#include "foundation/log/Logger.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace tri::protocol::udp {

using tri::foundation::ErrorCode;
using tri::foundation::LogCategory;
using tri::foundation::Result;

namespace {

std::string joinArgs(const std::vector<std::string>& args) {
    std::ostringstream oss;
    for (const auto& arg : args) {
        if (!oss.str().empty()) {
            oss << ' ';
        }
        oss << arg;
    }
    return oss.str();
}

} // namespace

UdpMpegTsPublisher::~UdpMpegTsPublisher() {
    stop();
}

Result<void> UdpMpegTsPublisher::start(const UdpMpegTsPublisherConfig& config,
                                       tri::service::StreamService* streamService) {
    if (running_) {
        return Result<void>::success();
    }

    if (streamService == nullptr) {
        return Result<void>::error(ErrorCode::InvalidArgument,
                                   "UdpMpegTsPublisher streamService is null");
    }

    if (config.udpUrl.empty()) {
        return Result<void>::error(ErrorCode::InvalidArgument,
                                   "UdpMpegTsPublisher udpUrl is empty");
    }

    ::signal(SIGPIPE, SIG_IGN);

    config_ = config;
    streamService_ = streamService;
    stats_ = UdpMpegTsPublisherStats{};

    auto ret = startFfmpeg();
    if (!ret) {
        return ret;
    }

    running_ = true;
    senderThread_ = std::thread(&UdpMpegTsPublisher::senderLoop, this);

    TRI_LOG_INFO(LogCategory::System)
        << "UDP MPEG-TS publisher started, url=" << config_.udpUrl;

    return Result<void>::success();
}

void UdpMpegTsPublisher::stop() {
    running_ = false;

    if (senderThread_.joinable()) {
        senderThread_.join();
    }

    closeWritePipe();
    waitChildIfExited();

    streamService_ = nullptr;
}

std::vector<std::string> UdpMpegTsPublisher::buildFfmpegArgs() const {
    std::vector<std::string> args;

    args.push_back(config_.ffmpegPath.empty() ? "ffmpeg" : config_.ffmpegPath);

    args.push_back("-hide_banner");
    args.push_back("-loglevel");
    args.push_back(config_.logLevel.empty() ? "info" : config_.logLevel);

    args.push_back("-fflags");
    args.push_back("nobuffer");

    args.push_back("-flags");
    args.push_back("low_delay");

    // 当前正确链路：
    // UVC YUYV422 -> RGA NV12 -> MPP H264 -> MainStream -> ffmpeg stdin
    // 所以这里必须按 H264 裸流输入，而不是 mjpeg/rawvideo。
    args.push_back("-f");
    args.push_back("h264");

    args.push_back("-i");
    args.push_back("pipe:0");

    // 不再让 ffmpeg 编码，只做 H264 -> MPEG-TS 封装 -> UDP。
    args.push_back("-c:v");
    args.push_back("copy");

    args.push_back("-f");
    args.push_back("mpegts");

    args.push_back(config_.udpUrl);

    return args;
}

Result<void> UdpMpegTsPublisher::startFfmpeg() {
    int pipefd[2] = {-1, -1};

    if (::pipe(pipefd) != 0) {
        return Result<void>::error(ErrorCode::IoError,
                                   std::string("pipe failed: ") + std::strerror(errno));
    }

    const auto args = buildFfmpegArgs();

    TRI_LOG_INFO(LogCategory::System)
        << "ffmpeg command: " << joinArgs(args)
        << ", stderr=/tmp/tri_fusion_ffmpeg_stderr.log";

    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        return Result<void>::error(ErrorCode::IoError,
                                   std::string("fork ffmpeg failed: ") + std::strerror(errno));
    }

    if (pid == 0) {
        ::close(pipefd[1]);

        ::dup2(pipefd[0], STDIN_FILENO);
        ::close(pipefd[0]);

        int logFd = ::open("/tmp/tri_fusion_ffmpeg_stderr.log",
                           O_CREAT | O_WRONLY | O_TRUNC,
                           0644);
        if (logFd >= 0) {
            ::dup2(logFd, STDERR_FILENO);
            ::close(logFd);
        }

        int nullFd = ::open("/dev/null", O_WRONLY);
        if (nullFd >= 0) {
            ::dup2(nullFd, STDOUT_FILENO);
            ::close(nullFd);
        }

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        ::execvp(argv[0], argv.data());

        // exec 失败时会写到 /tmp/tri_fusion_ffmpeg_stderr.log
        ::dprintf(STDERR_FILENO,
                  "execvp(%s) failed: %s\n",
                  argv[0],
                  std::strerror(errno));
        _exit(127);
    }

    ::close(pipefd[0]);

    childPid_ = static_cast<int>(pid);
    writeFd_ = pipefd[1];

    // 立即检查一次：如果 ffmpeg 启动即退出，这里能捕获到退出码。
    ::usleep(100 * 1000);
    int status = 0;
    const pid_t r = ::waitpid(pid, &status, WNOHANG);
    if (r == pid) {
        closeWritePipe();
        childPid_ = -1;

        std::ostringstream oss;
        oss << "ffmpeg exited immediately, status=" << status
            << ", see /tmp/tri_fusion_ffmpeg_stderr.log";
        return Result<void>::error(ErrorCode::IoError, oss.str());
    }

    return Result<void>::success();
}

void UdpMpegTsPublisher::senderLoop() {
    while (running_) {
        if (streamService_ == nullptr) {
            break;
        }

        waitChildIfExited();
        if (childPid_ < 0) {
            ++stats_.writeErrors;
            break;
        }

        auto frame = streamService_->waitFrame(1000);
        if (!frame) {
            continue;
        }

        ++stats_.framesPulled;

        const auto& bytes = frame.value().data;
        if (bytes.empty()) {
            continue;
        }

        if (!writeFrameToFfmpeg(bytes)) {
            ++stats_.writeErrors;
            break;
        }

        ++stats_.framesWritten;
        stats_.bytesWritten += bytes.size();

        if ((stats_.framesWritten % 100) == 0) {
            TRI_LOG_INFO(LogCategory::System)
                << "UDP MPEG-TS pushed frames=" << stats_.framesWritten
                << " bytes=" << stats_.bytesWritten;
        }
    }

    running_ = false;
}

bool UdpMpegTsPublisher::writeFrameToFfmpeg(const std::vector<std::uint8_t>& bytes) {
    if (writeFd_ < 0) {
        return false;
    }

    return writeAll(bytes.data(), bytes.size());
}

bool UdpMpegTsPublisher::writeAll(const std::uint8_t* data, std::size_t size) {
    std::size_t written = 0;

    while (written < size) {
        const auto n = ::write(writeFd_, data + written, size - written);
        if (n <= 0) {
            TRI_LOG_ERROR(LogCategory::System)
                << "write frame to ffmpeg failed: " << std::strerror(errno)
                << ", see /tmp/tri_fusion_ffmpeg_stderr.log";
            return false;
        }

        written += static_cast<std::size_t>(n);
    }

    return true;
}

void UdpMpegTsPublisher::closeWritePipe() {
    if (writeFd_ >= 0) {
        ::close(writeFd_);
        writeFd_ = -1;
    }
}

void UdpMpegTsPublisher::waitChildIfExited() {
    if (childPid_ < 0) {
        return;
    }

    int status = 0;
    const pid_t r = ::waitpid(static_cast<pid_t>(childPid_), &status, WNOHANG);
    if (r == static_cast<pid_t>(childPid_)) {
        TRI_LOG_ERROR(LogCategory::System)
            << "ffmpeg process exited, status=" << status
            << ", see /tmp/tri_fusion_ffmpeg_stderr.log";
        childPid_ = -1;
        closeWritePipe();
    }
}

} // namespace tri::protocol::udp
CPP

echo
echo "[DONE] ffmpeg H264 diagnostic publisher installed."
echo
grep -n "ffmpeg command\|tri_fusion_ffmpeg_stderr\|-f.*h264\|execvp" "$CPP" || true
