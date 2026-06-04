#include "protocol/ProtocolConfigLoader.h"

#include "foundation/config/ConfigManager.h"
#include "foundation/error/ErrorCode.h"
#include "foundation/utils/FileUtils.h"
#include "foundation/utils/NetUtils.h"
#include "foundation/utils/StringUtils.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace tri::protocol {
using tri::foundation::ErrorCode;
using tri::foundation::FlatConfig;
using tri::foundation::Result;

namespace {
std::string joinPath(const std::string& dir, const std::string& file) {
    if (dir.empty() || dir == ".") return "./" + file;
    if (dir.back() == '/') return dir + file;
    return dir + "/" + file;
}

std::string get(const FlatConfig& cfg, const std::string& key, const std::string& fallback) {
    auto it = cfg.find(key);
    return it == cfg.end() ? fallback : tri::foundation::str::unquote(it->second);
}

int toInt(const FlatConfig& cfg, const std::string& key, int fallback) {
    auto it = cfg.find(key);
    if (it == cfg.end()) return fallback;
    try { return std::stoi(tri::foundation::str::unquote(it->second)); } catch (...) { return fallback; }
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool toBool(const FlatConfig& cfg, const std::string& key, bool fallback) {
    const auto text = lower(get(cfg, key, fallback ? "true" : "false"));
    if (text == "true" || text == "1" || text == "yes" || text == "on") return true;
    if (text == "false" || text == "0" || text == "no" || text == "off") return false;
    return fallback;
}

std::uint16_t toPort(const FlatConfig& cfg, const std::string& key, std::uint16_t fallback) {
    const auto v = toInt(cfg, key, static_cast<int>(fallback));
    if (!tri::foundation::net::isValidPort(v)) return fallback;
    return static_cast<std::uint16_t>(v);
}

Result<FlatConfig> parseOptionalYaml(const std::string& path) {
    auto cfg = tri::foundation::parseSimpleYaml(path);
    if (cfg) return cfg;
    if (cfg.status().code() == ErrorCode::NotFound || cfg.status().code() == ErrorCode::IoError) {
        return Result<FlatConfig>::ok({});
    }
    return cfg;
}

Result<void> checkPort(std::uint16_t port, const std::string& name) {
    if (!tri::foundation::net::isValidPort(static_cast<int>(port))) {
        return Result<void>::error(ErrorCode::ConfigError, name + " is invalid");
    }
    return Result<void>::success();
}

Result<void> checkNotEmpty(const std::string& value, const std::string& name) {
    if (tri::foundation::str::trim(value).empty()) {
        return Result<void>::error(ErrorCode::ConfigError, name + " is empty");
    }
    return Result<void>::success();
}

std::string quote(const std::string& s) { return "\"" + s + "\""; }
} // namespace

Result<ProtocolEndpointConfigSet> ProtocolConfigLoader::loadAll(const std::string& configDir) {
    ProtocolEndpointConfigSet out;

    auto gb = loadGb28181(joinPath(configDir, "gb28181.yaml"));
    if (!gb) return Result<ProtocolEndpointConfigSet>::error(gb.status().code(), gb.status().describe());
    out.gb28181 = gb.value();

    auto onvif = loadOnvif(joinPath(configDir, "onvif.yaml"));
    if (!onvif) return Result<ProtocolEndpointConfigSet>::error(onvif.status().code(), onvif.status().describe());
    out.onvif = onvif.value();

    auto privateApi = loadPrivateApi(joinPath(configDir, "private_api.yaml"));
    if (!privateApi) return Result<ProtocolEndpointConfigSet>::error(privateApi.status().code(), privateApi.status().describe());
    out.privateApi = privateApi.value();

    auto rtsp = loadRtsp(joinPath(configDir, "rtsp.yaml"));
    if (!rtsp) return Result<ProtocolEndpointConfigSet>::error(rtsp.status().code(), rtsp.status().describe());
    out.rtsp = rtsp.value();

    auto v = validate(out);
    if (!v) return Result<ProtocolEndpointConfigSet>::error(v.status().code(), v.status().describe());
    return Result<ProtocolEndpointConfigSet>::ok(std::move(out));
}

Result<Gb28181Config> ProtocolConfigLoader::loadGb28181(const std::string& path) {
    auto cfg = parseOptionalYaml(path);
    if (!cfg) return Result<Gb28181Config>::error(cfg.status().code(), cfg.status().describe());

    Gb28181Config out;
    const auto& m = cfg.value();
    out.enable = toBool(m, "gb28181.enable", out.enable);
    out.local.deviceId = get(m, "gb28181.local.device_id", out.local.deviceId);
    out.local.domain = get(m, "gb28181.local.domain", out.local.domain);
    out.local.ip = get(m, "gb28181.local.ip", out.local.ip);
    out.local.port = toPort(m, "gb28181.local.port", out.local.port);
    out.local.password = get(m, "gb28181.local.password", out.local.password);
    out.local.manufacturer = get(m, "gb28181.local.manufacturer", out.local.manufacturer);
    out.local.model = get(m, "gb28181.local.model", out.local.model);
    out.local.firmware = get(m, "gb28181.local.firmware", out.local.firmware);

    out.channel.id = get(m, "gb28181.channel.id", out.channel.id);
    out.channel.name = get(m, "gb28181.channel.name", out.channel.name);

    out.server.id = get(m, "gb28181.server.id", out.server.id);
    out.server.domain = get(m, "gb28181.server.domain", out.server.domain);
    out.server.ip = get(m, "gb28181.server.ip", out.server.ip);
    out.server.port = toPort(m, "gb28181.server.port", out.server.port);
    out.server.password = get(m, "gb28181.server.password", out.server.password);

    out.reg.expiresSec = toInt(m, "gb28181.register.expires_sec", out.reg.expiresSec);
    out.reg.retryIntervalSec = toInt(m, "gb28181.register.retry_interval_sec", out.reg.retryIntervalSec);
    out.keepalive.intervalSec = toInt(m, "gb28181.keepalive.interval_sec", out.keepalive.intervalSec);
    out.keepalive.timeoutSec = toInt(m, "gb28181.keepalive.timeout_sec", out.keepalive.timeoutSec);

    out.media.rtpLocalPort = toPort(m, "gb28181.media.rtp_local_port", out.media.rtpLocalPort);
    out.media.ssrc = get(m, "gb28181.media.ssrc", out.media.ssrc);
    out.media.transport = lower(get(m, "gb28181.media.transport", out.media.transport));

    auto v = validateGb28181(out);
    if (!v) return Result<Gb28181Config>::error(v.status().code(), v.status().describe());
    return Result<Gb28181Config>::ok(std::move(out));
}

Result<OnvifConfig> ProtocolConfigLoader::loadOnvif(const std::string& path) {
    auto cfg = parseOptionalYaml(path);
    if (!cfg) return Result<OnvifConfig>::error(cfg.status().code(), cfg.status().describe());
    OnvifConfig out;
    const auto& m = cfg.value();
    out.enable = toBool(m, "onvif.enable", out.enable);
    out.listenIp = get(m, "onvif.listen.ip", out.listenIp);
    out.listenPort = toPort(m, "onvif.listen.port", out.listenPort);
    out.authEnable = toBool(m, "onvif.auth.enable", out.authEnable);
    out.username = get(m, "onvif.auth.username", out.username);
    out.password = get(m, "onvif.auth.password", out.password);
    out.rtspAdvertiseIp = get(m, "onvif.rtsp.advertise_ip", out.rtspAdvertiseIp);
    out.rtspPort = toPort(m, "onvif.rtsp.port", out.rtspPort);
    out.rtspPath = get(m, "onvif.rtsp.path", out.rtspPath);
    auto v = validateOnvif(out);
    if (!v) return Result<OnvifConfig>::error(v.status().code(), v.status().describe());
    return Result<OnvifConfig>::ok(std::move(out));
}

Result<PrivateApiConfig> ProtocolConfigLoader::loadPrivateApi(const std::string& path) {
    auto cfg = parseOptionalYaml(path);
    if (!cfg) return Result<PrivateApiConfig>::error(cfg.status().code(), cfg.status().describe());
    PrivateApiConfig out;
    const auto& m = cfg.value();
    out.enable = toBool(m, "private_api.enable", out.enable);
    out.listenIp = get(m, "private_api.listen.ip", out.listenIp);
    out.listenPort = toPort(m, "private_api.listen.port", out.listenPort);
    out.authEnable = toBool(m, "private_api.auth.enable", out.authEnable);
    out.username = get(m, "private_api.auth.username", out.username);
    out.password = get(m, "private_api.auth.password", out.password);
    auto v = validatePrivateApi(out);
    if (!v) return Result<PrivateApiConfig>::error(v.status().code(), v.status().describe());
    return Result<PrivateApiConfig>::ok(std::move(out));
}

Result<RtspConfig> ProtocolConfigLoader::loadRtsp(const std::string& path) {
    auto cfg = parseOptionalYaml(path);
    if (!cfg) return Result<RtspConfig>::error(cfg.status().code(), cfg.status().describe());
    RtspConfig out;
    const auto& m = cfg.value();
    out.enable = toBool(m, "rtsp.enable", out.enable);
    out.bindIp = get(m, "rtsp.bind_ip", out.bindIp);
    out.advertiseIp = get(m, "rtsp.advertise_ip", out.advertiseIp);
    out.port = toPort(m, "rtsp.port", out.port);
    out.path = get(m, "rtsp.path", out.path);
    out.authEnable = toBool(m, "rtsp.auth.enable", out.authEnable);
    out.username = get(m, "rtsp.auth.username", out.username);
    out.password = get(m, "rtsp.auth.password", out.password);
    auto v = validateRtsp(out);
    if (!v) return Result<RtspConfig>::error(v.status().code(), v.status().describe());
    return Result<RtspConfig>::ok(std::move(out));
}

Result<void> ProtocolConfigLoader::validate(const ProtocolEndpointConfigSet& config) {
    auto r1 = validateGb28181(config.gb28181); if (!r1) return r1;
    auto r2 = validateOnvif(config.onvif); if (!r2) return r2;
    auto r3 = validatePrivateApi(config.privateApi); if (!r3) return r3;
    auto r4 = validateRtsp(config.rtsp); if (!r4) return r4;
    return Result<void>::success();
}

Result<void> ProtocolConfigLoader::validateGb28181(const Gb28181Config& config) {
    if (!config.enable) return Result<void>::success();
    auto r1 = checkNotEmpty(config.local.deviceId, "gb28181.local.device_id"); if (!r1) return r1;
    auto r2 = checkNotEmpty(config.local.domain, "gb28181.local.domain"); if (!r2) return r2;
    auto r3 = checkNotEmpty(config.local.ip, "gb28181.local.ip"); if (!r3) return r3;
    auto r4 = checkPort(config.local.port, "gb28181.local.port"); if (!r4) return r4;
    auto r5 = checkNotEmpty(config.server.id, "gb28181.server.id"); if (!r5) return r5;
    auto r6 = checkNotEmpty(config.server.domain, "gb28181.server.domain"); if (!r6) return r6;
    auto r7 = checkNotEmpty(config.server.ip, "gb28181.server.ip"); if (!r7) return r7;
    auto r8 = checkPort(config.server.port, "gb28181.server.port"); if (!r8) return r8;
    if (config.reg.expiresSec <= 0) return Result<void>::error(ErrorCode::ConfigError, "gb28181.register.expires_sec must be positive");
    if (config.keepalive.intervalSec <= 0) return Result<void>::error(ErrorCode::ConfigError, "gb28181.keepalive.interval_sec must be positive");
    if (config.media.transport != "udp" && config.media.transport != "tcp") {
        return Result<void>::error(ErrorCode::ConfigError, "gb28181.media.transport must be udp or tcp");
    }
    return Result<void>::success();
}

Result<void> ProtocolConfigLoader::validateOnvif(const OnvifConfig& config) {
    if (!config.enable) return Result<void>::success();
    auto r1 = checkNotEmpty(config.listenIp, "onvif.listen.ip"); if (!r1) return r1;
    auto r2 = checkPort(config.listenPort, "onvif.listen.port"); if (!r2) return r2;
    auto r3 = checkNotEmpty(config.rtspPath, "onvif.rtsp.path"); if (!r3) return r3;
    return checkPort(config.rtspPort, "onvif.rtsp.port");
}

Result<void> ProtocolConfigLoader::validatePrivateApi(const PrivateApiConfig& config) {
    if (!config.enable) return Result<void>::success();
    auto r1 = checkNotEmpty(config.listenIp, "private_api.listen.ip"); if (!r1) return r1;
    return checkPort(config.listenPort, "private_api.listen.port");
}

Result<void> ProtocolConfigLoader::validateRtsp(const RtspConfig& config) {
    if (!config.enable) return Result<void>::success();
    auto r1 = checkNotEmpty(config.bindIp, "rtsp.bind_ip"); if (!r1) return r1;
    auto r2 = checkNotEmpty(config.path, "rtsp.path"); if (!r2) return r2;
    return checkPort(config.port, "rtsp.port");
}

Result<void> ProtocolConfigLoader::saveGb28181(const std::string& path, const Gb28181Config& c) {
    auto v = validateGb28181(c); if (!v) return v;
    std::ostringstream os;
    os << "gb28181:\n"
       << "  enable: " << (c.enable ? "true" : "false") << "\n\n"
       << "  local:\n"
       << "    device_id: " << quote(c.local.deviceId) << "\n"
       << "    domain: " << quote(c.local.domain) << "\n"
       << "    ip: " << quote(c.local.ip) << "\n"
       << "    port: " << c.local.port << "\n"
       << "    password: " << quote(c.local.password) << "\n"
       << "    manufacturer: " << quote(c.local.manufacturer) << "\n"
       << "    model: " << quote(c.local.model) << "\n"
       << "    firmware: " << quote(c.local.firmware) << "\n\n"
       << "  channel:\n"
       << "    id: " << quote(c.channel.id) << "\n"
       << "    name: " << quote(c.channel.name) << "\n\n"
       << "  server:\n"
       << "    id: " << quote(c.server.id) << "\n"
       << "    domain: " << quote(c.server.domain) << "\n"
       << "    ip: " << quote(c.server.ip) << "\n"
       << "    port: " << c.server.port << "\n"
       << "    password: " << quote(c.server.password) << "\n\n"
       << "  register:\n"
       << "    expires_sec: " << c.reg.expiresSec << "\n"
       << "    retry_interval_sec: " << c.reg.retryIntervalSec << "\n\n"
       << "  keepalive:\n"
       << "    interval_sec: " << c.keepalive.intervalSec << "\n"
       << "    timeout_sec: " << c.keepalive.timeoutSec << "\n\n"
       << "  media:\n"
       << "    rtp_local_port: " << c.media.rtpLocalPort << "\n"
       << "    ssrc: " << quote(c.media.ssrc) << "\n"
       << "    transport: " << quote(c.media.transport) << "\n";
    return tri::foundation::file::writeText(path, os.str());
}

Result<void> ProtocolConfigLoader::saveOnvif(const std::string& path, const OnvifConfig& c) {
    auto v = validateOnvif(c); if (!v) return v;
    std::ostringstream os;
    os << "onvif:\n"
       << "  enable: " << (c.enable ? "true" : "false") << "\n"
       << "  listen:\n"
       << "    ip: " << quote(c.listenIp) << "\n"
       << "    port: " << c.listenPort << "\n"
       << "  auth:\n"
       << "    enable: " << (c.authEnable ? "true" : "false") << "\n"
       << "    username: " << quote(c.username) << "\n"
       << "    password: " << quote(c.password) << "\n"
       << "  rtsp:\n"
       << "    advertise_ip: " << quote(c.rtspAdvertiseIp) << "\n"
       << "    port: " << c.rtspPort << "\n"
       << "    path: " << quote(c.rtspPath) << "\n";
    return tri::foundation::file::writeText(path, os.str());
}

Result<void> ProtocolConfigLoader::savePrivateApi(const std::string& path, const PrivateApiConfig& c) {
    auto v = validatePrivateApi(c); if (!v) return v;
    std::ostringstream os;
    os << "private_api:\n"
       << "  enable: " << (c.enable ? "true" : "false") << "\n"
       << "  listen:\n"
       << "    ip: " << quote(c.listenIp) << "\n"
       << "    port: " << c.listenPort << "\n"
       << "  auth:\n"
       << "    enable: " << (c.authEnable ? "true" : "false") << "\n"
       << "    username: " << quote(c.username) << "\n"
       << "    password: " << quote(c.password) << "\n";
    return tri::foundation::file::writeText(path, os.str());
}

Result<void> ProtocolConfigLoader::saveRtsp(const std::string& path, const RtspConfig& c) {
    auto v = validateRtsp(c); if (!v) return v;
    std::ostringstream os;
    os << "rtsp:\n"
       << "  enable: " << (c.enable ? "true" : "false") << "\n"
       << "  bind_ip: " << quote(c.bindIp) << "\n"
       << "  advertise_ip: " << quote(c.advertiseIp) << "\n"
       << "  port: " << c.port << "\n"
       << "  path: " << quote(c.path) << "\n"
       << "  auth:\n"
       << "    enable: " << (c.authEnable ? "true" : "false") << "\n"
       << "    username: " << quote(c.username) << "\n"
       << "    password: " << quote(c.password) << "\n";
    return tri::foundation::file::writeText(path, os.str());
}

} // namespace tri::protocol
