#pragma once
#include <string>
#include <unordered_map>
namespace tri::protocol::onvif {
struct OnvifRequest {
    std::string action;
    std::unordered_map<std::string, std::string> fields;
};
struct OnvifStreamUri {
    std::string uri;
    std::string transport{"RTSP"};
};
} // namespace tri::protocol::onvif
