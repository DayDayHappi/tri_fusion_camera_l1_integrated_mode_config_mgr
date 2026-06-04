#include "protocol/rtsp/RtspServer.h"

#include "foundation/error/ErrorCode.h"
#include "foundation/log/Logger.h"
#include "hardware/mpp/MppTypes.h"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

namespace tri::protocol::rtsp {

using tri::foundation::ErrorCode;
using tri::foundation::LogCategory;
using tri::foundation::Result;

namespace {

constexpr int kBacklog = 8;
constexpr std::size_t kRtspMaxRequest = 8192;
constexpr std::size_t kRtpTcpMaxPayload = 1200;
constexpr std::uint8_t kRtpVersion = 2;
constexpr std::uint8_t kRtpPayloadJpeg = 26;
constexpr std::uint8_t kInterleavedRtpChannel = 0;

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
    std::size_t pos = 0;
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) {
        ++pos;
    }
    return s.substr(pos);
}

std::string lower(std::string s) {
    for (auto& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return s;
}

std::string peerName(sockaddr_in addr) {
    char ip[64] = {};
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    std::ostringstream oss;
    oss << ip << ":" << ntohs(addr.sin_port);
    return oss.str();
}

std::uint32_t rtpTimestampFromPtsMs(std::int64_t ptsMs) {
    if (ptsMs < 0) {
        ptsMs = 0;
    }
    return static_cast<std::uint32_t>((static_cast<std::uint64_t>(ptsMs) * 90000ULL) / 1000ULL);
}

} // namespace

RtspServer::~RtspServer() {
    stop();
}

Result<void> RtspServer::init(const RtspServerConfig& config,
                              tri::service::StreamService* streamService) {
    if (streamService == nullptr) {
        return Result<void>::error(ErrorCode::InvalidArgument, "RTSP server stream service is null");
    }

    config_ = config;
    streamService_ = streamService;
    return Result<void>::success();
}

Result<void> RtspServer::start() {
    if (streamService_ == nullptr) {
        return Result<void>::error(ErrorCode::NotInitialized, "RTSP server is not initialized");
    }
    if (running_) {
        return Result<void>::success();
    }

    listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        return Result<void>::error(ErrorCode::IoError,
                                   std::string("socket failed: ") + std::strerror(errno));
    }

    int reuse = 1;
    ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);

    if (config_.host.empty() || config_.host == "0.0.0.0") {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (::inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr) != 1) {
            ::close(listenFd_);
            listenFd_ = -1;
            return Result<void>::error(ErrorCode::InvalidArgument,
                                       "invalid RTSP bind host: " + config_.host);
        }
    }

    if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        const auto msg = std::string("bind failed: ") + std::strerror(errno);
        ::close(listenFd_);
        listenFd_ = -1;
        return Result<void>::error(ErrorCode::IoError, msg);
    }

    if (::listen(listenFd_, kBacklog) < 0) {
        const auto msg = std::string("listen failed: ") + std::strerror(errno);
        ::close(listenFd_);
        listenFd_ = -1;
        return Result<void>::error(ErrorCode::IoError, msg);
    }

    running_ = true;
    acceptThread_ = std::thread(&RtspServer::acceptLoop, this);

    TRI_LOG_INFO(LogCategory::System)
        << "RTSP server listening on " << config_.host << ":"
        << config_.port << config_.path;

    return Result<void>::success();
}

void RtspServer::stop() {
    if (!running_ && listenFd_ < 0) {
        return;
    }

    running_ = false;

    if (listenFd_ >= 0) {
        ::shutdown(listenFd_, SHUT_RDWR);
        ::close(listenFd_);
        listenFd_ = -1;
    }

    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto& t : clientThreads_) {
            if (t.joinable()) {
                t.join();
            }
        }
        clientThreads_.clear();
    }
}

Result<tri::media::EncodedFrame> RtspServer::waitFrame(int timeoutMs) {
    if (!running_) {
        return Result<tri::media::EncodedFrame>::error(ErrorCode::MainStreamNotReady,
                                                       "RTSP server is not running");
    }
    return streamService_->waitFrame(timeoutMs);
}

