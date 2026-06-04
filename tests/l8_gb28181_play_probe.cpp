#include "protocol/ProtocolConfigLoader.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/md5.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

struct InviteInfo {
    std::string raw;
    std::string callId;
    std::string from;
    std::string to;
    std::string via;
    std::string contact;
    std::string cseq;
    std::string mediaIp;
    int mediaPort{0};
    std::string ssrc;
    bool tcpMode{false};
};

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

std::string md5Hex(const std::string& text) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(text.data()), text.size(), digest);

    std::ostringstream oss;
    for (unsigned char c : digest) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(c);
    }
    return oss.str();
}

std::string getHeaderLine(const std::string& text, const std::string& name) {
    std::istringstream in(text);
    std::string line;
    const std::string prefix = name + ":";

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        auto lowerLine = line;
        auto lowerPrefix = prefix;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
        std::transform(lowerPrefix.begin(), lowerPrefix.end(), lowerPrefix.begin(), ::tolower);

        if (lowerLine.rfind(lowerPrefix, 0) == 0) {
            const auto pos = line.find(':');
            return pos == std::string::npos ? "" : line.substr(pos + 1);
        }
    }

    return {};
}

std::string getHeaderParam(const std::string& text, const std::string& key) {
    const std::string pattern = key + "=\"?([^\",\\r\\n]+)\"?";
    std::regex re(pattern, std::regex::icase);

    std::smatch m;
    if (std::regex_search(text, m, re) && m.size() >= 2) {
        return m[1].str();
    }

    return {};
}

std::string getXmlValue(const std::string& xml, const std::string& tag) {
    const std::string begin = "<" + tag + ">";
    const std::string end = "</" + tag + ">";

    auto p1 = xml.find(begin);
    if (p1 == std::string::npos) return {};
    p1 += begin.size();

    auto p2 = xml.find(end, p1);
    if (p2 == std::string::npos) return {};

    return trim(xml.substr(p1, p2 - p1));
}

bool containsStatus(const std::string& text, int code) {
    return text.find("SIP/2.0 " + std::to_string(code)) != std::string::npos;
}

std::string statusLine(const std::string& text) {
    auto pos = text.find("\r\n");
    if (pos == std::string::npos) return text;
    return text.substr(0, pos);
}

std::string makeBranch() {
    return "z9hG4bK-trifusion-" + std::to_string(std::time(nullptr));
}

std::string makeCallId(const std::string& deviceId) {
    return deviceId + "-" + std::to_string(std::time(nullptr)) + "@tri-fusion-camera";
}

bool sendPacket(int fd,
                const sockaddr_in& dst,
                const std::string& packet,
                const std::string& title,
                bool printBody = true) {
    auto n = sendto(fd,
                    packet.data(),
                    packet.size(),
                    0,
                    reinterpret_cast<const sockaddr*>(&dst),
                    sizeof(dst));
    if (n < 0) {
        std::cerr << "[FAIL] send " << title << ": " << std::strerror(errno) << "\n";
        return false;
    }

    std::cout << "\n========== SEND " << title << " ==========" << "\n";
    if (printBody) {
        std::cout << packet << "\n";
    } else {
        std::cout << "bytes=" << packet.size() << "\n";
    }

    return true;
}

bool recvPacket(int fd, std::string& out, sockaddr_in* from, int timeoutMs) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    const int ret = select(fd + 1, &rfds, nullptr, nullptr, &tv);
    if (ret <= 0) return false;

    char buf[8192]{};
    sockaddr_in src{};
    socklen_t srcLen = sizeof(src);

    const ssize_t n = recvfrom(fd,
                               buf,
                               sizeof(buf) - 1,
                               0,
                               reinterpret_cast<sockaddr*>(&src),
                               &srcLen);
    if (n <= 0) return false;

    out.assign(buf, static_cast<std::size_t>(n));
    if (from != nullptr) *from = src;

    std::cout << "\n========== RECV ==========" << "\n";
    std::cout << out << "\n";

    return true;
}

std::string buildRegisterNoAuth(const tri::protocol::Gb28181Config& cfg,
                                int cseq,
                                const std::string& branch,
                                const std::string& callId) {
    const std::string uri = "sip:" + cfg.server.id + "@" + cfg.server.domain;

    std::ostringstream ss;
    ss << "REGISTER " << uri << " SIP/2.0\r\n";
    ss << "Via: SIP/2.0/UDP " << cfg.local.ip << ":" << cfg.local.port
       << ";branch=" << branch << ";rport\r\n";
    ss << "From: <sip:" << cfg.local.deviceId << "@" << cfg.local.domain
       << ">;tag=tri-fusion\r\n";
    ss << "To: <sip:" << cfg.local.deviceId << "@" << cfg.local.domain << ">\r\n";
    ss << "Call-ID: " << callId << "\r\n";
    ss << "CSeq: " << cseq << " REGISTER\r\n";
    ss << "Contact: <sip:" << cfg.local.deviceId << "@" << cfg.local.ip
       << ":" << cfg.local.port << ">\r\n";
    ss << "Max-Forwards: 70\r\n";
    ss << "User-Agent: TriFusionCamera-L8-GB28181-Play-Probe\r\n";
    ss << "Expires: " << cfg.reg.expiresSec << "\r\n";
    ss << "Content-Length: 0\r\n\r\n";
    return ss.str();
}

