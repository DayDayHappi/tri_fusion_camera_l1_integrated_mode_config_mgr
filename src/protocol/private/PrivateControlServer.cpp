#include "protocol/private/PrivateControlServer.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cctype>
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

std::string normalize(std::string s) {
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) {
        return std::isspace(c) || c == '_' || c == '-' || c == '.';
    }), s.end());
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string lowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

bool splitTwo(const std::string& path, std::string* a, std::string* b) {
    const auto slash = path.find('/');
    if (slash == std::string::npos || slash == 0 || slash + 1 >= path.size()) return false;
    *a = path.substr(0, slash);
    *b = path.substr(slash + 1);
    return b->find('/') == std::string::npos;
}

bool parseFusionColor(const std::string& text, std::uint16_t* value, std::string* canonical) {
    const std::string s = normalize(text);
    if (s == "1" || s == "blackwhite" || s == "bw" || s == "mono" || s == "monochrome") {
        *value = 1; *canonical = "black_white"; return true;
    }
    if (s == "2" || s == "forest") {
        *value = 2; *canonical = "forest"; return true;
    }
    if (s == "3" || s == "snow") {
        *value = 3; *canonical = "snow"; return true;
    }
    if (s == "4" || s == "ocean" || s == "sea") {
        *value = 4; *canonical = "ocean"; return true;
    }
    if (s == "5" || s == "city" || s == "urban") {
        *value = 5; *canonical = "city"; return true;
    }
    if (s == "6" || s == "desert") {
        *value = 6; *canonical = "desert"; return true;
    }if (s == "7" || s == "default" || s == "normal") {
    *value = 7; *canonical = "default"; return true;
}
    
    return false;
}

bool parseContourMode(const std::string& text, std::uint16_t* value, std::string* canonical) {
    const std::string s = normalize(text);
    if (s == "0" || s == "off" || s == "close" || s == "none" || s == "disable" || s == "disabled") {
        *value = 0; *canonical = "off"; return true;
    }
    if (s == "1" || s == "red") {
        *value = 1; *canonical = "red"; return true;
    }
    if (s == "2" || s == "green") {
        *value = 2; *canonical = "green"; return true;
    }
    if (s == "3" || s == "blue") {
        *value = 3; *canonical = "blue"; return true;
    }
    if (s == "4" || s == "purple" || s == "violet") {
        *value = 4; *canonical = "purple"; return true;
    }
    return false;
}

bool parseInfraredPolarity(const std::string& text, std::uint16_t* value, std::string* canonical) {
    const std::string s = normalize(text);
    if (s == "0" || s == "whitehot" || s == "white") {
        *value = 0; *canonical = "white_hot"; return true;
    }
    if (s == "1" || s == "blackhot" || s == "black") {
        *value = 1; *canonical = "black_hot"; return true;
    }
    return false;
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
    if (name == "visible" || name == "1" || name == "VISIBLE_ONLY") { *mode = PrivateWorkMode::VisibleOnly; return true; }
    if (name == "lowlight" || name == "2" || name == "LOWLIGHT_ONLY") { *mode = PrivateWorkMode::LowlightOnly; return true; }
    if (name == "thermal" || name == "3" || name == "THERMAL_ONLY") { *mode = PrivateWorkMode::ThermalOnly; return true; }
    if (name == "lowlight_thermal" || name == "4" || name == "LOWLIGHT_THERMAL_COMPOSITE") { *mode = PrivateWorkMode::LowlightThermalComposite; return true; }
    if (name == "visible_lowlight" || name == "5" || name == "VISIBLE_LOWLIGHT_FUSION") { *mode = PrivateWorkMode::VisibleLowlightFusion; return true; }
    if (name == "visible_thermal" || name == "6" || name == "VISIBLE_THERMAL_FUSION") { *mode = PrivateWorkMode::VisibleThermalFusion; return true; }
    if (name == "visible_composite" || name == "7" || name == "VISIBLE_COMPOSITE_FUSION") { *mode = PrivateWorkMode::VisibleCompositeFusion; return true; }
    return false;
}

std::string toString(PrivateCompositeControlKind kind) {
    switch (kind) {
        case PrivateCompositeControlKind::FusionColor: return "fusion_color";
        case PrivateCompositeControlKind::ContourMode: return "contour_mode";
        case PrivateCompositeControlKind::InfraredPolarity: return "infrared_polarity";
        case PrivateCompositeControlKind::Unknown: return "unknown";
    }
    return "unknown";
}

PrivateControlServer::PrivateControlServer() = default;
PrivateControlServer::~PrivateControlServer() { stop(); }

