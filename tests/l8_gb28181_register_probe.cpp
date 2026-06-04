#include "protocol/ProtocolConfigLoader.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/md5.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <thread>

namespace {

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

std::string nowSipDate() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);

    char buf[128]{};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    return std::string(buf);
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

std::string getStatusLine(const std::string& text) {
    auto pos = text.find("\r\n");
    if (pos == std::string::npos) return text;
    return text.substr(0, pos);
}

bool containsStatus(const std::string& text, int code) {
    return text.find("SIP/2.0 " + std::to_string(code)) != std::string::npos;
}

std::string makeBranch() {
    return "z9hG4bK-trifusion-" + std::to_string(std::time(nullptr));
}

std::string makeCallId(const std::string& deviceId) {
    return deviceId + "-" + std::to_string(std::time(nullptr)) + "@tri-fusion-camera";
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
    ss << "User-Agent: TriFusionCamera-L8-GB28181-Probe\r\n";
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
    ss << "User-Agent: TriFusionCamera-L8-GB28181-Probe\r\n";
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
    ss << "User-Agent: TriFusionCamera-L8-GB28181-Probe\r\n";
    ss << "Content-Type: Application/MANSCDP+xml\r\n";
    ss << "Content-Length: " << bodyText.size() << "\r\n\r\n";
    ss << bodyText;
    return ss.str();
}

bool sendPacket(int fd,
                const sockaddr_in& server,
                const std::string& packet,
                const std::string& title) {
    auto n = sendto(fd,
                    packet.data(),
                    packet.size(),
                    0,
                    reinterpret_cast<const sockaddr*>(&server),
                    sizeof(server));
    if (n < 0) {
        std::cerr << "[FAIL] send " << title << ": " << std::strerror(errno) << "\n";
        return false;
    }

    std::cout << "\n========== SEND " << title << " ==========\n";
    std::cout << packet << "\n";
    return true;
}

bool recvPacket(int fd, std::string& out, int timeoutMs) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int ret = select(fd + 1, &rfds, nullptr, nullptr, &tv);
    if (ret <= 0) return false;

    char buf[8192]{};
    sockaddr_in from{};
    socklen_t fromLen = sizeof(from);
    ssize_t n = recvfrom(fd, buf, sizeof(buf) - 1, 0,
                         reinterpret_cast<sockaddr*>(&from), &fromLen);
    if (n <= 0) return false;

    out.assign(buf, static_cast<std::size_t>(n));

    std::cout << "\n========== RECV ==========\n";
    std::cout << out << "\n";
    return true;
}

} // namespace

int main(int argc, char** argv) {
    const std::string configDir = argc >= 2 ? argv[1] : "./configs";
    const int keepaliveSeconds = argc >= 3 ? std::stoi(argv[2]) : 30;

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
    std::cout << "  server_id=" << cfg.server.id << "\n";
    std::cout << "  server_domain=" << cfg.server.domain << "\n";
    std::cout << "  server_ip=" << cfg.server.ip << "\n";
    std::cout << "  server_port=" << cfg.server.port << "\n";

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        std::cerr << "[FAIL] socket: " << std::strerror(errno) << "\n";
        return 2;
    }

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(cfg.local.port);
    if (inet_pton(AF_INET, cfg.local.ip.c_str(), &local.sin_addr) != 1) {
        std::cerr << "[FAIL] invalid local ip: " << cfg.local.ip << "\n";
        close(fd);
        return 3;
    }

    if (bind(fd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0) {
        std::cerr << "[FAIL] bind " << cfg.local.ip << ":" << cfg.local.port
                  << ": " << std::strerror(errno) << "\n";
        std::cerr << "[HINT] 如果提示 Address already in use，说明本机 5060 被占用，"
                  << "可以先把 gb28181.yaml 里的 local.port 改成 5062。\n";
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

    const std::string callId = makeCallId(cfg.local.deviceId);

    int cseq = 1;
    auto firstRegister = buildRegisterNoAuth(cfg, cseq, makeBranch(), callId);
    if (!sendPacket(fd, server, firstRegister, "REGISTER no-auth")) {
        close(fd);
        return 6;
    }

    std::string resp401;
    if (!recvPacket(fd, resp401, 5000)) {
        std::cerr << "[FAIL] no response from GB28181 server. Check network/firewall/platform IP/port.\n";
        close(fd);
        return 7;
    }

    if (!containsStatus(resp401, 401)) {
        if (containsStatus(resp401, 200)) {
            std::cout << "[PASS] REGISTER accepted without auth.\n";
        } else {
            std::cerr << "[FAIL] unexpected REGISTER response: "
                      << getStatusLine(resp401) << "\n";
            close(fd);
            return 8;
        }
    } else {
        std::string realm = getHeaderParam(resp401, "realm");
        std::string nonce = getHeaderParam(resp401, "nonce");
        std::string qop = getHeaderParam(resp401, "qop");

        if (realm.empty()) realm = cfg.server.domain;
        if (nonce.empty()) {
            std::cerr << "[FAIL] 401 response has no nonce\n";
            close(fd);
            return 9;
        }

        if (qop.find("auth") != std::string::npos) {
            qop = "auth";
        } else {
            qop.clear();
        }

        ++cseq;
        auto authRegister = buildRegisterAuth(cfg, cseq, makeBranch(), callId, realm, nonce, qop);
        if (!sendPacket(fd, server, authRegister, "REGISTER auth")) {
            close(fd);
            return 10;
        }

        std::string resp200;
        if (!recvPacket(fd, resp200, 5000)) {
            std::cerr << "[FAIL] no response after auth REGISTER\n";
            close(fd);
            return 11;
        }

        if (!containsStatus(resp200, 200)) {
            std::cerr << "[FAIL] auth REGISTER rejected: " << getStatusLine(resp200) << "\n";
            std::cerr << "[CHECK] 重点检查 gb28181.yaml 的 local.device_id、server.id、server.domain、server.password。\n";
            close(fd);
            return 12;
        }

        std::cout << "[PASS] REGISTER success. Device should be online on platform.\n";
    }

    std::cout << "[KEEPALIVE] sending keepalive MESSAGE for "
              << keepaliveSeconds << " seconds\n";

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(keepaliveSeconds);

    int sn = 1;
    while (std::chrono::steady_clock::now() < deadline) {
        ++cseq;
        auto msg = buildKeepaliveMessage(cfg, cseq, makeBranch(), callId, sn++);
        if (!sendPacket(fd, server, msg, "KEEPALIVE MESSAGE")) {
            close(fd);
            return 13;
        }

        std::string response;
        if (recvPacket(fd, response, 3000)) {
            if (containsStatus(response, 200)) {
                std::cout << "[OK] keepalive accepted\n";
            } else {
                std::cout << "[WARN] keepalive response: " << getStatusLine(response) << "\n";
            }
        } else {
            std::cout << "[WARN] keepalive no response\n";
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    close(fd);

    std::cout << "[DONE] GB28181 register probe finished.\n";
    return 0;
}
