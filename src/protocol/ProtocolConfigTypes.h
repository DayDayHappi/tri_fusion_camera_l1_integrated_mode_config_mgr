#pragma once

#include <cstdint>
#include <string>

namespace tri::protocol {

struct Gb28181LocalConfig {
    std::string deviceId{"34020000001320000001"};
    std::string domain{"3402000000"};
    std::string ip{"0.0.0.0"};
    std::uint16_t port{5060};
    std::string password{"12345678"};
    std::string manufacturer{"TriFusion"};
    std::string model{"TriFusion-Camera"};
    std::string firmware{"1.0.0"};
};

struct Gb28181ChannelConfig {
    std::string id{"34020000001320000001"};
    std::string name{"Tri-Fusion MainStream"};
};

struct Gb28181ServerConfig {
    std::string id{"41010500002000000001"};
    std::string domain{"4101050000"};
    std::string ip{"192.168.1.153"};
    std::uint16_t port{8118};
    std::string password{"12345678"};
};

struct Gb28181RegisterConfig {
    int expiresSec{3600};
    int retryIntervalSec{5};
};

struct Gb28181KeepaliveConfig {
    int intervalSec{60};
    int timeoutSec{3};
};

struct Gb28181MediaConfig {
    std::uint16_t rtpLocalPort{30000};
    std::string ssrc{"0000000001"};
    std::string transport{"udp"};
};

struct Gb28181Config {
    bool enable{true};
    Gb28181LocalConfig local{};
    Gb28181ChannelConfig channel{};
    Gb28181ServerConfig server{};
    Gb28181RegisterConfig reg{};
    Gb28181KeepaliveConfig keepalive{};
    Gb28181MediaConfig media{};
};

struct OnvifConfig {
    bool enable{true};
    std::string listenIp{"0.0.0.0"};
    std::uint16_t listenPort{8080};
    bool authEnable{false};
    std::string username{"admin"};
    std::string password{"admin"};
    std::string rtspAdvertiseIp{"0.0.0.0"};
    std::uint16_t rtspPort{8554};
    std::string rtspPath{"/live/main"};
};

struct PrivateApiConfig {
    bool enable{true};
    std::string listenIp{"0.0.0.0"};
    std::uint16_t listenPort{18080};
    bool authEnable{false};
    std::string username{"admin"};
    std::string password{"admin"};
};

struct RtspConfig {
    bool enable{true};
    std::string bindIp{"0.0.0.0"};
    std::string advertiseIp{"0.0.0.0"};
    std::uint16_t port{8554};
    std::string path{"/live/main"};
    bool authEnable{false};
    std::string username{"admin"};
    std::string password{"admin"};
};

struct ProtocolEndpointConfigSet {
    Gb28181Config gb28181{};
    OnvifConfig onvif{};
    PrivateApiConfig privateApi{};
    RtspConfig rtsp{};
};

} // namespace tri::protocol
