#pragma once

#include "foundation/error/Result.h"
#include "media/frame/EncodedFrame.h"
#include "protocol/rtsp/RtspTypes.h"
#include "service/StreamService.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tri::protocol::rtsp {

class RtspServer final {
public:
    RtspServer() = default;
    ~RtspServer();

    RtspServer(const RtspServer&) = delete;
    RtspServer& operator=(const RtspServer&) = delete;

    tri::foundation::Result<void> init(const RtspServerConfig& config,
                                       tri::service::StreamService* streamService);
    tri::foundation::Result<void> start();
    void stop();

    tri::foundation::Result<tri::media::EncodedFrame> waitFrame(int timeoutMs);

    bool running() const noexcept { return running_; }
    const RtspServerConfig& config() const noexcept { return config_; }

private:
    void acceptLoop();
    void clientLoop(int clientFd, std::string peer);

    static bool sendAll(int fd, const void* data, std::size_t len);
    static bool sendText(int fd, const std::string& text);
    static bool readRtspRequest(int fd, std::string& request);
    static std::string getHeader(const std::string& request, const std::string& name);
    static std::string getCSeq(const std::string& request);
    static std::string getMethod(const std::string& request);

    std::string makeOptionsResponse(const std::string& cseq) const;
    std::string makeDescribeResponse(const std::string& cseq,
                                     const std::string& baseUrl) const;
    std::string makeSetupResponse(const std::string& cseq) const;
    std::string makePlayResponse(const std::string& cseq) const;
    std::string makeTeardownResponse(const std::string& cseq) const;
    std::string makeErrorResponse(const std::string& cseq,
                                  int code,
                                  const std::string& reason) const;

    bool streamMjpegOverTcp(int clientFd);
    bool sendMjpegRtpFrame(int clientFd,
                           const tri::media::EncodedFrame& frame,
                           std::uint16_t& rtpSeq,
                           std::uint32_t ssrc);

    static void writeBe16(std::uint8_t* p, std::uint16_t v);
    static void writeBe24(std::uint8_t* p, std::uint32_t v);
    static void writeBe32(std::uint8_t* p, std::uint32_t v);

private:
    RtspServerConfig config_{};
    tri::service::StreamService* streamService_{nullptr};

    std::atomic<bool> running_{false};
    int listenFd_{-1};

    std::thread acceptThread_;
    std::mutex clientsMutex_;
    std::vector<std::thread> clientThreads_;
    struct JpegRtpInfo {
    std::uint16_t width{800};
    std::uint16_t height{600};
    std::uint8_t type{1};
    std::vector<std::uint8_t> quantTables;
    const std::uint8_t* scanData{nullptr};
    std::size_t scanSize{0};
};

static bool parseJpegForRtp(const std::vector<std::uint8_t>& jpeg, JpegRtpInfo& info);
};

} // namespace tri::protocol::rtsp
