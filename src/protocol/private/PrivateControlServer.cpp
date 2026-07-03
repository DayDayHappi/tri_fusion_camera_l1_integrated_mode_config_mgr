#include "protocol/private/PrivateControlServer.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <exception>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace tri::protocol::private_api {

namespace {

constexpr int kRecvBufferSize = 4096;

void closeFd(int* fd) {
    if (fd != nullptr && *fd >= 0) {
        ::close(*fd);
        *fd = -1;
    }
}

bool methodAllowedForControl(const std::string& method) {
    return method == "POST" || method == "GET";
}

std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::size_t begin = 0;
    while (begin <= path.size()) {
        const std::size_t end = path.find('/', begin);
        const std::string part = path.substr(begin,
            end == std::string::npos ? std::string::npos : end - begin);
        if (!part.empty()) parts.push_back(part);
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return parts;
}

bool parseStrictInt(const std::string& text, int* value) {
    if (value == nullptr || text.empty()) return false;
    try {
        std::size_t consumed = 0;
        const int parsed = std::stoi(text, &consumed, 10);
        if (consumed != text.size()) return false;
        *value = parsed;
        return true;
    } catch (...) {
        return false;
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
    return start(config, std::move(callback), CompositeControlCallbacks{});
}

bool PrivateControlServer::start(const PrivateControlServerConfig& config,
                                 ModeSwitchCallback callback,
                                 const CompositeControlCallbacks& compositeCallbacks) {
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
    compositeCallbacks_ = compositeCallbacks;

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

    constexpr const char* modePrefix = "/api/v1/mode/";
    if (path.rfind(modePrefix, 0) == 0) {
        return handleModeRequest(method, path.substr(std::strlen(modePrefix)));
    }

    constexpr const char* compositePrefix = "/api/v1/composite/";
    if (path.rfind(compositePrefix, 0) == 0) {
        return handleCompositeRequest(method, path.substr(std::strlen(compositePrefix)));
    }

    constexpr const char* fusionPrefix = "/api/v1/fusion/";
    if (path.rfind(fusionPrefix, 0) == 0) {
        return handleFusionRequest(method, path.substr(std::strlen(fusionPrefix)));
    }

    return httpJson(404, "Not Found",
        "{\"ok\":false,\"error\":\"unknown api\",\"usage\":\"/api/v1/mode/{mode}, /api/v1/composite/fusion_color/{value}, /api/v1/composite/contour/{value}, /api/v1/composite/infrared_polarity/{value}, /api/v1/composite/query_config, /api/v1/composite/read_all_registers, /api/v1/composite/registration, /api/v1/composite/registration/{infrared|lowlight}/{x|y}/{value}, /api/v1/composite/registration/{infrared|lowlight}/zoom/{value}, /api/v1/composite/registration/{infrared|lowlight}/move/{left|right|up|down}/{step}, /api/v1/fusion/composite_position, /api/v1/fusion/composite_position/{x}/{y}, /api/v1/fusion/composite_position/move/{left|right|up|down}/{step}, /api/v1/fusion/composite_shrink, /api/v1/fusion/composite_shrink/{horizontal}/{vertical}, /api/v1/fusion/composite_adjustment/save\"}");
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

std::string PrivateControlServer::handleCompositeRequest(const std::string& method,
                                                         const std::string& path) {
    if (!methodAllowedForControl(method)) {
        return httpJson(405, "Method Not Allowed", "{\"ok\":false,\"error\":\"method must be POST or GET\"}");
    }

    const std::string queryConfig = "query_config";
    const std::string readAllRegisters = "read_all_registers";
    const std::string legacyConfig = "config";
    if (path == queryConfig || path == legacyConfig) {
        return invokeCompositeCallback(compositeCallbacks_.queryConfig,
                                       "query_config callback is not installed");
    }
    if (path == readAllRegisters) {
        return invokeCompositeCallback(compositeCallbacks_.readAllRegisters,
                                       "read_all_registers callback is not installed");
    }

    if (path == "registration") {
        return invokeCompositeCallback(compositeCallbacks_.queryRegistration,
                                       "query_registration callback is not installed");
    }

    constexpr const char* registrationPrefix = "registration/";
    if (path.rfind(registrationPrefix, 0) == 0) {
        const auto parts = splitPath(path.substr(std::strlen(registrationPrefix)));

        // registration/{infrared|lowlight}/{x|y}/{signed_value}
        if (parts.size() == 3 && (parts[1] == "x" || parts[1] == "y")) {
            int value = 0;
            if (!parseStrictInt(parts[2], &value)) {
                return httpJson(400, "Bad Request",
                    "{\"ok\":false,\"action\":\"set_registration_offset\",\"error\":\"invalid integer value\"}");
            }
            if (value < -32768 || value > 32767) {
                return httpJson(400, "Bad Request",
                    "{\"ok\":false,\"action\":\"set_registration_offset\",\"error\":\"value must be in int16 range -32768..32767\"}");
            }
            if (!compositeCallbacks_.setRegistrationOffset) {
                return httpJson(200, "OK",
                    "{\"ok\":false,\"error\":\"set_registration_offset callback is not installed\"}");
            }
            try {
                const auto ret = compositeCallbacks_.setRegistrationOffset(parts[0], parts[1], value);
                return httpJson(ret.statusCode, ret.statusText,
                    ret.bodyJson.empty() ? "{\"ok\":false,\"error\":\"empty callback response\"}" : ret.bodyJson);
            } catch (const std::exception& e) {
                return httpJson(200, "OK",
                    std::string("{\"ok\":false,\"error\":\"registration callback exception: ") +
                    jsonEscape(e.what()) + "\"}");
            }
        }

        // registration/{infrared|lowlight}/zoom/{value}
        if (parts.size() == 3 && parts[1] == "zoom") {
            int value = 0;
            if (!parseStrictInt(parts[2], &value)) {
                return httpJson(400, "Bad Request",
                    "{\"ok\":false,\"action\":\"set_registration_zoom\",\"error\":\"invalid integer value\"}");
            }
            if (!compositeCallbacks_.setRegistrationZoom) {
                return httpJson(200, "OK",
                    "{\"ok\":false,\"error\":\"set_registration_zoom callback is not installed\"}");
            }
            try {
                const auto ret = compositeCallbacks_.setRegistrationZoom(parts[0], value);
                return httpJson(ret.statusCode, ret.statusText,
                    ret.bodyJson.empty() ? "{\"ok\":false,\"error\":\"empty callback response\"}" : ret.bodyJson);
            } catch (const std::exception& e) {
                return httpJson(200, "OK",
                    std::string("{\"ok\":false,\"error\":\"registration zoom callback exception: ") +
                    jsonEscape(e.what()) + "\"}");
            }
        }

        // registration/{infrared|lowlight}/move/{left|right|up|down}/{positive_step}
        if (parts.size() == 4 && parts[1] == "move") {
            int step = 0;
            if (!parseStrictInt(parts[3], &step) || step <= 0 || step > 32767) {
                return httpJson(400, "Bad Request",
                    "{\"ok\":false,\"action\":\"move_registration_offset\",\"error\":\"step must be an integer in range 1..32767\"}");
            }
            if (!compositeCallbacks_.moveRegistrationOffset) {
                return httpJson(200, "OK",
                    "{\"ok\":false,\"error\":\"move_registration_offset callback is not installed\"}");
            }
            try {
                const auto ret = compositeCallbacks_.moveRegistrationOffset(parts[0], parts[2], step);
                return httpJson(ret.statusCode, ret.statusText,
                    ret.bodyJson.empty() ? "{\"ok\":false,\"error\":\"empty callback response\"}" : ret.bodyJson);
            } catch (const std::exception& e) {
                return httpJson(200, "OK",
                    std::string("{\"ok\":false,\"error\":\"registration callback exception: ") +
                    jsonEscape(e.what()) + "\"}");
            }
        }

        return httpJson(400, "Bad Request",
            "{\"ok\":false,\"error\":\"invalid registration path\",\"usage\":\"registration/{infrared|lowlight}/{x|y}/{value}, registration/{infrared|lowlight}/zoom/{value}, or registration/{infrared|lowlight}/move/{left|right|up|down}/{step}\"}");
    }

    constexpr const char* fusionPrefix = "fusion_color/";
    if (path.rfind(fusionPrefix, 0) == 0) {
        return invokeCompositeValueCallback(compositeCallbacks_.setFusionColor,
                                            path.substr(std::strlen(fusionPrefix)),
                                            "fusion_color callback is not installed");
    }

    constexpr const char* contourPrefix = "contour/";
    if (path.rfind(contourPrefix, 0) == 0) {
        return invokeCompositeValueCallback(compositeCallbacks_.setContour,
                                            path.substr(std::strlen(contourPrefix)),
                                            "contour callback is not installed");
    }

    constexpr const char* polarityPrefix = "infrared_polarity/";
    if (path.rfind(polarityPrefix, 0) == 0) {
        return invokeCompositeValueCallback(compositeCallbacks_.setInfraredPolarity,
                                            path.substr(std::strlen(polarityPrefix)),
                                            "infrared_polarity callback is not installed");
    }

    return httpJson(404, "Not Found",
        "{\"ok\":false,\"error\":\"unknown composite api\",\"usage\":\"/api/v1/composite/fusion_color/{black_white|forest|snow|ocean|city|desert|default|7}, /api/v1/composite/contour/{off|red|green|blue|purple}, /api/v1/composite/infrared_polarity/{white_hot|black_hot}, /api/v1/composite/query_config, /api/v1/composite/read_all_registers, /api/v1/composite/registration, /api/v1/composite/registration/{infrared|lowlight}/{x|y}/{value}, /api/v1/composite/registration/{infrared|lowlight}/zoom/{value}, /api/v1/composite/registration/{infrared|lowlight}/move/{left|right|up|down}/{step}, /api/v1/fusion/composite_position, /api/v1/fusion/composite_position/{x}/{y}, /api/v1/fusion/composite_position/move/{left|right|up|down}/{step}, /api/v1/fusion/composite_shrink, /api/v1/fusion/composite_shrink/{horizontal}/{vertical}, /api/v1/fusion/composite_adjustment/save\"}");
}

std::string PrivateControlServer::handleFusionRequest(const std::string& method,
                                                      const std::string& path) {
    if (!methodAllowedForControl(method)) {
        return httpJson(405, "Method Not Allowed", "{\"ok\":false,\"error\":\"method must be POST or GET\"}");
    }

    if (path == "composite_position" || path == "composite_offset") {
        return invokeCompositeCallback(compositeCallbacks_.queryCompositePosition,
                                       "query_composite_position callback is not installed");
    }

    if (path == "composite_shrink" || path == "composite_resize" || path == "composite_border") {
        return invokeCompositeCallback(compositeCallbacks_.queryCompositeShrink,
                                       "query_composite_shrink callback is not installed");
    }

    if (path == "composite_adjustment/save" ||
        path == "composite_save" ||
        path == "save_composite_adjustment") {
        return invokeCompositeCallback(compositeCallbacks_.saveCompositeAdjustment,
                                       "save_composite_adjustment callback is not installed");
    }

    // Old visible adjustment APIs are intentionally not mapped to the new behavior.
    // Visible is now the reference frame; use composite_position/composite_shrink instead.
    if (path == "visible_position" || path == "visible_offset" ||
        path == "visible_shrink" || path == "visible_resize" || path == "visible_border" ||
        path == "visible_adjustment/save" || path == "visible_save" ||
        path == "save_visible_adjustment") {
        return httpJson(200, "OK",
            "{\"ok\":false,\"error\":\"visible adjustment is disabled because visible is now the reference frame; use composite_position/composite_shrink/composite_adjustment/save\"}");
    }

    constexpr const char* compositePositionPrefix = "composite_position/";
    constexpr const char* compositeOffsetPrefix = "composite_offset/";
    constexpr const char* compositeShrinkPrefix = "composite_shrink/";
    constexpr const char* compositeResizePrefix = "composite_resize/";
    constexpr const char* compositeBorderPrefix = "composite_border/";

    // Also catch old visible-prefix control paths with a clear message.
    constexpr const char* visiblePositionPrefix = "visible_position/";
    constexpr const char* visibleOffsetPrefix = "visible_offset/";
    constexpr const char* visibleShrinkPrefix = "visible_shrink/";
    constexpr const char* visibleResizePrefix = "visible_resize/";
    constexpr const char* visibleBorderPrefix = "visible_border/";

    if (path.rfind(visiblePositionPrefix, 0) == 0 ||
        path.rfind(visibleOffsetPrefix, 0) == 0 ||
        path.rfind(visibleShrinkPrefix, 0) == 0 ||
        path.rfind(visibleResizePrefix, 0) == 0 ||
        path.rfind(visibleBorderPrefix, 0) == 0) {
        return httpJson(200, "OK",
            "{\"ok\":false,\"error\":\"visible adjustment is disabled because visible is now the reference frame; use composite_position/composite_shrink\"}");
    }

    std::string rest;
    bool isPositionPath = false;
    bool isShrinkPath = false;

    if (path.rfind(compositePositionPrefix, 0) == 0) {
        rest = path.substr(std::strlen(compositePositionPrefix));
        isPositionPath = true;
    } else if (path.rfind(compositeOffsetPrefix, 0) == 0) {
        rest = path.substr(std::strlen(compositeOffsetPrefix));
        isPositionPath = true;
    } else if (path.rfind(compositeShrinkPrefix, 0) == 0) {
        rest = path.substr(std::strlen(compositeShrinkPrefix));
        isShrinkPath = true;
    } else if (path.rfind(compositeResizePrefix, 0) == 0) {
        rest = path.substr(std::strlen(compositeResizePrefix));
        isShrinkPath = true;
    } else if (path.rfind(compositeBorderPrefix, 0) == 0) {
        rest = path.substr(std::strlen(compositeBorderPrefix));
        isShrinkPath = true;
    } else {
        return httpJson(404, "Not Found",
            "{\"ok\":false,\"error\":\"unknown fusion api\",\"usage\":\"/api/v1/fusion/composite_position, /api/v1/fusion/composite_position/{x}/{y}, /api/v1/fusion/composite_position/move/{left|right|up|down}/{step}, /api/v1/fusion/composite_shrink, /api/v1/fusion/composite_shrink/{horizontal}/{vertical}, /api/v1/fusion/composite_adjustment/save\"}");
    }

    const auto parts = splitPath(rest);

    if (isShrinkPath) {
        // composite_shrink/{horizontal_pixels}/{vertical_pixels}
        // horizontal_pixels is total width compression in visible/output coordinates.
        // vertical_pixels is total height compression in visible/output coordinates.
        if (parts.size() == 2) {
            int horizontal = 0;
            int vertical = 0;
            if (!parseStrictInt(parts[0], &horizontal) || !parseStrictInt(parts[1], &vertical)) {
                return httpJson(400, "Bad Request",
                    "{\"ok\":false,\"action\":\"set_composite_shrink\",\"error\":\"horizontal and vertical must be integers\"}");
            }
            if (!compositeCallbacks_.setCompositeShrink) {
                return httpJson(200, "OK",
                    "{\"ok\":false,\"error\":\"set_composite_shrink callback is not installed\"}");
            }
            try {
                const auto ret = compositeCallbacks_.setCompositeShrink(horizontal, vertical);
                return httpJson(ret.statusCode, ret.statusText,
                    ret.bodyJson.empty() ? "{\"ok\":false,\"error\":\"empty callback response\"}" : ret.bodyJson);
            } catch (const std::exception& e) {
                return httpJson(200, "OK",
                    std::string("{\"ok\":false,\"error\":\"composite shrink callback exception: ") +
                    jsonEscape(e.what()) + "\"}");
            }
        }

        return httpJson(400, "Bad Request",
            "{\"ok\":false,\"error\":\"invalid composite shrink path\",\"usage\":\"composite_shrink/{horizontal_pixels}/{vertical_pixels}\"}");
    }

    if (isPositionPath) {
        // composite_position/{x}/{y}
        if (parts.size() == 2) {
            int x = 0;
            int y = 0;
            if (!parseStrictInt(parts[0], &x) || !parseStrictInt(parts[1], &y)) {
                return httpJson(400, "Bad Request",
                    "{\"ok\":false,\"action\":\"set_composite_position\",\"error\":\"x and y must be integers\"}");
            }
            if (!compositeCallbacks_.setCompositePosition) {
                return httpJson(200, "OK",
                    "{\"ok\":false,\"error\":\"set_composite_position callback is not installed\"}");
            }
            try {
                const auto ret = compositeCallbacks_.setCompositePosition(x, y);
                return httpJson(ret.statusCode, ret.statusText,
                    ret.bodyJson.empty() ? "{\"ok\":false,\"error\":\"empty callback response\"}" : ret.bodyJson);
            } catch (const std::exception& e) {
                return httpJson(200, "OK",
                    std::string("{\"ok\":false,\"error\":\"composite position callback exception: ") +
                    jsonEscape(e.what()) + "\"}");
            }
        }

        // composite_position/move/{left|right|up|down}/{step}
        if (parts.size() == 3 && parts[0] == "move") {
            int step = 0;
            if (!parseStrictInt(parts[2], &step) || step <= 0 || step > 4096) {
                return httpJson(400, "Bad Request",
                    "{\"ok\":false,\"action\":\"move_composite_position\",\"error\":\"step must be an integer in range 1..4096\"}");
            }
            if (!compositeCallbacks_.moveCompositePosition) {
                return httpJson(200, "OK",
                    "{\"ok\":false,\"error\":\"move_composite_position callback is not installed\"}");
            }
            try {
                const auto ret = compositeCallbacks_.moveCompositePosition(parts[1], step);
                return httpJson(ret.statusCode, ret.statusText,
                    ret.bodyJson.empty() ? "{\"ok\":false,\"error\":\"empty callback response\"}" : ret.bodyJson);
            } catch (const std::exception& e) {
                return httpJson(200, "OK",
                    std::string("{\"ok\":false,\"error\":\"composite position callback exception: ") +
                    jsonEscape(e.what()) + "\"}");
            }
        }

        return httpJson(400, "Bad Request",
            "{\"ok\":false,\"error\":\"invalid composite position path\",\"usage\":\"composite_position/{x}/{y} or composite_position/move/{left|right|up|down}/{step}\"}");
    }

    return httpJson(404, "Not Found",
        "{\"ok\":false,\"error\":\"unknown fusion api\"}");
}

std::string PrivateControlServer::handleStatusRequest() {
    std::ostringstream body;
    body << "{"
         << "\"ok\":true,"
         << "\"server\":\"private_control\","
         << "\"control_port\":" << config_.port << ","
         << "\"current_mode\":\"" << jsonEscape(toCommandName(currentMode_)) << "\","
         << "\"current_mode_verbose\":\"" << jsonEscape(toString(currentMode_)) << "\"";

    if (compositeCallbacks_.currentCompositeStatusJson) {
        const std::string status = compositeCallbacks_.currentCompositeStatusJson();
        if (!status.empty()) {
            body << ",\"current_composite\":" << status;
        }
    }

    body << ",\"commands\":{"
         << "\"1\":\"/api/v1/mode/visible\","
         << "\"2\":\"/api/v1/mode/lowlight\","
         << "\"3\":\"/api/v1/mode/thermal\","
         << "\"4\":\"/api/v1/mode/lowlight_thermal\","
         << "\"5\":\"/api/v1/mode/visible_lowlight\","
         << "\"6\":\"/api/v1/mode/visible_thermal\","
         << "\"7\":\"/api/v1/mode/visible_composite\""
         << "},"
         << "\"composite_api\":["
         << "\"/api/v1/composite/fusion_color/{black_white|forest|snow|ocean|city|desert|default|7}\","
         << "\"/api/v1/composite/contour/{off|red|green|blue|purple}\","
         << "\"/api/v1/composite/infrared_polarity/{white_hot|black_hot}\","
         << "\"/api/v1/composite/query_config\","
         << "\"/api/v1/composite/read_all_registers\","
         << "\"/api/v1/composite/registration\","
         << "\"/api/v1/composite/registration/{infrared|lowlight}/{x|y}/{value}\","
         << "\"/api/v1/composite/registration/{infrared|lowlight}/zoom/{value}\","
         << "\"/api/v1/composite/registration/{infrared|lowlight}/move/{left|right|up|down}/{step}\""
         << "],"
         << "\"fusion_api\":["
         << "\"/api/v1/fusion/composite_position\","
         << "\"/api/v1/fusion/composite_position/{x}/{y}\","
         << "\"/api/v1/fusion/composite_position/move/{left|right|up|down}/{step}\","
         << "\"/api/v1/fusion/composite_shrink\","
         << "\"/api/v1/fusion/composite_shrink/{horizontal}/{vertical}\","
         << "\"/api/v1/fusion/composite_adjustment/save\""
         << "]"
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

std::string PrivateControlServer::invokeCompositeCallback(
    const std::function<PrivateHttpResult()>& callback,
    const std::string& missingMessage) {
    if (!callback) {
        std::ostringstream body;
        body << "{\"ok\":false,\"error\":\"" << jsonEscape(missingMessage) << "\"}";
        return httpJson(200, "OK", body.str());
    }
    try {
        const auto ret = callback();
        return httpJson(ret.statusCode, ret.statusText, ret.bodyJson.empty() ? "{\"ok\":false,\"error\":\"empty callback response\"}" : ret.bodyJson);
    } catch (const std::exception& e) {
        std::ostringstream body;
        body << "{\"ok\":false,\"error\":\"composite callback exception: " << jsonEscape(e.what()) << "\"}";
        return httpJson(200, "OK", body.str());
    } catch (...) {
        return httpJson(200, "OK", "{\"ok\":false,\"error\":\"composite callback unknown exception\"}");
    }
}

std::string PrivateControlServer::invokeCompositeValueCallback(
    const std::function<PrivateHttpResult(const std::string&)>& callback,
    const std::string& value,
    const std::string& missingMessage) {
    if (!callback) {
        std::ostringstream body;
        body << "{\"ok\":false,\"error\":\"" << jsonEscape(missingMessage) << "\"}";
        return httpJson(200, "OK", body.str());
    }
    try {
        const auto ret = callback(value);
        return httpJson(ret.statusCode, ret.statusText, ret.bodyJson.empty() ? "{\"ok\":false,\"error\":\"empty callback response\"}" : ret.bodyJson);
    } catch (const std::exception& e) {
        std::ostringstream body;
        body << "{\"ok\":false,\"error\":\"composite callback exception: " << jsonEscape(e.what()) << "\"}";
        return httpJson(200, "OK", body.str());
    } catch (...) {
        return httpJson(200, "OK", "{\"ok\":false,\"error\":\"composite callback unknown exception\"}");
    }
}

} // namespace tri::protocol::private_api
