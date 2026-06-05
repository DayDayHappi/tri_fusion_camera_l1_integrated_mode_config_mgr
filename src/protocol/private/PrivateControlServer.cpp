#include "protocol/private/PrivateControlServer.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

namespace tri::protocol::private_api {

namespace {

constexpr int kRecvBufferSize = 4096;

void closeFd(int* fd) {
    if (fd != nullptr && *fd >= 0) {
        ::close(*fd);
        *fd = -1;
    }
}

} // namespace

std::string toString(PrivateWorkMode mode) {
    switch (mode) {
        case PrivateWorkMode::VisibleOnly: return "VISIBLE_ONLY";
        case PrivateWorkMode::LowlightOnly: return "LOWLIGHT_ONLY";
        case PrivateWorkMode::ThermalOnly: return "THERMAL_ONLY";
        case PrivateWorkMode::LowlightThermalComposite: return "LOWLIGHT_THERMAL_COMPOSITE";
        case PrivateWorkMode::VisibleLowlightFusion: return "VISIBLE_LOWLIGHT_FUSION";
        case PrivateWorkMode::VisibleThermalFusion: return "VISIBLE_THERMAL_FUSION";
        case PrivateWorkMode::VisibleCompositeFusion: return "VISIBLE_COMPOSITE_FUSION";
    }
    return "UNKNOWN";
}

std::string toCommandName(PrivateWorkMode mode) {
    switch (mode) {
        case PrivateWorkMode::VisibleOnly: return "visible";
        case PrivateWorkMode::LowlightOnly: return "lowlight";
        case PrivateWorkMode::ThermalOnly: return "thermal";
        case PrivateWorkMode::LowlightThermalComposite: return "lowlight_thermal";
        case PrivateWorkMode::VisibleLowlightFusion: return "visible_lowlight";
        case PrivateWorkMode::VisibleThermalFusion: return "visible_thermal";
        case PrivateWorkMode::VisibleCompositeFusion: return "visible_composite";
    }
    return "unknown";
}

bool privateWorkModeFromCommandName(const std::string& name, PrivateWorkMode* mode) {
    if (mode == nullptr) return false;
    if (name == "visible" || name == "1" || name == "VISIBLE_ONLY") {
        *mode = PrivateWorkMode::VisibleOnly; return true;
    }
    if (name == "lowlight" || name == "2" || name == "LOWLIGHT_ONLY") {
        *mode = PrivateWorkMode::LowlightOnly; return true;
    }
    if (name == "thermal" || name == "3" || name == "THERMAL_ONLY") {
        *mode = PrivateWorkMode::ThermalOnly; return true;
    }
    if (name == "lowlight_thermal" || name == "4" || name == "LOWLIGHT_THERMAL_COMPOSITE") {
        *mode = PrivateWorkMode::LowlightThermalComposite; return true;
    }
    if (name == "visible_lowlight" || name == "5" || name == "VISIBLE_LOWLIGHT_FUSION") {
        *mode = PrivateWorkMode::VisibleLowlightFusion; return true;
    }
    if (name == "visible_thermal" || name == "6" || name == "VISIBLE_THERMAL_FUSION") {
        *mode = PrivateWorkMode::VisibleThermalFusion; return true;
    }
    if (name == "visible_composite" || name == "7" || name == "VISIBLE_COMPOSITE_FUSION") {
        *mode = PrivateWorkMode::VisibleCompositeFusion; return true;
    }
    return false;
}

PrivateControlServer::PrivateControlServer() = default;
PrivateControlServer::~PrivateControlServer() { stop(); }

bool PrivateControlServer::start(const PrivateControlServerConfig& config, ModeSwitchCallback callback) {
    if (running_.load()) {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = "private control server already running";
        return false;
    }
    if (!callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = "private control server callback is empty";
        return false;
    }

    config_ = config;
    callback_ = std::move(callback);

    listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = std::string("socket failed: ") + std::strerror(errno);
        return false;
    }

    int yes = 1;
    ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);
    if (config_.bindAddress.empty() || config_.bindAddress == "0.0.0.0") {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (::inet_pton(AF_INET, config_.bindAddress.c_str(), &addr.sin_addr) != 1) {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = "invalid bind address: " + config_.bindAddress;
        closeFd(&listenFd_);
        return false;
    }

    if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = std::string("bind failed: ") + std::strerror(errno);
        closeFd(&listenFd_);
        return false;
    }

    if (::listen(listenFd_, config_.backlog) != 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = std::string("listen failed: ") + std::strerror(errno);
        closeFd(&listenFd_);
        return false;
    }

    running_.store(true);
    serverThread_ = std::thread(&PrivateControlServer::runLoop, this);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_.clear();
    }
    return true;
}

void PrivateControlServer::stop() {
    if (!running_.exchange(false)) return;
    closeFd(&listenFd_);
    if (serverThread_.joinable()) serverThread_.join();
}

bool PrivateControlServer::isRunning() const { return running_.load(); }

