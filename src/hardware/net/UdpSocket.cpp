#include "hardware/net/UdpSocket.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
namespace tri::hardware::net {
UdpSocket::~UdpSocket(){ closeSocket(); }
foundation::Result<void> UdpSocket::openSocket(){ if(fd_>=0) return foundation::Result<void>::success(); fd_=::socket(AF_INET,SOCK_DGRAM|SOCK_CLOEXEC,0); if(fd_<0) return foundation::Result<void>::error(foundation::ErrorCode::IoError,std::string("udp socket: ")+std::strerror(errno)); return foundation::Result<void>::success(); }
void UdpSocket::closeSocket(){ if(fd_>=0) ::close(fd_); fd_=-1; }
foundation::Result<void> UdpSocket::sendTo(const std::string& ip, std::uint16_t port, const std::vector<std::uint8_t>& data){ if(fd_<0){ auto r=openSocket(); if(!r) return r; } sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(port); if(::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr)!=1) return foundation::Result<void>::error(foundation::ErrorCode::InvalidArgument,"invalid IPv4: "+ip); auto n=::sendto(fd_, data.data(), data.size(), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)); if(n<0) return foundation::Result<void>::error(foundation::ErrorCode::IoError,std::string("sendto: ")+std::strerror(errno)); return foundation::Result<void>::success(); }
}
