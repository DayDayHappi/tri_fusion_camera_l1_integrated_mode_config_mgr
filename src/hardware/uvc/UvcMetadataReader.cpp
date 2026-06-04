#include "hardware/uvc/UvcMetadataReader.h"
#include "foundation/log/Logger.h"
#include "foundation/time/Clock.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

namespace tri::hardware::uvc {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

UvcMetadataReader::~UvcMetadataReader() { closeNode(); }

Result<void> UvcMetadataReader::openNode(const std::string& path) {
    if (isOpen()) return Result<void>::error(ErrorCode::AlreadyInitialized, "metadata node already open");
    if (path.empty()) return Result<void>::error(ErrorCode::InvalidArgument, "empty metadata node path");
    fd_ = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) return Result<void>::error(ErrorCode::CompositeMetadataOpenFailed, std::string("open ") + path + ": " + std::strerror(errno));
    path_ = path;
    TRI_LOG_INFO(tri::foundation::LogCategory::Uvc) << "opened metadata node " << path;
    return Result<void>::success();
}

void UvcMetadataReader::closeNode() {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    path_.clear();
}

Result<UvcMetadataPacket> UvcMetadataReader::readPacket(int timeoutMs, std::size_t maxBytes) {
    if (fd_ < 0) return Result<UvcMetadataPacket>::error(ErrorCode::NotInitialized, "metadata node not open");
    if (maxBytes == 0) return Result<UvcMetadataPacket>::error(ErrorCode::InvalidArgument, "maxBytes is zero");
    pollfd pfd{fd_, POLLIN, 0};
    int pr = ::poll(&pfd, 1, timeoutMs);
    if (pr == 0) return Result<UvcMetadataPacket>::error(ErrorCode::Timeout, "metadata read timeout");
    if (pr < 0) return Result<UvcMetadataPacket>::error(ErrorCode::IoError, std::string("metadata poll: ") + std::strerror(errno));

    UvcMetadataPacket pkt;
    pkt.bytes.resize(maxBytes);
    auto n = ::read(fd_, pkt.bytes.data(), pkt.bytes.size());
    if (n < 0) return Result<UvcMetadataPacket>::error(ErrorCode::IoError, std::string("metadata read: ") + std::strerror(errno));
    pkt.bytes.resize(static_cast<std::size_t>(n));
    pkt.timestampMs = tri::foundation::Clock::nowMs();
    return Result<UvcMetadataPacket>::ok(std::move(pkt));
}

} // namespace tri::hardware::uvc
