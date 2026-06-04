#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include "foundation/config/ConfigTypes.h"
#include "foundation/error/Result.h"

namespace tri::foundation {

class ConfigManager final {
public:
    static ConfigManager& instance();

    Result<void> loadAll(const std::string& configDir);
    const SystemConfig& system() const;
    const CameraConfig& camera() const;
    const SerialConfig& serial() const;
    const ModeConfig& mode() const;
    const MediaConfig& media() const;
    const ProtocolConfig& protocol() const;

private:
    ConfigManager() = default;
    Result<void> loadCamera(const std::string& path);
    Result<void> loadSerial(const std::string& path);
    Result<void> loadMode(const std::string& path);
    Result<void> loadMedia(const std::string& path);
    Result<void> loadProtocol(const std::string& path);

    mutable std::mutex mutex_;
    SystemConfig config_{};
    bool loaded_{false};
};

using FlatConfig = std::unordered_map<std::string, std::string>;
Result<FlatConfig> parseSimpleYaml(const std::string& path);

} // namespace tri::foundation