std::string buildRegisterAuth(const tri::protocol::Gb28181Config& cfg,
                              int cseq,
                              const std::string& branch,
                              const std::string& callId,
                              const std::string& realm,
                              const std::string& nonce,
                              const std::string& qop) {
    const std::string method = "REGISTER";
    const std::string uri = "sip:" + cfg.server.id + "@" + cfg.server.domain;

    const std::string username = cfg.local.deviceId;
    const std::string password = cfg.server.password.empty()
                                     ? cfg.local.password
                                     : cfg.server.password;

    const std::string ha1 = md5Hex(username + ":" + realm + ":" + password);
    const std::string ha2 = md5Hex(method + ":" + uri);

    std::string response;
    std::string authExtra;

    if (!qop.empty()) {
        const std::string nc = "00000001";
        const std::string cnonce = "tri-fusion-cnonce";
        response = md5Hex(ha1 + ":" + nonce + ":" + nc + ":" + cnonce + ":" + qop + ":" + ha2);
        authExtra = ", qop=" + qop + ", nc=" + nc + ", cnonce=\"" + cnonce + "\"";
    } else {
        response = md5Hex(ha1 + ":" + nonce + ":" + ha2);
    }

    std::ostringstream ss;
    ss << "REGISTER " << uri << " SIP/2.0\r\n";
    ss << "Via: SIP/2.0/UDP " << cfg.local.ip << ":" << cfg.local.port
       << ";branch=" << branch << ";rport\r\n";
    ss << "From: <sip:" << cfg.local.deviceId << "@" << cfg.local.domain
       << ">;tag=tri-fusion\r\n";
    ss << "To: <sip:" << cfg.local.deviceId << "@" << cfg.local.domain << ">\r\n";
    ss << "Call-ID: " << callId << "\r\n";
    ss << "CSeq: " << cseq << " REGISTER\r\n";
    ss << "Contact: <sip:" << cfg.local.deviceId << "@" << cfg.local.ip
       << ":" << cfg.local.port << ">\r\n";
    ss << "Max-Forwards: 70\r\n";
    ss << "User-Agent: TriFusionCamera-L8-GB28181-Play-Probe\r\n";
    ss << "Expires: " << cfg.reg.expiresSec << "\r\n";
    ss << "Authorization: Digest username=\"" << username
       << "\", realm=\"" << realm
       << "\", nonce=\"" << nonce
       << "\", uri=\"" << uri
       << "\", response=\"" << response
       << "\", algorithm=MD5"
       << authExtra << "\r\n";
    ss << "Content-Length: 0\r\n\r\n";

    return ss.str();
}

std::string buildKeepaliveMessage(const tri::protocol::Gb28181Config& cfg,
                                  int cseq,
                                  const std::string& branch,
                                  const std::string& callId,
                                  int sn) {
    std::ostringstream body;
    body << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n";
    body << "<Notify>\r\n";
    body << "<CmdType>Keepalive</CmdType>\r\n";
    body << "<SN>" << sn << "</SN>\r\n";
    body << "<DeviceID>" << cfg.local.deviceId << "</DeviceID>\r\n";
    body << "<Status>OK</Status>\r\n";
    body << "</Notify>\r\n";

    const std::string bodyText = body.str();
    const std::string uri = "sip:" + cfg.server.id + "@" + cfg.server.domain;

    std::ostringstream ss;
    ss << "MESSAGE " << uri << " SIP/2.0\r\n";
    ss << "Via: SIP/2.0/UDP " << cfg.local.ip << ":" << cfg.local.port
       << ";branch=" << branch << ";rport\r\n";
    ss << "From: <sip:" << cfg.local.deviceId << "@" << cfg.local.domain
       << ">;tag=tri-fusion-keepalive\r\n";
    ss << "To: <sip:" << cfg.server.id << "@" << cfg.server.domain << ">\r\n";
    ss << "Call-ID: " << callId << "\r\n";
    ss << "CSeq: " << cseq << " MESSAGE\r\n";
    ss << "Max-Forwards: 70\r\n";
    ss << "User-Agent: TriFusionCamera-L8-GB28181-Play-Probe\r\n";
    ss << "Content-Type: Application/MANSCDP+xml\r\n";
    ss << "Content-Length: " << bodyText.size() << "\r\n\r\n";
    ss << bodyText;
    return ss.str();
}

std::string buildCatalogResponseXml(const tri::protocol::Gb28181Config& cfg, int sn) {
    std::ostringstream body;
    body << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n";
    body << "<Response>\r\n";
    body << "<CmdType>Catalog</CmdType>\r\n";
    body << "<SN>" << sn << "</SN>\r\n";
    body << "<DeviceID>" << cfg.local.deviceId << "</DeviceID>\r\n";
    body << "<SumNum>1</SumNum>\r\n";
    body << "<DeviceList Num=\"1\">\r\n";
    body << "<Item>\r\n";
    body << "<DeviceID>" << cfg.channel.id << "</DeviceID>\r\n";
    body << "<Name>" << cfg.channel.name << "</Name>\r\n";
    body << "<Manufacturer>" << cfg.local.manufacturer << "</Manufacturer>\r\n";
    body << "<Model>" << cfg.local.model << "</Model>\r\n";
    body << "<Owner>TriFusion</Owner>\r\n";
    body << "<CivilCode>" << cfg.local.domain << "</CivilCode>\r\n";
    body << "<Address>TriFusion Camera</Address>\r\n";
    body << "<Parental>0</Parental>\r\n";
    body << "<ParentID>" << cfg.local.deviceId << "</ParentID>\r\n";
    body << "<SafetyWay>0</SafetyWay>\r\n";
    body << "<RegisterWay>1</RegisterWay>\r\n";
    body << "<Secrecy>0</Secrecy>\r\n";
    body << "<Status>ON</Status>\r\n";
    body << "</Item>\r\n";
    body << "</DeviceList>\r\n";
    body << "</Response>\r\n";
    return body.str();
}