bool PrivateControlServer::start(const PrivateControlServerConfig& config,
                                 ModeSwitchCallback modeCallback,
                                 CompositeControlCallback compositeCallback,
                                 CompositeConfigQueryCallback configQueryCallback) {
    if (running_.load()) {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = "private control server already running";
        return false;
    }
    if (!modeCallback) {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = "private control server mode callback is empty";
        return false;
    }

    config_ = config;
    modeCallback_ = std::move(modeCallback);
    compositeCallback_ = std::move(compositeCallback);
    configQueryCallback_ = std::move(configQueryCallback);

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
    std::cout << "[PrivateControlServer] listening on " << config_.bindAddress << ":" << config_.port << std::endl;
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

    if (path == "/api/v1/status") return handleStatusRequest();

    constexpr const char* modePrefix = "/api/v1/mode/";
    if (path.rfind(modePrefix, 0) == 0) return handleModeRequest(method, path.substr(std::strlen(modePrefix)));

    constexpr const char* compositeConfigPath = "/api/v1/composite/config";
    if (path == compositeConfigPath || path == "/api/v1/composite/configuration") {
        return handleCompositeConfigRequest(method);
    }

    constexpr const char* compositePrefix = "/api/v1/composite/";
    if (path.rfind(compositePrefix, 0) == 0) return handleCompositeRequest(method, path.substr(std::strlen(compositePrefix)));

    return httpJson(404, "Not Found",
        "{\"ok\":false,\"error\":\"unknown api\",\"usage\":\"POST /api/v1/mode/{mode}, POST /api/v1/composite/{fusion_color|contour|infrared_polarity}/{value}, or GET /api/v1/composite/config\"}");
}

std::string PrivateControlServer::handleModeRequest(const std::string& method, const std::string& modeName) {
    if (method != "POST" && method != "GET") {
        return httpJson(405, "Method Not Allowed", "{\"ok\":false,\"result\":\"failed\",\"error\":\"method must be POST or GET\"}");
    }

    PrivateWorkMode targetMode{};
    if (!privateWorkModeFromCommandName(modeName, &targetMode)) {
        return httpJson(400, "Bad Request",
            "{\"ok\":false,\"error\":\"unsupported mode\",\"supported\":[\"visible\",\"lowlight\",\"thermal\",\"lowlight_thermal\",\"visible_lowlight\",\"visible_thermal\",\"visible_composite\"]}");
    }

    PrivateControlResult result;
    if (modeCallback_) result = modeCallback_(targetMode);
    else { result.ok = false; result.message = "mode switch callback is not installed"; }
    if (result.ok) currentMode_ = targetMode;

    std::ostringstream body;
    body << "{"
         << "\"ok\":" << (result.ok ? "true" : "false") << ","
         << "\"result\":\"" << (result.ok ? "succeeded" : "failed") << "\","
         << "\"mode\":\"" << jsonEscape(toString(targetMode)) << "\","
         << "\"command\":\"" << jsonEscape(toCommandName(targetMode)) << "\","
         << "\"message\":\"" << jsonEscape(result.message) << "\""
         << "}";
    return httpJson(result.ok ? 200 : 500, result.ok ? "OK" : "Internal Server Error", body.str());
}

std::string PrivateControlServer::handleCompositeRequest(const std::string& method, const std::string& subPath) {
    if (method != "POST" && method != "GET") {
        return httpJson(405, "Method Not Allowed", "{\"ok\":false,\"result\":\"failed\",\"error\":\"method must be POST or GET\"}");
    }

    PrivateCompositeControlRequest req;
    if (!parseCompositeControlPath(subPath, &req)) {
        return httpJson(400, "Bad Request",
            "{\"ok\":false,\"error\":\"unsupported composite control\",\"supported\":{\"fusion_color\":[\"black_white\",\"forest\",\"snow\",\"ocean\",\"city\",\"desert\",\"default\"],\"contour\":[\"off\",\"red\",\"green\",\"blue\",\"purple\"],\"infrared_polarity\":[\"white_hot\",\"black_hot\"]}}");
    }

    PrivateControlResult result;
    if (compositeCallback_) result = compositeCallback_(req);
    else { result.ok = false; result.message = "composite control callback is not installed"; }

    if (result.ok) {
        if (req.kind == PrivateCompositeControlKind::FusionColor) currentFusionColor_ = req.valueName;
        if (req.kind == PrivateCompositeControlKind::ContourMode) currentContourMode_ = req.valueName;
        if (req.kind == PrivateCompositeControlKind::InfraredPolarity) currentInfraredPolarity_ = req.valueName;
    }

    std::ostringstream body;
    body << "{"
         << "\"ok\":" << (result.ok ? "true" : "false") << ","
         << "\"result\":\"" << (result.ok ? "succeeded" : "failed") << "\","
         << "\"control\":\"" << jsonEscape(req.name) << "\","
         << "\"value_name\":\"" << jsonEscape(req.valueName) << "\","
         << "\"value\":" << req.value << ","
         << "\"message\":\"" << jsonEscape(result.message) << "\""
         << "}";
    return httpJson(result.ok ? 200 : 500, result.ok ? "OK" : "Internal Server Error", body.str());
}

