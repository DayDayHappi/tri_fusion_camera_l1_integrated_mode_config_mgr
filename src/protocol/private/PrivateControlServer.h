#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace tri::protocol::private_api {

enum class PrivateWorkMode : std::uint8_t {
    VisibleOnly = 1,
    LowlightOnly = 2,
    ThermalOnly = 3,
    LowlightThermalComposite = 4,
    VisibleLowlightFusion = 5,
    VisibleThermalFusion = 6,
    VisibleCompositeFusion = 7,
};

std::string toString(PrivateWorkMode mode);
std::string toCommandName(PrivateWorkMode mode);
bool privateWorkModeFromCommandName(const std::string& name, PrivateWorkMode* mode);

struct PrivateControlServerConfig {
    std::string bindAddress{"0.0.0.0"};
    std::uint16_t port{18080};
    std::int32_t backlog{8};
};

struct PrivateControlResult {
    bool ok{false};
    std::string message;
    std::string currentMode;
};

using ModeSwitchCallback = std::function<PrivateControlResult(PrivateWorkMode)>;

class PrivateControlServer {
public:
    PrivateControlServer();
    ~PrivateControlServer();

    PrivateControlServer(const PrivateControlServer&) = delete;
    PrivateControlServer& operator=(const PrivateControlServer&) = delete;

    bool start(const PrivateControlServerConfig& config, ModeSwitchCallback callback);
    void stop();
    bool isRunning() const;

    const std::string& lastError() const;

private:
    void runLoop();
    void handleClient(int clientFd);

    std::string handleHttpRequest(const std::string& request);
    std::string handleModeRequest(const std::string& method, const std::string& modeName);
    std::string handleStatusRequest();

    static std::string parseMethod(const std::string& request);
    static std::string parsePath(const std::string& request);
    static std::string httpJson(int statusCode, const std::string& statusText, const std::string& jsonBody);
    static std::string jsonEscape(const std::string& text);

private:
    PrivateControlServerConfig config_{};
    ModeSwitchCallback callback_{};

    std::atomic_bool running_{false};
    std::thread serverThread_;

    int listenFd_{-1};
    mutable std::mutex mutex_;
    mutable std::string lastError_;
    PrivateWorkMode currentMode_{PrivateWorkMode::LowlightThermalComposite};
};

} // namespace tri::protocol::private_api