void RtspServer::acceptLoop() {
    while (running_) {
        sockaddr_in peerAddr {};
        socklen_t len = sizeof(peerAddr);
        int clientFd = ::accept(listenFd_, reinterpret_cast<sockaddr*>(&peerAddr), &len);
        if (clientFd < 0) {
            if (running_) {
                TRI_LOG_WARN(LogCategory::System)
                    << "RTSP accept failed: " << std::strerror(errno);
            }
            continue;
        }

        const auto peer = peerName(peerAddr);
        TRI_LOG_INFO(LogCategory::System) << "RTSP client connected: " << peer;

        std::lock_guard<std::mutex> lock(clientsMutex_);
        clientThreads_.emplace_back(&RtspServer::clientLoop, this, clientFd, peer);
    }
}

void RtspServer::clientLoop(int clientFd, std::string peer) {
    bool playing = false;

    while (running_) {
        std::string req;
        if (!readRtspRequest(clientFd, req)) {
            break;
        }

        const auto method = getMethod(req);
        const auto cseq = getCSeq(req);
        const auto host = getHeader(req, "Host");

        std::string baseUrl = "rtsp://";
        if (!host.empty()) {
            baseUrl += host;
        } else {
            baseUrl += "127.0.0.1:" + std::to_string(config_.port);
        }
        baseUrl += config_.path;

        if (method == "OPTIONS") {
            if (!sendText(clientFd, makeOptionsResponse(cseq))) break;
        } else if (method == "DESCRIBE") {
            if (!sendText(clientFd, makeDescribeResponse(cseq, baseUrl))) break;
        } else if (method == "SETUP") {
            if (!sendText(clientFd, makeSetupResponse(cseq))) break;
        } else if (method == "PLAY") {
            if (!sendText(clientFd, makePlayResponse(cseq))) break;
            playing = true;
            break;
        } else if (method == "TEARDOWN") {
            (void)sendText(clientFd, makeTeardownResponse(cseq));
            ::close(clientFd);
            return;
        } else {
            if (!sendText(clientFd, makeErrorResponse(cseq, 405, "Method Not Allowed"))) break;
        }
    }

    if (playing && running_) {
        TRI_LOG_INFO(LogCategory::System) << "RTSP client play start: " << peer;
        (void)streamMjpegOverTcp(clientFd);
    }

    TRI_LOG_INFO(LogCategory::System) << "RTSP client disconnected: " << peer;
    ::shutdown(clientFd, SHUT_RDWR);
    ::close(clientFd);
}

bool RtspServer::sendAll(int fd, const void* data, std::size_t len) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    std::size_t sent = 0;

    while (sent < len) {
        const auto n = ::send(fd, p + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }

    return true;
}

bool RtspServer::sendText(int fd, const std::string& text) {
    return sendAll(fd, text.data(), text.size());
}

bool RtspServer::readRtspRequest(int fd, std::string& request) {
    request.clear();
    request.reserve(1024);

    char ch = 0;
    while (request.size() < kRtspMaxRequest) {
        const auto n = ::recv(fd, &ch, 1, 0);
        if (n <= 0) {
            return false;
        }

        request.push_back(ch);

        if (request.size() >= 4 &&
            request.compare(request.size() - 4, 4, "\r\n\r\n") == 0) {
            return true;
        }
    }

    return false;
}

std::string RtspServer::getHeader(const std::string& request, const std::string& name) {
    std::istringstream iss(request);
    std::string line;
    const auto wanted = lower(name);

    while (std::getline(iss, line)) {
        auto pos = line.find(':');
        if (pos == std::string::npos) {
            continue;
        }

        auto key = lower(trim(line.substr(0, pos)));
        if (key == wanted) {
            return trim(line.substr(pos + 1));
        }
    }

    return {};
}

std::string RtspServer::getCSeq(const std::string& request) {
    auto cseq = getHeader(request, "CSeq");
    return cseq.empty() ? "1" : cseq;
}