std::string buildDeviceInfoResponseXml(const tri::protocol::Gb28181Config& cfg, int sn) {
    std::ostringstream body;
    body << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n";
    body << "<Response>\r\n";
    body << "<CmdType>DeviceInfo</CmdType>\r\n";
    body << "<SN>" << sn << "</SN>\r\n";
    body << "<DeviceID>" << cfg.local.deviceId << "</DeviceID>\r\n";
    body << "<DeviceName>" << cfg.local.model << "</DeviceName>\r\n";
    body << "<Result>OK</Result>\r\n";
    body << "<Manufacturer>" << cfg.local.manufacturer << "</Manufacturer>\r\n";
    body << "<Model>" << cfg.local.model << "</Model>\r\n";
    body << "<Firmware>" << cfg.local.firmware << "</Firmware>\r\n";
    body << "<Channel>1</Channel>\r\n";
    body << "</Response>\r\n";
    return body.str();
}

std::string buildDeviceStatusResponseXml(const tri::protocol::Gb28181Config& cfg, int sn) {
    std::ostringstream body;
    body << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n";
    body << "<Response>\r\n";
    body << "<CmdType>DeviceStatus</CmdType>\r\n";
    body << "<SN>" << sn << "</SN>\r\n";
    body << "<DeviceID>" << cfg.local.deviceId << "</DeviceID>\r\n";
    body << "<Result>OK</Result>\r\n";
    body << "<Online>ONLINE</Online>\r\n";
    body << "<Status>OK</Status>\r\n";
    body << "</Response>\r\n";
    return body.str();
}

bool sendGbXmlMessage(int fd,
                      const sockaddr_in& server,
                      const tri::protocol::Gb28181Config& cfg,
                      const std::string& xml,
                      int cseq,
                      const std::string& title) {
    const std::string uri = "sip:" + cfg.server.id + "@" + cfg.server.domain;

    std::ostringstream ss;
    ss << "MESSAGE " << uri << " SIP/2.0\r\n";
    ss << "Via: SIP/2.0/UDP " << cfg.local.ip << ":" << cfg.local.port
       << ";branch=" << makeBranch() << ";rport\r\n";
    ss << "From: <sip:" << cfg.local.deviceId << "@" << cfg.local.domain
       << ">;tag=tri-fusion-response\r\n";
    ss << "To: <sip:" << cfg.server.id << "@" << cfg.server.domain << ">\r\n";
    ss << "Call-ID: " << cfg.local.deviceId << "-" << std::time(nullptr)
       << "-response@tri-fusion-camera\r\n";
    ss << "CSeq: " << cseq << " MESSAGE\r\n";
    ss << "Max-Forwards: 70\r\n";
    ss << "User-Agent: TriFusionCamera-L8-GB28181-Play-Probe\r\n";
    ss << "Content-Type: Application/MANSCDP+xml\r\n";
    ss << "Content-Length: " << xml.size() << "\r\n\r\n";
    ss << xml;

    return sendPacket(fd, server, ss.str(), title);
}

bool answerSimpleMessage(int fd,
                         const sockaddr_in& server,
                         const std::string& msg,
                         const tri::protocol::Gb28181Config& cfg) {
    if (msg.rfind("MESSAGE ", 0) != 0) {
        return false;
    }

    const auto via = getHeaderLine(msg, "Via");
    const auto from = getHeaderLine(msg, "From");
    const auto to = getHeaderLine(msg, "To");
    const auto callId = trim(getHeaderLine(msg, "Call-ID"));
    const auto cseq = getHeaderLine(msg, "CSeq");

    std::ostringstream ok;
    ok << "SIP/2.0 200 OK\r\n";
    ok << "Via:" << via << "\r\n";
    ok << "From:" << from << "\r\n";
    ok << "To:" << to << "\r\n";
    ok << "Call-ID:" << callId << "\r\n";
    ok << "CSeq:" << cseq << "\r\n";
    ok << "User-Agent: TriFusionCamera-L8-GB28181-Play-Probe\r\n";
    ok << "Content-Length: 0\r\n\r\n";
    sendPacket(fd, server, ok.str(), "200 OK for MESSAGE");

    const auto cmdType = getXmlValue(msg, "CmdType");
    const auto snText = getXmlValue(msg, "SN");

    int sn = 1;
    if (!snText.empty()) {
        try {
            sn = std::stoi(snText);
        } catch (...) {
            sn = 1;
        }
    }

    static int responseCseq = 1000;

    if (cmdType == "Catalog") {
        std::cout << "[GB] Catalog query received, send Catalog response\n";
        const auto xml = buildCatalogResponseXml(cfg, sn);
        return sendGbXmlMessage(fd, server, cfg, xml, responseCseq++, "Catalog Response");
    }

    if (cmdType == "DeviceInfo") {
        std::cout << "[GB] DeviceInfo query received, send DeviceInfo response\n";
        const auto xml = buildDeviceInfoResponseXml(cfg, sn);
        return sendGbXmlMessage(fd, server, cfg, xml, responseCseq++, "DeviceInfo Response");
    }

    if (cmdType == "DeviceStatus") {
        std::cout << "[GB] DeviceStatus query received, send DeviceStatus response\n";
        const auto xml = buildDeviceStatusResponseXml(cfg, sn);
        return sendGbXmlMessage(fd, server, cfg, xml, responseCseq++, "DeviceStatus Response");
    }

    std::cout << "[GB] MESSAGE received, cmdType=" << cmdType
              << ", only 200 OK handled\n";
    return true;
}

