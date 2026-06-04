#!/usr/bin/env bash
set -e

H="src/protocol/udp/UdpMpegTsPublisher.h"
CPP="src/protocol/udp/UdpMpegTsPublisher.cpp"
TS="$(date +%Y%m%d_%H%M%S)"

cp "$H" "${H}.bak_direct_h264_${TS}"
cp "$CPP" "${CPP}.bak_direct_h264_${TS}"

echo "[BACKUP] $H -> ${H}.bak_direct_h264_${TS}"
echo "[BACKUP] $CPP -> ${CPP}.bak_direct_h264_${TS}"

cat > "$H" <<'HPP'
#pragma once

#include "foundation/error/Result.h"
#include "service/StreamService.h"

#include <atomic>
#include <cstdint>
#include <netinet/in.h>
#include <string>
#include <thread>

namespace tri::protocol::udp {

enum class FfmpegPipeInputFormat {
    Mjpeg,
    RawYuyv422,
};

struct UdpMpegTsPublisherConfig {
    std::string udpUrl{"udp://192.168.1.35:5004?pkt_size=1316"};

    // 保留字段，兼容原测试程序配置。
    FfmpegPipeInputFormat inputFormat{FfmpegPipeInputFormat::Mjpeg};

    std::uint32_t width{800};
    std::uint32_t height{600};
    std::uint32_t fps{30};

    std::string encoder{"libx264"};
    std::string preset{"ultrafast"};
    std::string tune{"zerolatency"};

    std::string ffmpegPath{"ffmpeg"};
    std::string logLevel{"warning"};
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
    tri::foundation::Result<void> startUdpSocket();
    void senderLoop();

    bool sendFrameUdp(const std::vector<std::uint8_t>& bytes);
    bool parseUdpUrl(const std::string& url, std::string& host, std::uint16_t& port, std::size_t& pktSize) const;

private:
    UdpMpegTsPublisherConfig config_{};
    tri::service::StreamService* streamService_{nullptr};

    std::atomic<bool> running_{false};
    std::thread senderThread_;

    int udpFd_{-1};
    sockaddr_in destAddr_{};
    std::size_t pktSize_{1316};

    UdpMpegTsPublisherStats stats_{};
};

} // namespace tri::protocol::udp
HPP

cat > "$CPP" <<'CPP'
#include "protocol/udp/UdpMpegTsPublisher.h"

#include "foundation/error/ErrorCode.h"
#include "foundation/log/Logger.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace tri::protocol::udp {

using tri::foundation::ErrorCode;
using tri::foundation::LogCategory;
using tri::foundation::Result;

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

    config_ = config;
    streamService_ = streamService;
    stats_ = UdpMpegTsPublisherStats{};

    auto ret = startUdpSocket();
    if (!ret) {
        return ret;
    }

    running_ = true;
    senderThread_ = std::thread(&UdpMpegTsPublisher::senderLoop, this);

    TRI_LOG_INFO(LogCategory::System)
        << "Direct H264 UDP publisher started, url=" << config_.udpUrl
        << ", pktSize=" << pktSize_;

    return Result<void>::success();
}

void UdpMpegTsPublisher::stop() {
    running_ = false;

    if (senderThread_.joinable()) {
        senderThread_.join();
    }

    if (udpFd_ >= 0) {
        ::close(udpFd_);
        udpFd_ = -1;
    }

    streamService_ = nullptr;
}