std::string RtspServer::getMethod(const std::string& request) {
    auto pos = request.find(' ');
    if (pos == std::string::npos) {
        return {};
    }
    return request.substr(0, pos);
}

std::string RtspServer::makeOptionsResponse(const std::string& cseq) const {
    std::ostringstream oss;
    oss << "RTSP/1.0 200 OK\r\n"
        << "CSeq: " << cseq << "\r\n"
        << "Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN\r\n"
        << "\r\n";
    return oss.str();
}

std::string RtspServer::makeDescribeResponse(const std::string& cseq,
                                             const std::string& baseUrl) const {
    std::ostringstream sdp;
    sdp << "v=0\r\n"
        << "o=- 0 0 IN IP4 127.0.0.1\r\n"
        << "s=TriFusion MainStream\r\n"
        << "c=IN IP4 0.0.0.0\r\n"
        << "t=0 0\r\n"
        << "a=control:*\r\n"
        << "m=video 0 RTP/AVP/TCP 26\r\n"
        << "a=control:trackID=0\r\n"
        << "a=framerate:25\r\n";

    const auto body = sdp.str();

    std::ostringstream oss;
    oss << "RTSP/1.0 200 OK\r\n"
        << "CSeq: " << cseq << "\r\n"
        << "Content-Base: " << baseUrl << "/\r\n"
        << "Content-Type: application/sdp\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "\r\n"
        << body;

    return oss.str();
}

std::string RtspServer::makeSetupResponse(const std::string& cseq) const {
    std::ostringstream oss;
    oss << "RTSP/1.0 200 OK\r\n"
        << "CSeq: " << cseq << "\r\n"
        << "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
        << "Session: tri-fusion-rtsp\r\n"
        << "\r\n";
    return oss.str();
}

std::string RtspServer::makePlayResponse(const std::string& cseq) const {
    std::ostringstream oss;
    oss << "RTSP/1.0 200 OK\r\n"
        << "CSeq: " << cseq << "\r\n"
        << "Session: tri-fusion-rtsp\r\n"
        << "RTP-Info: url=" << config_.path << "/trackID=0;seq=0;rtptime=0\r\n"
        << "\r\n";
    return oss.str();
}

std::string RtspServer::makeTeardownResponse(const std::string& cseq) const {
    std::ostringstream oss;
    oss << "RTSP/1.0 200 OK\r\n"
        << "CSeq: " << cseq << "\r\n"
        << "Session: tri-fusion-rtsp\r\n"
        << "\r\n";
    return oss.str();
}

std::string RtspServer::makeErrorResponse(const std::string& cseq,
                                          int code,
                                          const std::string& reason) const {
    std::ostringstream oss;
    oss << "RTSP/1.0 " << code << " " << reason << "\r\n"
        << "CSeq: " << cseq << "\r\n"
        << "\r\n";
    return oss.str();
}

bool RtspServer::streamMjpegOverTcp(int clientFd) {
    std::uint16_t rtpSeq = 0;
    constexpr std::uint32_t ssrc = 0x54524653; // TRFS

    while (running_) {
        auto frame = streamService_->waitFrame(1000);
        if (!frame) {
            continue;
        }

        if (frame.value().data.empty()) {
            continue;
        }

        if (!sendMjpegRtpFrame(clientFd, frame.value(), rtpSeq, ssrc)) {
            return false;
        }
    }

    return true;
}