const std::string& PrivateControlServer::lastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

void PrivateControlServer::runLoop() {
    std::cout << "[PrivateControlServer] listening on "
              << config_.bindAddress << ":" << config_.port << std::endl;

    while (running_.load()) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        const int clientFd = ::accept(listenFd_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientFd < 0) {
            if (running_.load()) {
                std::lock_guard<std::mutex> lock(mutex_);
                lastError_ = std::string("accept failed: ") + std::strerror(errno);
            }
            continue;
        }
        handleClient(clientFd);
        ::close(clientFd);
    }
}

void PrivateControlServer::handleClient(int clientFd) {
    char buffer[kRecvBufferSize] = {0};
    const ssize_t n = ::recv(clientFd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) return;

    std::string request(buffer, static_cast<std::size_t>(n));
    std::string response = handleHttpRequest(request);

    const char* data = response.c_str();
    std::size_t left = response.size();
    while (left > 0) {
        const ssize_t sent = ::send(clientFd, data, left, 0);
        if (sent <= 0) return;
        data += sent;
        left -= static_cast<std::size_t>(sent);
    }
}

std::string PrivateControlServer::handleHttpRequest(const std::string& request) {
    const std::string method = parseMethod(request);
    const std::string path = parsePath(request);

    if (path == "/api/v1/status") {
        return handleStatusRequest();
    }

    constexpr const char* prefix = "/api/v1/mode/";
    if (path.rfind(prefix, 0) == 0) {
        return handleModeRequest(method, path.substr(std::strlen(prefix)));
    }

    return httpJson(404, "Not Found",
        "{\"ok\":false,\"error\":\"unknown api\",\"usage\":\"POST /api/v1/mode/{visible|lowlight|thermal|lowlight_thermal|visible_lowlight|visible_thermal|visible_composite}\"}");
}

std::string PrivateControlServer::handleModeRequest(const std::string& method,
                                                    const std::string& modeName) {
    if (method != "POST" && method != "GET") {
        return httpJson(405, "Method Not Allowed", "{\"ok\":false,\"error\":\"method must be POST or GET\"}");
    }

    PrivateWorkMode targetMode{};
    if (!privateWorkModeFromCommandName(modeName, &targetMode)) {
        return httpJson(400, "Bad Request",
            "{\"ok\":false,\"error\":\"unsupported mode\",\"supported\":[\"visible\",\"lowlight\",\"thermal\",\"lowlight_thermal\",\"visible_lowlight\",\"visible_thermal\",\"visible_composite\"]}");
    }

    PrivateControlResult result;
    if (callback_) {
        result = callback_(targetMode);
    } else {
        result.ok = false;
        result.message = "mode switch callback is not installed";
    }
    if (result.ok) currentMode_ = targetMode;

    std::ostringstream body;
    body << "{"
         << "\"ok\":" << (result.ok ? "true" : "false") << ","
         << "\"mode\":\"" << jsonEscape(toString(targetMode)) << "\","
         << "\"command\":\"" << jsonEscape(toCommandName(targetMode)) << "\","
         << "\"message\":\"" << jsonEscape(result.message) << "\""
         << "}";

    return httpJson(result.ok ? 200 : 500, result.ok ? "OK" : "Internal Server Error", body.str());
}

std::string PrivateControlServer::handleStatusRequest() {
    std::ostringstream body;
    body << "{"
         << "\"ok\":true,"
         << "\"server\":\"private_control\","
         << "\"control_port\":" << config_.port << ","
         << "\"current_mode\":\"" << jsonEscape(toString(currentMode_)) << "\","
         << "\"commands\":{"
         << "\"1\":\"/api/v1/mode/visible\","
         << "\"2\":\"/api/v1/mode/lowlight\","
         << "\"3\":\"/api/v1/mode/thermal\","
         << "\"4\":\"/api/v1/mode/lowlight_thermal\","
         << "\"5\":\"/api/v1/mode/visible_lowlight\","
         << "\"6\":\"/api/v1/mode/visible_thermal\","
         << "\"7\":\"/api/v1/mode/visible_composite\""
         << "}"
         << "}";
    return httpJson(200, "OK", body.str());
}

std::string PrivateControlServer::parseMethod(const std::string& request) {
    const auto space = request.find(' ');
    return space == std::string::npos ? std::string{} : request.substr(0, space);
}

std::string PrivateControlServer::parsePath(const std::string& request) {
    const auto first = request.find(' ');
    if (first == std::string::npos) return {};
    const auto second = request.find(' ', first + 1);
    if (second == std::string::npos) return {};
    return request.substr(first + 1, second - first - 1);
}

std::string PrivateControlServer::httpJson(int statusCode,
                                           const std::string& statusText,
                                           const std::string& jsonBody) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << jsonBody.size() << "\r\n"
        << "Connection: close\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "\r\n"
        << jsonBody;
    return oss.str();
}

std::string PrivateControlServer::jsonEscape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

} // namespace tri::protocol::private_api
