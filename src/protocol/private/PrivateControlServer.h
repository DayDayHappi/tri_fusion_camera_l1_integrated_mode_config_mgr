#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

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

enum class PrivateCompositeControlKind : std::uint8_t {
    Unknown = 0,
    FusionColor,
    ContourMode,
    InfraredPolarity,
};

std::string toString(PrivateWorkMode mode);
std::string toCommandName(PrivateWorkMode mode);
bool privateWorkModeFromCommandName(const std::string& name, PrivateWorkMode* mode);
std::string toString(PrivateCompositeControlKind kind);

struct PrivateCompositeControlRequest {
    PrivateCompositeControlKind kind{PrivateCompositeControlKind::Unknown};
    std::string name;
    std::string valueName;
    std::uint16_t value{0};
};

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

struct PrivateCompositeRegisterValue {
    int index{0};
    std::string name;
    std::string displayName;
    std::uint16_t address{0};
    std::uint16_t rawValue{0};
    int value{0};
    std::string valueName;
};

struct PrivateCompositeConfigSnapshot {
    bool ok{false};
    std::string message;
    std::vector<PrivateCompositeRegisterValue> registers;
};

using ModeSwitchCallback = std::function<PrivateControlResult(PrivateWorkMode)>;
using CompositeControlCallback = std::function<PrivateControlResult(const PrivateCompositeControlRequest&)>;
using CompositeConfigQueryCallback = std::function<PrivateCompositeConfigSnapshot()>;

class PrivateControlServer {
public:
    PrivateControlServer();
    ~PrivateControlServer();

    PrivateControlServer(const PrivateControlServer&) = delete;
    PrivateControlServer& operator=(const PrivateControlServer&) = delete;

    bool start(const PrivateControlServerConfig& config,
               ModeSwitchCallback modeCallback,
               CompositeControlCallback compositeCallback = nullptr,
               CompositeConfigQueryCallback configQueryCallback = nullptr);
    void stop();
    bool isRunning() const;

    const std::string& lastError() const;

private:
    void runLoop();
    void handleClient(int clientFd);

    std::string handleHttpRequest(const std::string& request);
    std::string handleModeRequest(const std::string& method, const std::string& modeName);
    std::string handleCompositeRequest(const std::string& method, const std::string& subPath);
    std::string handleCompositeConfigRequest(const std::string& method);
    std::string handleStatusRequest();

    static std::string parseMethod(const std::string& request);
    static std::string parsePath(const std::string& request);
    static bool parseCompositeControlPath(const std::string& subPath, PrivateCompositeControlRequest* out);
    static std::string httpJson(int statusCode, const std::string& statusText, const std::string& jsonBody);
    static std::string jsonEscape(const std::string& text);

private:
    PrivateControlServerConfig config_{};
    ModeSwitchCallback modeCallback_{};
    CompositeControlCallback compositeCallback_{};
    CompositeConfigQueryCallback configQueryCallback_{};

    std::atomic_bool running_{false};
    std::thread serverThread_;

    int listenFd_{-1};
    mutable std::mutex mutex_;
    mutable std::string lastError_;
    PrivateWorkMode currentMode_{PrivateWorkMode::LowlightThermalComposite};
    std::string currentFusionColor_{"forest"};
    std::string currentContourMode_{"off"};
    std::string currentInfraredPolarity_{"white_hot"};
};

} // namespace tri::protocol::private_api