std::string buildSip200ForInvite(const tri::protocol::Gb28181Config& cfg,
                                 const InviteInfo& inv,
                                 int localRtpPort) {
    const std::string ssrc = inv.ssrc.empty() ? cfg.media.ssrc : inv.ssrc;

    std::ostringstream sdp;
    sdp << "v=0\r\n";
    sdp << "o=" << cfg.local.deviceId << " 0 0 IN IP4 " << cfg.local.ip << "\r\n";
    sdp << "s=Play\r\n";
    sdp << "c=IN IP4 " << cfg.local.ip << "\r\n";
    sdp << "t=0 0\r\n";
    sdp << "m=video " << localRtpPort << " RTP/AVP 96\r\n";
    sdp << "a=sendonly\r\n";
    sdp << "a=rtpmap:96 PS/90000\r\n";
    sdp << "y=" << ssrc << "\r\n";
    sdp << "f=v/2/30/1/4096a/0/0/0\r\n";

    const auto body = sdp.str();

    std::ostringstream ss;
    ss << "SIP/2.0 200 OK\r\n";
    ss << "Via:" << inv.via << "\r\n";
    ss << "From:" << inv.from << "\r\n";
    ss << "To:" << inv.to << ";tag=tri-fusion-play\r\n";
    ss << "Call-ID:" << inv.callId << "\r\n";
    ss << "CSeq:" << inv.cseq << "\r\n";
    ss << "Contact: <sip:" << cfg.local.deviceId << "@" << cfg.local.ip
       << ":" << cfg.local.port << ">\r\n";
    ss << "User-Agent: TriFusionCamera-L8-GB28181-Play-Probe\r\n";
    ss << "Content-Type: application/sdp\r\n";
    ss << "Content-Length: " << body.size() << "\r\n\r\n";
    ss << body;
    return ss.str();
}

bool parseInvite(const std::string& msg, InviteInfo& out) {
    if (msg.rfind("INVITE ", 0) != 0) {
        return false;
    }

    out.raw = msg;
    out.callId = trim(getHeaderLine(msg, "Call-ID"));
    out.from = getHeaderLine(msg, "From");
    out.to = getHeaderLine(msg, "To");
    out.via = getHeaderLine(msg, "Via");
    out.cseq = getHeaderLine(msg, "CSeq");
    out.contact = getHeaderLine(msg, "Contact");

    out.tcpMode = msg.find("TCP/RTP") != std::string::npos ||
                  msg.find("RTP/AVP/TCP") != std::string::npos ||
                  msg.find("a=setup:") != std::string::npos;

    std::smatch m;

    std::regex cRe(R"(c=IN IP4 ([0-9.]+))");
    if (std::regex_search(msg, m, cRe) && m.size() >= 2) {
        out.mediaIp = m[1].str();
    }

    std::regex mRe(R"(m=video ([0-9]+))");
    if (std::regex_search(msg, m, mRe) && m.size() >= 2) {
        out.mediaPort = std::stoi(m[1].str());
    }

    std::regex yRe(R"(\r?\ny=([0-9]+))");
    if (std::regex_search(msg, m, yRe) && m.size() >= 2) {
        out.ssrc = m[1].str();
    }

    if (out.mediaIp.empty() || out.mediaPort <= 0) {
        std::cerr << "[FAIL] INVITE SDP missing media IP or media port\n";
        return false;
    }

    std::cout << "[INVITE] media_ip=" << out.mediaIp
              << " media_port=" << out.mediaPort
              << " ssrc=" << out.ssrc
              << " tcpMode=" << (out.tcpMode ? "true" : "false")
              << "\n";

    if (out.tcpMode) {
        std::cerr << "[WARN] INVITE requests TCP RTP, but this probe only supports UDP PS/RTP. "
                  << "Change platform stream mode to UDP first.\n";
    }

    return true;
}

std::vector<std::uint8_t> loadFile(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return {};
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(ifs),
        std::istreambuf_iterator<char>());
}

std::vector<std::vector<std::uint8_t>> splitAnnexBFrames(const std::vector<std::uint8_t>& data) {
    std::vector<std::size_t> starts;

    for (std::size_t i = 0; i + 4 < data.size(); ++i) {
        if (data[i] == 0x00 && data[i + 1] == 0x00 &&
            data[i + 2] == 0x00 && data[i + 3] == 0x01) {
            starts.push_back(i);
            i += 3;
        } else if (data[i] == 0x00 && data[i + 1] == 0x00 &&
                   data[i + 2] == 0x01) {
            starts.push_back(i);
            i += 2;
        }
    }

    std::vector<std::vector<std::uint8_t>> frames;
    if (starts.empty()) {
        frames.push_back(data);
        return frames;
    }

    std::vector<std::uint8_t> current;
    bool currentHasVcl = false;

    for (std::size_t idx = 0; idx < starts.size(); ++idx) {
        const auto begin = starts[idx];
        const auto end = idx + 1 < starts.size() ? starts[idx + 1] : data.size();

        std::size_t nalOffset = begin;
        if (begin + 4 <= data.size() &&
            data[begin] == 0x00 && data[begin + 1] == 0x00 &&
            data[begin + 2] == 0x00 && data[begin + 3] == 0x01) {
            nalOffset = begin + 4;
        } else {
            nalOffset = begin + 3;
        }

        if (nalOffset >= end) continue;

        const std::uint8_t nalType = data[nalOffset] & 0x1f;
        const bool isVcl = nalType >= 1 && nalType <= 5;

        if (isVcl && currentHasVcl && !current.empty()) {
            frames.push_back(std::move(current));
            current.clear();
            currentHasVcl = false;
        }

        current.insert(current.end(), data.begin() + begin, data.begin() + end);
        if (isVcl) currentHasVcl = true;
    }

    if (!current.empty()) frames.push_back(std::move(current));
    return frames;
}

