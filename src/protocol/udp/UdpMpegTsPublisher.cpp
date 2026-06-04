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

    args.push_back(config_.ffmpegPath);

    args.push_back("-hide_banner");

    args.push_back("-loglevel");
    args.push_back(config_.logLevel);

    /*
     * 对应 GStreamer 低延时：
     * queue leaky + do-timestamp + udpsink sync=false async=false
     */
    args.push_back("-fflags");
    args.push_back("+genpts+nobuffer");

    args.push_back("-flags");
    args.push_back("low_delay");

    args.push_back("-use_wallclock_as_timestamps");
    args.push_back("1");

    args.push_back("-probesize");
    args.push_back("32");

    args.push_back("-analyzeduration");
    args.push_back("0");

    if (config_.inputFormat == FfmpegPipeInputFormat::H264AnnexB) {
        /*
         * MainStream 已经是 MPP 输出的 H264 Annex-B。
         * 这里只负责封装 MPEG-TS，不做重新编码。
         */
        args.push_back("-r");
        args.push_back(std::to_string(config_.fps));

        args.push_back("-f");
        args.push_back("h264");

        args.push_back("-i");
        args.push_back("pipe:0");

        args.push_back("-an");

        args.push_back("-c:v");
        args.push_back("copy");
    } else if (config_.inputFormat == FfmpegPipeInputFormat::RawYuyv422) {
        /*
         * 备用调试路径：raw YUYV 走 ffmpeg/libx264。
         * 正式 RGA+MPP 链路不要走这个分支。
         */
        args.push_back("-f");
        args.push_back("rawvideo");

        args.push_back("-pixel_format");
        args.push_back("yuyv422");

        args.push_back("-video_size");
        args.push_back(std::to_string(config_.width) + "x" + std::to_string(config_.height));

        args.push_back("-framerate");
        args.push_back(std::to_string(config_.fps));

        args.push_back("-i");
        args.push_back("pipe:0");

        args.push_back("-vf");
        args.push_back("format=yuv420p");

        args.push_back("-c:v");
        args.push_back(config_.encoder);

        args.push_back("-preset");
        args.push_back(config_.preset);

        args.push_back("-tune");
        args.push_back(config_.tune);

        args.push_back("-g");
        args.push_back(std::to_string(config_.fps / 2 > 0 ? config_.fps / 2 : 15));

        args.push_back("-bf");
        args.push_back("0");
    } else {
        /*
         * 备用 MJPEG 调试路径。
         */
        args.push_back("-f");
        args.push_back("mjpeg");

        args.push_back("-r");
        args.push_back(std::to_string(config_.fps));

        args.push_back("-i");
        args.push_back("pipe:0");

        args.push_back("-vf");
        args.push_back("format=yuv420p");

        args.push_back("-c:v");
        args.push_back(config_.encoder);

        args.push_back("-preset");
        args.push_back(config_.preset);

        args.push_back("-tune");
        args.push_back(config_.tune);

        args.push_back("-g");
        args.push_back(std::to_string(config_.fps / 2 > 0 ? config_.fps / 2 : 15));

        args.push_back("-bf");
        args.push_back("0");
    }

    /*
     * MPEG-TS 低延时封装。
     */
    args.push_back("-muxdelay");
    args.push_back("0");

    args.push_back("-muxpreload");
    args.push_back("0");

    args.push_back("-flush_packets");
    args.push_back("1");

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
