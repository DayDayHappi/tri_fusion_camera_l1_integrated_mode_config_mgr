#pragma once
#include "foundation/error/Result.h"
#include <cstdint>
#include <string>
#include <vector>
namespace tri::hardware::net {
class UdpSocket final { public: ~UdpSocket(); foundation::Result<void> openSocket(); void closeSocket(); foundation::Result<void> sendTo(const std::string& ip, std::uint16_t port, const std::vector<std::uint8_t>& data); private: int fd_{-1}; };
}