void append32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xff));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xff));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
    out.push_back(static_cast<std::uint8_t>(v & 0xff));
}

void append16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
    out.push_back(static_cast<std::uint8_t>(v & 0xff));
}

std::uint64_t to90k(std::uint64_t frameIndex, int fps) {
    return frameIndex * 90000ULL / static_cast<std::uint64_t>(fps);
}

void appendPesTimestamp(std::vector<std::uint8_t>& out, std::uint8_t fb, std::uint64_t ts90k) {
    const std::uint64_t v = ts90k & 0x1ffffffffULL;
    out.push_back(static_cast<std::uint8_t>(
        (fb << 4) | (((v >> 30) & 0x07) << 1) | 1));
    out.push_back(static_cast<std::uint8_t>((v >> 22) & 0xff));
    out.push_back(static_cast<std::uint8_t>((((v >> 15) & 0x7f) << 1) | 1));
    out.push_back(static_cast<std::uint8_t>((v >> 7) & 0xff));
    out.push_back(static_cast<std::uint8_t>(((v & 0x7f) << 1) | 1));
}

std::vector<std::uint8_t> makePsPacket(const std::vector<std::uint8_t>& h264Frame,
                                       std::uint64_t frameIndex,
                                       int fps) {
    const auto ts = to90k(frameIndex, fps);

    std::vector<std::uint8_t> ps;
    ps.reserve(h264Frame.size() + 128);

    append32(ps, 0x000001BA);
    ps.push_back(0x44);
    ps.push_back(static_cast<std::uint8_t>(((ts >> 27) & 0x38) | 0x04 | ((ts >> 28) & 0x03)));
    ps.push_back(static_cast<std::uint8_t>((ts >> 20) & 0xff));
    ps.push_back(static_cast<std::uint8_t>((((ts >> 15) & 0x1f) << 3) | 0x04 | ((ts >> 13) & 0x03)));
    ps.push_back(static_cast<std::uint8_t>((ts >> 5) & 0xff));
    ps.push_back(static_cast<std::uint8_t>(((ts & 0x1f) << 3) | 0x04));
    ps.push_back(0x01);
    ps.push_back(0x89);
    ps.push_back(0xc3);
    ps.push_back(0xf8);

    static const std::uint8_t sysHeader[] = {
        0x00, 0x00, 0x01, 0xBB,
        0x00, 0x0C,
        0x80, 0x04, 0x04, 0xE1, 0x7F, 0xE0,
        0xE0, 0xE8, 0xC0, 0xC0,
        0xC0, 0x20
    };
    ps.insert(ps.end(), std::begin(sysHeader), std::end(sysHeader));

    static const std::uint8_t psm[] = {
        0x00, 0x00, 0x01, 0xBC,
        0x00, 0x12,
        0xE1, 0xFF,
        0x00, 0x00,
        0x00, 0x08,
        0x1B, 0xE0, 0x00, 0x00,
        0x90, 0xC0, 0x00, 0x00,
        0x2A, 0xB1, 0x04, 0xB2
    };
    ps.insert(ps.end(), std::begin(psm), std::end(psm));

    append32(ps, 0x000001E0);

    const std::size_t pesPayloadLen = h264Frame.size() + 8;
    const std::uint16_t pesLen = pesPayloadLen > 0xffff
                                     ? 0
                                     : static_cast<std::uint16_t>(pesPayloadLen);
    append16(ps, pesLen);

    ps.push_back(0x80);
    ps.push_back(0x80);
    ps.push_back(0x05);
    appendPesTimestamp(ps, 0x02, ts);

    ps.insert(ps.end(), h264Frame.begin(), h264Frame.end());
    return ps;
}

std::vector<std::uint8_t> makeRtpPacket(const std::uint8_t* payload,
                                        std::size_t payloadLen,
                                        std::uint16_t seq,
                                        std::uint32_t timestamp,
                                        std::uint32_t ssrc,
                                        bool marker) {
    std::vector<std::uint8_t> rtp;
    rtp.reserve(payloadLen + 12);

    rtp.push_back(0x80);
    rtp.push_back(static_cast<std::uint8_t>((marker ? 0x80 : 0x00) | 96));
    rtp.push_back(static_cast<std::uint8_t>((seq >> 8) & 0xff));
    rtp.push_back(static_cast<std::uint8_t>(seq & 0xff));
    rtp.push_back(static_cast<std::uint8_t>((timestamp >> 24) & 0xff));
    rtp.push_back(static_cast<std::uint8_t>((timestamp >> 16) & 0xff));
    rtp.push_back(static_cast<std::uint8_t>((timestamp >> 8) & 0xff));
    rtp.push_back(static_cast<std::uint8_t>(timestamp & 0xff));
    rtp.push_back(static_cast<std::uint8_t>((ssrc >> 24) & 0xff));
    rtp.push_back(static_cast<std::uint8_t>((ssrc >> 16) & 0xff));
    rtp.push_back(static_cast<std::uint8_t>((ssrc >> 8) & 0xff));
    rtp.push_back(static_cast<std::uint8_t>(ssrc & 0xff));

    rtp.insert(rtp.end(), payload, payload + payloadLen);
    return rtp;
}

