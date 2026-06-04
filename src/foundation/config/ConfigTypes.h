#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace tri::foundation {

struct CameraEndpointConfig {
    bool enable{false};
    std::string name;
    std::string videoNode;
    std::string metadataNode;
    int width{0};
    int height{0};
    int fps{0};
    std::string format;
    std::string metadataFormat;
};

struct CameraConfig {
    CameraEndpointConfig visible;
    CameraEndpointConfig compositeLowThermal;
};

struct SerialConfig {
    bool enable{false};
    std::string dev;
    int baudrate{115200};
    int databits{8};
    int stopbits{1};
    std::string parity{"none"};
    int timeoutMs{500};
    int retryCount{3};
    std::unordered_map<std::string, std::string> commands;
};

struct ModeConfig {
    std::string defaultMode;
    std::unordered_map<std::string, bool> enabledModes;
    std::unordered_map<std::string, std::string> compositeOutputs;
};

struct MediaConfig {
    std::string codec{"h264"};
    std::string encoder{"mpp"};
    int width{1920};
    int height{1080};
    int fps{25};
    int bitrate{4096};
    int gop{25};
    bool lowLatency{true};
    bool dropFrame{true};
    bool rtspEnable{true};
    int rtspPort{8554};
    std::string rtspPath{"/live/main"};
};

struct ProtocolConfig {
    std::string active{"gb28181"};
    bool allowMultiOnline{false};
    std::vector<std::string> priority;
    bool reserveMultiOnlineArchitecture{true};
};

struct SystemConfig {
    CameraConfig camera;
    SerialConfig serial;
    ModeConfig mode;
    MediaConfig media;
    ProtocolConfig protocol;
};

} // namespace tri::foundation
