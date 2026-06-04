#pragma once

#include "device/CameraNodeBinding.h"
#include "hardware/v4l2/V4L2Types.h"

#include <string>
#include <vector>

namespace tri::device {

struct CameraCapability {
    std::string name;
    CameraNodeBinding nodes;
    std::string driver;
    std::string card;
    std::string busInfo;
    std::vector<hardware::v4l2::V4L2FormatDesc> formats;
    hardware::v4l2::CaptureFormat configuredFormat{};
    bool hasVideoNode{false};
    bool hasMetadataNode{false};
};

} // namespace tri::device