std::uint32_t parseSsrc(const std::string& text) {
    if (text.empty()) return 1;
    try {
        return static_cast<std::uint32_t>(std::stoul(text));
    } catch (...) {
        return 1;
    }
}

bool streamH264AsGbPsRtp(const std::string& h264Path,
                         const std::string& dstIp,
                         int dstPort,
                         const std::string& ssrcText,
                         int fps,
                         int loopCount) {
    const auto file = loadFile(h264Path);
    if (file.empty()) {
        std::cerr << "[FAIL] load h264 file failed or empty: " << h264Path << "\n";
        return false;
    }

    auto frames = splitAnnexBFrames(file);
    if (frames.empty()) {
        std::cerr << "[FAIL] no H264 frames found in " << h264Path << "\n";
        return false;
    }

    std::cout << "[RTP] loaded h264 frames=" << frames.size()
              << " dst=" << dstIp << ":" << dstPort
              << " ssrc=" << ssrcText << "\n";

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        std::cerr << "[FAIL] RTP socket: " << std::strerror(errno) << "\n";
        return false;
    }

    int sendBuf = 4 * 1024 * 1024;
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendBuf, sizeof(sendBuf));

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(dstPort);
    if (inet_pton(AF_INET, dstIp.c_str(), &dst.sin_addr) != 1) {
        std::cerr << "[FAIL] invalid RTP dst ip: " << dstIp << "\n";
        close(fd);
        return false;
    }

    std::uint16_t seq = 0;
    const std::uint32_t ssrc = parseSsrc(ssrcText);
    const std::size_t maxPayload = 1200;
    std::uint64_t frameIndex = 0;

    const auto startTime = std::chrono::steady_clock::now();
    const auto frameInterval = std::chrono::nanoseconds(1000000000LL / fps);

    for (int loop = 0; loop < loopCount; ++loop) {
        for (const auto& frame : frames) {
            const auto frameStartTime = startTime + frameInterval * frameIndex;

            auto ps = makePsPacket(frame, frameIndex, fps);
            const auto ts = static_cast<std::uint32_t>(to90k(frameIndex, fps));

            const std::size_t packetCount = (ps.size() + maxPayload - 1) / maxPayload;
            const auto packetInterval = packetCount > 1
                                            ? frameInterval / static_cast<int>(packetCount)
                                            : std::chrono::nanoseconds(0);

            std::size_t offset = 0;
            std::size_t packetIndex = 0;

            while (offset < ps.size()) {
                const auto sendTime = frameStartTime +
                                      packetInterval * static_cast<int>(packetIndex);
                std::this_thread::sleep_until(sendTime);

                const auto chunk = std::min(maxPayload, ps.size() - offset);
                const bool marker = offset + chunk >= ps.size();

                auto rtp = makeRtpPacket(ps.data() + offset,
                                         chunk,
                                         seq++,
                                         ts,
                                         ssrc,
                                         marker);

                auto n = sendto(fd,
                                rtp.data(),
                                rtp.size(),
                                0,
                                reinterpret_cast<const sockaddr*>(&dst),
                                sizeof(dst));
                if (n < 0) {
                    std::cerr << "[FAIL] RTP sendto: " << std::strerror(errno) << "\n";
                    close(fd);
                    return false;
                }

                offset += chunk;
                ++packetIndex;
            }

            ++frameIndex;
            const auto nextFrameTime = startTime + frameInterval * frameIndex;
            std::this_thread::sleep_until(nextFrameTime);
        }
    }

    close(fd);
    std::cout << "[RTP] stream finished\n";
    return true;
}

std::vector<std::vector<std::uint8_t>> splitPsPacks(const std::vector<std::uint8_t>& data) {
    std::vector<std::size_t> starts;

    for (std::size_t i = 0; i + 4 <= data.size(); ++i) {
        if (data[i] == 0x00 &&
            data[i + 1] == 0x00 &&
            data[i + 2] == 0x01 &&
            data[i + 3] == 0xBA) {
            starts.push_back(i);
            i += 3;
        }
    }

    std::vector<std::vector<std::uint8_t>> packs;

    if (starts.empty()) {
        packs.push_back(data);
        return packs;
    }

    for (std::size_t i = 0; i < starts.size(); ++i) {
        const std::size_t begin = starts[i];
        const std::size_t end = i + 1 < starts.size() ? starts[i + 1] : data.size();

        if (end > begin) {
            packs.emplace_back(data.begin() + begin, data.begin() + end);
        }
    }

    return packs;
}

