#pragma once

#include "protocol/ProtocolConfigTypes.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace tri::protocol::gb28181 {

struct GbDeviceProfile {
    std::string deviceId{"34020000001320000001"};
    std::string domain{"3402000000"};
    std::string ip{"0.0.0.0"};
    std::uint16_t port{5060};
    std::string password{"12345678"};
    std::string name{"Tri-Fusion Camera"};
    std::string manufacturer{"TriFusion"};
    std::string model{"TriFusion-Camera"};
    std::string firmware{"1.0.0"};

    std::string channelId{"34020000001320000001"};
    std::string channelName{"Tri-Fusion MainStream"};

    std::string serverId{"41010500002000000001"};
    std::string serverDomain{"4101050000"};
    std::string serverIp{"192.168.1.153"};
    std::uint16_t serverPort{8118};
    std::string serverPassword{"12345678"};

    int registerExpiresSec{3600};
    int registerRetryIntervalSec{5};
    int keepaliveIntervalSec{60};
    int keepaliveTimeoutSec{3};

    std::uint16_t rtpLocalPort{30000};
    std::string ssrc{"0000000001"};
    std::string transport{"udp"};
};

struct GbControlRequest {
    std::string action;
    std::unordered_map<std::string, std::string> fields;
};

struct GbInviteContext {
    std::string callId;
    std::string remoteIp;
    std::uint16_t remotePort{0};
    std::string ssrc;
};

GbDeviceProfile makeGbDeviceProfile(const tri::protocol::Gb28181Config& config);

} // namespace tri::protocol::gb28181
