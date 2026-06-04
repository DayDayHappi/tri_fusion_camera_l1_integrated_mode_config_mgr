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
    H264AnnexB,
    Mjpeg,
    RawYuyv422,
};

struct UdpMpegTsPublisherConfig {
    std::string udpUrl{"udp://192.168.1.35:5004?pkt_size=1316"};

    // 当前 L4/L8 联调链路里 MainStream 已经是 MPP H264。
    // 保留这个字段兼容测试程序，但实际 ffmpeg 输入按 H264 处理。
    FfmpegPipeInputFormat inputFormat{FfmpegPipeInputFormat::H264AnnexB};

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