bool streamPsFileAsRtp(const std::string& psPath,
                       const std::string& dstIp,
                       int dstPort,
                       const std::string& ssrcText,
                       int fps,
                       int loopCount) {
    const auto psFile = loadFile(psPath);
    if (psFile.empty()) {
        std::cerr << "[FAIL] load PS file failed or empty: " << psPath << "\n";
        return false;
    }

    auto packs = splitPsPacks(psFile);
    if (packs.empty()) {
        std::cerr << "[FAIL] no PS packs found in file: " << psPath << "\n";
        return false;
    }

    std::cout << "[RTP-PS] loaded ps file bytes=" << psFile.size()
              << " packs=" << packs.size()
              << " dst=" << dstIp << ":" << dstPort
              << " ssrc=" << ssrcText
              << "\n";

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        std::cerr << "[FAIL] RTP socket: " << std::strerror(errno) << "\n";
        return false;
    }

    int sendBuf = 4 * 1024 * 1024;
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendBuf, sizeof(sendBuf));

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(dstPort);
    if (inet_pton(AF_INET, dstIp.c_str(), &dst.sin_addr) != 1) {
        std::cerr << "[FAIL] invalid RTP dst ip: " << dstIp << "\n";
        close(fd);
        return false;
    }

    const std::size_t maxPayload = 1200;
    const std::uint32_t ssrc = parseSsrc(ssrcText);
    std::uint16_t seq = 0;
    std::uint64_t frameIndex = 0;

    const auto startTime = std::chrono::steady_clock::now();
    const auto frameInterval = std::chrono::nanoseconds(1000000000LL / fps);

    for (int loop = 0; loop < loopCount; ++loop) {
        for (const auto& pack : packs) {
            const auto frameStartTime = startTime + frameInterval * frameIndex;
            const auto rtpTs = static_cast<std::uint32_t>(to90k(frameIndex, fps));

            const std::size_t packetCount =
                (pack.size() + maxPayload - 1) / maxPayload;

            const auto packetInterval =
                packetCount > 1
                    ? frameInterval / static_cast<int>(packetCount)
                    : std::chrono::nanoseconds(0);

            std::size_t offset = 0;
            std::size_t packetIndex = 0;

            while (offset < pack.size()) {
                const auto chunk = std::min(maxPayload, pack.size() - offset);
                const bool marker = offset + chunk >= pack.size();

                const auto sendTime =
                    frameStartTime + packetInterval * static_cast<int>(packetIndex);

                std::this_thread::sleep_until(sendTime);

                auto rtp = makeRtpPacket(pack.data() + offset,
                                         chunk,
                                         seq++,
                                         rtpTs,
                                         ssrc,
                                         marker);

                auto n = sendto(fd,
                                rtp.data(),
                                rtp.size(),
                                0,
                                reinterpret_cast<const sockaddr*>(&dst),
                                sizeof(dst));
                if (n < 0) {
                    std::cerr << "[FAIL] RTP sendto: " << std::strerror(errno) << "\n";
                    close(fd);
                    return false;
                }

                offset += chunk;
                ++packetIndex;
            }

            ++frameIndex;

            const auto nextFrameTime = startTime + frameInterval * frameIndex;
            std::this_thread::sleep_until(nextFrameTime);
        }
    }

    close(fd);
    std::cout << "[RTP-PS] stream finished\n";
    return true;
}

bool registerToPlatform(int fd,
                        const tri::protocol::Gb28181Config& cfg,
                        const sockaddr_in& server,
                        int& cseq,
                        std::string& callId) {
    callId = makeCallId(cfg.local.deviceId);

    auto firstRegister = buildRegisterNoAuth(cfg, cseq, makeBranch(), callId);
    if (!sendPacket(fd, server, firstRegister, "REGISTER no-auth")) {
        return false;
    }

    std::string resp;
    sockaddr_in from{};
    if (!recvPacket(fd, resp, &from, 5000)) {
        std::cerr << "[FAIL] no REGISTER response\n";
        return false;
    }

    if (containsStatus(resp, 200)) {
        std::cout << "[PASS] REGISTER success without auth\n";
        return true;
    }

    if (!containsStatus(resp, 401)) {
        std::cerr << "[FAIL] unexpected REGISTER response: " << statusLine(resp) << "\n";
        return false;
    }

    std::string realm = getHeaderParam(resp, "realm");
    std::string nonce = getHeaderParam(resp, "nonce");
    std::string qop = getHeaderParam(resp, "qop");

    if (realm.empty()) realm = cfg.server.domain;
    if (nonce.empty()) {
        std::cerr << "[FAIL] 401 response has no nonce\n";
        return false;
    }

    if (qop.find("auth") != std::string::npos) {
        qop = "auth";
    } else {
        qop.clear();
    }

    ++cseq;
    auto secondRegister = buildRegisterAuth(cfg, cseq, makeBranch(), callId, realm, nonce, qop);
    if (!sendPacket(fd, server, secondRegister, "REGISTER auth")) {
        return false;
    }

    std::string resp2;
    if (!recvPacket(fd, resp2, &from, 5000)) {
        std::cerr << "[FAIL] no auth REGISTER response\n";
        return false;
    }

    if (!containsStatus(resp2, 200)) {
        std::cerr << "[FAIL] REGISTER auth failed: " << statusLine(resp2) << "\n";
        return false;
    }

    std::cout << "[PASS] REGISTER success\n";
    return true;
}

} // namespace

