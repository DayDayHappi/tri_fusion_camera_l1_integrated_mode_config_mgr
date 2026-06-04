#include "protocol/gb28181/Gb28181Types.h"

namespace tri::protocol::gb28181 {

GbDeviceProfile makeGbDeviceProfile(const tri::protocol::Gb28181Config& config) {
    GbDeviceProfile p;
    p.deviceId = config.local.deviceId;
    p.domain = config.local.domain;
    p.ip = config.local.ip;
    p.port = config.local.port;
    p.password = config.local.password;
    p.name = config.channel.name;
    p.manufacturer = config.local.manufacturer;
    p.model = config.local.model;
    p.firmware = config.local.firmware;

    p.channelId = config.channel.id;
    p.channelName = config.channel.name;

    p.serverId = config.server.id;
    p.serverDomain = config.server.domain;
    p.serverIp = config.server.ip;
    p.serverPort = config.server.port;
    p.serverPassword = config.server.password;

    p.registerExpiresSec = config.reg.expiresSec;
    p.registerRetryIntervalSec = config.reg.retryIntervalSec;
    p.keepaliveIntervalSec = config.keepalive.intervalSec;
    p.keepaliveTimeoutSec = config.keepalive.timeoutSec;

    p.rtpLocalPort = config.media.rtpLocalPort;
    p.ssrc = config.media.ssrc;
    p.transport = config.media.transport;
    return p;
}

} // namespace tri::protocol::gb28181