bool UdpMpegTsPublisher::parseUdpUrl(const std::string& url,
                                     std::string& host,
                                     std::uint16_t& port,
                                     std::size_t& pktSize) const {
    // 支持格式：
    // udp://192.168.1.35:5004?pkt_size=1316
    // udp://192.168.1.35:5004
    std::string s = url;

    const std::string prefix = "udp://";
    if (s.rfind(prefix, 0) == 0) {
        s = s.substr(prefix.size());
    }

    const auto qpos = s.find('?');
    std::string addr = qpos == std::string::npos ? s : s.substr(0, qpos);
    std::string query = qpos == std::string::npos ? std::string{} : s.substr(qpos + 1);

    const auto cpos = addr.rfind(':');
    if (cpos == std::string::npos) {
        return false;
    }

    host = addr.substr(0, cpos);
    const auto portStr = addr.substr(cpos + 1);

    if (host.empty() || portStr.empty()) {
        return false;
    }

    try {
        const int p = std::stoi(portStr);
        if (p <= 0 || p > 65535) {
            return false;
        }
        port = static_cast<std::uint16_t>(p);
    } catch (...) {
        return false;
    }

    pktSize = 1316;
    const std::string key = "pkt_size=";
    const auto kpos = query.find(key);
    if (kpos != std::string::npos) {
        const auto begin = kpos + key.size();
        auto end = query.find('&', begin);
        if (end == std::string::npos) {
            end = query.size();
        }

        try {
            const auto v = static_cast<std::size_t>(std::stoul(query.substr(begin, end - begin)));
            if (v >= 256 && v <= 60000) {
                pktSize = v;
            }
        } catch (...) {
            pktSize = 1316;
        }
    }

    return true;
}

Result<void> UdpMpegTsPublisher::startUdpSocket() {
    std::string host;
    std::uint16_t port = 0;
    std::size_t pktSize = 1316;

    if (!parseUdpUrl(config_.udpUrl, host, port, pktSize)) {
        return Result<void>::error(ErrorCode::InvalidArgument,
                                   "invalid udp url: " + config_.udpUrl);
    }

    udpFd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (udpFd_ < 0) {
        return Result<void>::error(ErrorCode::IoError,
                                   std::string("socket(AF_INET, SOCK_DGRAM) failed: ") +
                                       std::strerror(errno));
    }

    destAddr_ = {};
    destAddr_.sin_family = AF_INET;
    destAddr_.sin_port = htons(port);

    if (::inet_pton(AF_INET, host.c_str(), &destAddr_.sin_addr) != 1) {
        ::close(udpFd_);
        udpFd_ = -1;
        return Result<void>::error(ErrorCode::InvalidArgument,
                                   "invalid udp host ip: " + host);
    }

    pktSize_ = pktSize;

    TRI_LOG_INFO(LogCategory::System)
        << "Direct UDP destination " << host << ":" << port
        << ", pktSize=" << pktSize_;

    return Result<void>::success();
}

void UdpMpegTsPublisher::senderLoop() {
    while (running_) {
        if (streamService_ == nullptr) {
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

        if (!sendFrameUdp(bytes)) {
            ++stats_.writeErrors;
            break;
        }

        ++stats_.framesWritten;
        stats_.bytesWritten += bytes.size();

        if ((stats_.framesWritten % 100) == 0) {
            TRI_LOG_INFO(LogCategory::System)
                << "Direct H264 UDP pushed frames=" << stats_.framesWritten
                << " bytes=" << stats_.bytesWritten;
        }
    }

    running_ = false;
}

bool UdpMpegTsPublisher::sendFrameUdp(const std::vector<std::uint8_t>& bytes) {
    if (udpFd_ < 0) {
        return false;
    }

    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const std::size_t chunk = std::min(pktSize_, bytes.size() - offset);

        const auto n = ::sendto(udpFd_,
                                bytes.data() + offset,
                                chunk,
                                0,
                                reinterpret_cast<const sockaddr*>(&destAddr_),
                                sizeof(destAddr_));
        if (n < 0) {
            TRI_LOG_ERROR(LogCategory::System)
                << "send udp h264 packet failed: " << std::strerror(errno);
            return false;
        }

        if (static_cast<std::size_t>(n) != chunk) {
            TRI_LOG_ERROR(LogCategory::System)
                << "send udp h264 packet short write";
            return false;
        }

        offset += chunk;
    }

    return true;
}

} // namespace tri::protocol::udp
CPP

echo "[DONE] UdpMpegTsPublisher replaced by direct H264 UDP sender."
echo
grep -n "Direct H264 UDP\|sendFrameUdp\|startUdpSocket" "$CPP"