int main(int argc, char** argv) {
    const std::string configDir = argc >= 2 ? argv[1] : "./configs";
    const std::string h264Path = argc >= 3 ? argv[2] : "./gb_play_test_yuyv_400k.h264";
    const int fps = argc >= 4 ? std::stoi(argv[3]) : 30;
    const int loopCount = argc >= 5 ? std::stoi(argv[4]) : 100;

    const std::string gbPath = configDir + "/gb28181.yaml";

    auto cfgRet = tri::protocol::ProtocolConfigLoader::loadGb28181(gbPath);
    if (!cfgRet) {
        std::cerr << "[FAIL] load gb28181 config: "
                  << cfgRet.status().describe() << "\n";
        return 1;
    }

    const auto cfg = cfgRet.value();

    std::cout << "[CONFIG]\n";
    std::cout << "  local_device_id=" << cfg.local.deviceId << "\n";
    std::cout << "  local_domain=" << cfg.local.domain << "\n";
    std::cout << "  local_ip=" << cfg.local.ip << "\n";
    std::cout << "  local_port=" << cfg.local.port << "\n";
    std::cout << "  channel_id=" << cfg.channel.id << "\n";
    std::cout << "  channel_name=" << cfg.channel.name << "\n";
    std::cout << "  server_id=" << cfg.server.id << "\n";
    std::cout << "  server_domain=" << cfg.server.domain << "\n";
    std::cout << "  server_ip=" << cfg.server.ip << "\n";
    std::cout << "  server_port=" << cfg.server.port << "\n";
    std::cout << "  h264_path=" << h264Path << "\n";

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        std::cerr << "[FAIL] SIP socket: " << std::strerror(errno) << "\n";
        return 2;
    }

    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(cfg.local.port);

    if (cfg.local.ip.empty() || cfg.local.ip == "0.0.0.0") {
        local.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (inet_pton(AF_INET, cfg.local.ip.c_str(), &local.sin_addr) != 1) {
            std::cerr << "[FAIL] invalid local ip: " << cfg.local.ip << "\n";
            close(fd);
            return 3;
        }
    }

    if (bind(fd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0) {
        std::cerr << "[FAIL] bind "
                  << cfg.local.ip << ":" << cfg.local.port
                  << ": " << std::strerror(errno) << "\n";
        close(fd);
        return 4;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(cfg.server.port);
    if (inet_pton(AF_INET, cfg.server.ip.c_str(), &server.sin_addr) != 1) {
        std::cerr << "[FAIL] invalid server ip: " << cfg.server.ip << "\n";
        close(fd);
        return 5;
    }

    int cseq = 1;
    std::string registerCallId;

    if (!registerToPlatform(fd, cfg, server, cseq, registerCallId)) {
        close(fd);
        return 6;
    }

    std::cout << "[WAIT] Device is online. Trigger Catalog/Sync Channel on platform if needed.\n";
    std::cout << "[WAIT] Then click play on platform. Waiting for INVITE ...\n";

    auto lastKeepalive = std::chrono::steady_clock::now();
    int sn = 1;

    while (true) {
        std::string msg;
        sockaddr_in from{};

        if (!recvPacket(fd, msg, &from, 1000)) {
            auto now = std::chrono::steady_clock::now();
            if (now - lastKeepalive > std::chrono::seconds(20)) {
                ++cseq;
                auto keepalive = buildKeepaliveMessage(cfg, cseq, makeBranch(), registerCallId, sn++);
                sendPacket(fd, server, keepalive, "KEEPALIVE");
                lastKeepalive = now;
            }
            continue;
        }

        if (msg.rfind("MESSAGE ", 0) == 0) {
            answerSimpleMessage(fd, server, msg, cfg);
            continue;
        }

        if (msg.rfind("SIP/2.0 ", 0) == 0) {
            std::cout << "[INFO] received SIP response, ignoring: " << statusLine(msg) << "\n";
            continue;
        }

        InviteInfo inv;
        if (!parseInvite(msg, inv)) {
            std::cout << "[INFO] ignoring non-INVITE SIP message\n";
            continue;
        }

        if (inv.tcpMode) {
            std::cerr << "[FAIL] platform INVITE uses TCP RTP. This probe only supports UDP.\n";
            std::cerr << "[ACTION] Change platform stream mode to UDP, then retry.\n";
            close(fd);
            return 7;
        }

        const int localRtpPort = cfg.media.rtpLocalPort > 0 ? cfg.media.rtpLocalPort : 30000;
        auto ok = buildSip200ForInvite(cfg, inv, localRtpPort);
        if (!sendPacket(fd, server, ok, "200 OK for INVITE")) {
            close(fd);
            return 8;
        }

        std::cout << "[WAIT] Waiting ACK before RTP ...\n";
        bool ackReceived = false;
        for (int i = 0; i < 5; ++i) {
            std::string ack;
            sockaddr_in ackFrom{};
            if (recvPacket(fd, ack, &ackFrom, 2000)) {
                if (ack.rfind("ACK ", 0) == 0) {
                    std::cout << "[OK] ACK received\n";
                    ackReceived = true;
                    break;
                }
                if (ack.rfind("MESSAGE ", 0) == 0) {
                    answerSimpleMessage(fd, server, ack, cfg);
                }
            }
        }

        if (!ackReceived) {
            std::cout << "[WARN] ACK not received in timeout, try sending RTP anyway\n";
        }

std::string rtpDstIp = inv.mediaIp;

if (rtpDstIp == "127.0.0.1" || rtpDstIp == "0.0.0.0" || rtpDstIp.empty()) {
    std::cout << "[WARN] INVITE SDP media IP is invalid for remote RTP: "
              << rtpDstIp
              << ", override to server.ip=" << cfg.server.ip
              << "\n";
    rtpDstIp = cfg.server.ip;
}

std::cout << "[PLAY] Start PS/RTP to platform: "
          << rtpDstIp << ":" << inv.mediaPort
          << " original_sdp_ip=" << inv.mediaIp
          << "\n";

const std::string ssrc = inv.ssrc.empty() ? cfg.media.ssrc : inv.ssrc;
const bool okStream = streamPsFileAsRtp(h264Path,
                                        rtpDstIp,
                                        inv.mediaPort,
                                        ssrc,
                                        fps,
                                        loopCount);
        close(fd);
        return okStream ? 0 : 9;
    }

    close(fd);
    return 0;
}