std::string PrivateControlServer::handleCompositeConfigRequest(const std::string& method) {
    if (method != "GET" && method != "POST") {
        return httpJson(405, "Method Not Allowed", "{\"ok\":false,\"result\":\"failed\",\"error\":\"method must be GET or POST\"}");
    }

    PrivateCompositeConfigSnapshot snapshot;
    if (configQueryCallback_) {
        snapshot = configQueryCallback_();
    } else {
        snapshot.ok = false;
        snapshot.message = "composite config query callback is not installed";
    }

    std::ostringstream body;
    body << "{"
         << "\"ok\":" << (snapshot.ok ? "true" : "false") << ","
         << "\"result\":\"" << (snapshot.ok ? "succeeded" : "failed") << "\","
         << "\"message\":\"" << jsonEscape(snapshot.message) << "\","
         << "\"registers\":[";

    for (std::size_t i = 0; i < snapshot.registers.size(); ++i) {
        const auto& item = snapshot.registers[i];
        if (i > 0) body << ",";
        body << "{"
             << "\"index\":" << item.index << ","
             << "\"name\":\"" << jsonEscape(item.name) << "\","
             << "\"display_name\":\"" << jsonEscape(item.displayName) << "\","
             << "\"address\":\"0x" << std::hex << std::uppercase << item.address << std::nouppercase << std::dec << "\","
             << "\"raw_value\":" << item.rawValue << ","
             << "\"value\":" << item.value << ","
             << "\"value_name\":\"" << jsonEscape(item.valueName) << "\""
             << "}";
    }

    body << "]}";
    return httpJson(snapshot.ok ? 200 : 500, snapshot.ok ? "OK" : "Internal Server Error", body.str());
}

std::string PrivateControlServer::handleStatusRequest() {
    std::ostringstream body;
    body << "{"
         << "\"ok\":true,"
         << "\"server\":\"private_control\","
         << "\"control_port\":" << config_.port << ","
         << "\"current_mode\":\"" << jsonEscape(toString(currentMode_)) << "\","
         << "\"current_composite\":{"
         << "\"fusion_color\":\"" << jsonEscape(currentFusionColor_) << "\","
         << "\"contour\":\"" << jsonEscape(currentContourMode_) << "\","
         << "\"infrared_polarity\":\"" << jsonEscape(currentInfraredPolarity_) << "\""
         << "},"
         << "\"commands\":{"
         << "\"1\":\"/api/v1/mode/visible\","
         << "\"2\":\"/api/v1/mode/lowlight\","
         << "\"3\":\"/api/v1/mode/thermal\","
         << "\"4\":\"/api/v1/mode/lowlight_thermal\","
         << "\"5\":\"/api/v1/mode/visible_lowlight\","
         << "\"6\":\"/api/v1/mode/visible_thermal\","
         << "\"7\":\"/api/v1/mode/visible_composite\""
         << "},"
         << "\"composite_controls\":{"
         << "\"fusion_color\":\"/api/v1/composite/fusion_color/{black_white|forest|snow|ocean|city|desert|default}\","
         << "\"contour\":\"/api/v1/composite/contour/{off|red|green|blue|purple}\","
         << "\"infrared_polarity\":\"/api/v1/composite/infrared_polarity/{white_hot|black_hot}\","
         << "\"query_config\":\"/api/v1/composite/config\""
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

bool PrivateControlServer::parseCompositeControlPath(const std::string& subPath,
                                                     PrivateCompositeControlRequest* out) {
    if (out == nullptr) return false;
    std::string name;
    std::string valueText;
    if (!splitTwo(subPath, &name, &valueText)) return false;

    std::uint16_t value = 0;
    std::string canonical;
    const std::string key = normalize(name);

    if (key == "fusioncolor" || key == "color") {
        if (!parseFusionColor(valueText, &value, &canonical)) return false;
        out->kind = PrivateCompositeControlKind::FusionColor;
        out->name = "fusion_color";
    } else if (key == "contour" || key == "contourmode" || key == "outline" || key == "outlinemode") {
        if (!parseContourMode(valueText, &value, &canonical)) return false;
        out->kind = PrivateCompositeControlKind::ContourMode;
        out->name = "contour";
    } else if (key == "infraredpolarity" || key == "irpolarity" || key == "polarity") {
        if (!parseInfraredPolarity(valueText, &value, &canonical)) return false;
        out->kind = PrivateCompositeControlKind::InfraredPolarity;
        out->name = "infrared_polarity";
    } else {
        return false;
    }

    out->value = value;
    out->valueName = canonical;
    return true;
}

std::string PrivateControlServer::httpJson(int statusCode, const std::string& statusText, const std::string& jsonBody) {
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