bool RtspServer::sendMjpegRtpFrame(int clientFd,
                                   const tri::media::EncodedFrame& frame,
                                   std::uint16_t& rtpSeq,
                                   std::uint32_t ssrc) {
    JpegRtpInfo info;
    if (!parseJpegForRtp(frame.data, info)) {
        TRI_LOG_WARN(LogCategory::System) << "invalid JPEG frame for RTP/JPEG";
        return true;
    }

    const auto ts = rtpTimestampFromPtsMs(frame.ptsMs);
    std::size_t offset = 0;
    bool firstPacket = true;

    while (offset < info.scanSize) {
        const bool withQtable = firstPacket && !info.quantTables.empty();

        const std::size_t jpegHeaderLen = 8;
        const std::size_t qtableHeaderLen = withQtable ? 4 + info.quantTables.size() : 0;
        const std::size_t rtpHeaderLen = 12;
        const std::size_t maxPacket = 1400;

        const std::size_t fixedLen = rtpHeaderLen + jpegHeaderLen + qtableHeaderLen;
        if (fixedLen >= maxPacket) {
            TRI_LOG_WARN(LogCategory::System) << "RTP/JPEG fixed header too large";
            return false;
        }

        const std::size_t maxScanPayload = maxPacket - fixedLen;
        const std::size_t remain = info.scanSize - offset;
        const std::size_t chunk = remain > maxScanPayload ? maxScanPayload : remain;
        const bool marker = (offset + chunk) >= info.scanSize;

        std::vector<std::uint8_t> pkt;
        pkt.resize(fixedLen + chunk);

        // RTP fixed header
        pkt[0] = static_cast<std::uint8_t>(kRtpVersion << 6);
        pkt[1] = static_cast<std::uint8_t>((marker ? 0x80 : 0x00) | kRtpPayloadJpeg);
        writeBe16(pkt.data() + 2, rtpSeq++);
        writeBe32(pkt.data() + 4, ts);
        writeBe32(pkt.data() + 8, ssrc);

        // RFC2435 JPEG payload header
        std::size_t p = 12;
        pkt[p + 0] = 0x00; // type-specific
        writeBe24(pkt.data() + p + 1, static_cast<std::uint32_t>(offset));
        pkt[p + 4] = info.type;
        pkt[p + 5] = withQtable ? 128 : 75; // q >= 128 means dynamic quant table follows
        pkt[p + 6] = static_cast<std::uint8_t>(info.width / 8);
        pkt[p + 7] = static_cast<std::uint8_t>(info.height / 8);
        p += 8;

        if (withQtable) {
            pkt[p + 0] = 0x00; // MBZ
            pkt[p + 1] = 0x00; // precision: 8-bit
            writeBe16(pkt.data() + p + 2, static_cast<std::uint16_t>(info.quantTables.size()));
            p += 4;

            std::memcpy(pkt.data() + p, info.quantTables.data(), info.quantTables.size());
            p += info.quantTables.size();
        }

        std::memcpy(pkt.data() + p, info.scanData + offset, chunk);

        std::uint8_t tcpHeader[4];
        tcpHeader[0] = '$';
        tcpHeader[1] = kInterleavedRtpChannel;
        writeBe16(tcpHeader + 2, static_cast<std::uint16_t>(pkt.size()));

        if (!sendAll(clientFd, tcpHeader, sizeof(tcpHeader))) {
            return false;
        }
        if (!sendAll(clientFd, pkt.data(), pkt.size())) {
            return false;
        }

        offset += chunk;
        firstPacket = false;
    }

    return true;
}

void RtspServer::writeBe16(std::uint8_t* p, std::uint16_t v) {
    p[0] = static_cast<std::uint8_t>((v >> 8) & 0xff);
    p[1] = static_cast<std::uint8_t>(v & 0xff);
}

void RtspServer::writeBe24(std::uint8_t* p, std::uint32_t v) {
    p[0] = static_cast<std::uint8_t>((v >> 16) & 0xff);
    p[1] = static_cast<std::uint8_t>((v >> 8) & 0xff);
    p[2] = static_cast<std::uint8_t>(v & 0xff);
}

bool RtspServer::parseJpegForRtp(const std::vector<std::uint8_t>& jpeg, JpegRtpInfo& info) {
    if (jpeg.size() < 16) {
        return false;
    }

    if (jpeg[0] != 0xff || jpeg[1] != 0xd8) {
        return false; // SOI
    }

    std::size_t pos = 2;
    std::size_t scanStart = 0;
    std::size_t scanEnd = jpeg.size();

    info.quantTables.clear();
    info.scanData = nullptr;
    info.scanSize = 0;

    while (pos + 4 <= jpeg.size()) {
        if (jpeg[pos] != 0xff) {
            ++pos;
            continue;
        }

        while (pos < jpeg.size() && jpeg[pos] == 0xff) {
            ++pos;
        }

        if (pos >= jpeg.size()) {
            break;
        }

        const std::uint8_t marker = jpeg[pos++];

        // EOI
        if (marker == 0xd9) {
            break;
        }

        // Standalone markers without length
        if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd7)) {
            continue;
        }

        if (pos + 2 > jpeg.size()) {
            return false;
        }

        const std::uint16_t segLen =
            static_cast<std::uint16_t>((jpeg[pos] << 8) | jpeg[pos + 1]);

        if (segLen < 2 || pos + segLen > jpeg.size()) {
            return false;
        }

        const std::size_t segData = pos + 2;
        const std::size_t segDataLen = segLen - 2;

        // DQT
        if (marker == 0xdb) {
            std::size_t qpos = segData;
            const std::size_t qend = segData + segDataLen;

            while (qpos < qend) {
                const std::uint8_t pqTq = jpeg[qpos++];
                const std::uint8_t precision = static_cast<std::uint8_t>((pqTq >> 4) & 0x0f);
                const std::size_t tableLen = precision == 0 ? 64 : 128;

                if (precision != 0) {
                    return false; // RTP/JPEG 这里先只支持 8-bit quant table
                }

                if (qpos + tableLen > qend) {
                    return false;
                }

                info.quantTables.insert(info.quantTables.end(),
                                        jpeg.begin() + static_cast<std::ptrdiff_t>(qpos),
                                        jpeg.begin() + static_cast<std::ptrdiff_t>(qpos + tableLen));
                qpos += tableLen;
            }
        }

        // SOF0 baseline
        if (marker == 0xc0) {
            if (segDataLen < 8) {
                return false;
            }

            const std::uint8_t precision = jpeg[segData];
            if (precision != 8) {
                return false;
            }

            info.height = static_cast<std::uint16_t>((jpeg[segData + 1] << 8) | jpeg[segData + 2]);
            info.width  = static_cast<std::uint16_t>((jpeg[segData + 3] << 8) | jpeg[segData + 4]);

            const std::uint8_t components = jpeg[segData + 5];
            if (components >= 3 && segDataLen >= 15) {
                const std::uint8_t ySampling = jpeg[segData + 7];
                const std::uint8_t h = static_cast<std::uint8_t>((ySampling >> 4) & 0x0f);
                const std::uint8_t v = static_cast<std::uint8_t>(ySampling & 0x0f);

                // RFC2435: type 0 通常对应 4:2:0，type 1 通常对应 4:2:2
                if (h == 2 && v == 2) {
                    info.type = 0;
                } else {
                    info.type = 1;
                }
            }
        }

        // SOS，scan data 从 SOS 段后开始
        if (marker == 0xda) {
            scanStart = pos + segLen;

            // 找 EOI，scan data 不带 EOI
            scanEnd = jpeg.size();
            for (std::size_t i = scanStart; i + 1 < jpeg.size(); ++i) {
                if (jpeg[i] == 0xff && jpeg[i + 1] == 0xd9) {
                    scanEnd = i;
                    break;
                }
            }

            break;
        }

        pos += segLen;
    }

    if (scanStart == 0 || scanStart >= jpeg.size() || scanEnd <= scanStart) {
        return false;
    }

    if (info.width == 0 || info.height == 0) {
        return false;
    }

    info.scanData = jpeg.data() + scanStart;
    info.scanSize = scanEnd - scanStart;

    return info.scanSize > 0;
}

void RtspServer::writeBe32(std::uint8_t* p, std::uint32_t v) {
    p[0] = static_cast<std::uint8_t>((v >> 24) & 0xff);
    p[1] = static_cast<std::uint8_t>((v >> 16) & 0xff);
    p[2] = static_cast<std::uint8_t>((v >> 8) & 0xff);
    p[3] = static_cast<std::uint8_t>(v & 0xff);
}

} // namespace tri::protocol::rtsp
