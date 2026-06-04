#pragma once

#include "device/CameraManager.h"
#include "device/CameraStatus.h"
#include "device_control/CompositeSensorController.h"
#include "device_control/CompositeSensorState.h"
#include "media/stream/MainStream.h"
#include "media/stream/StreamTypes.h"
#include "mode/ModeManager.h"
#include "mode/ModeTypes.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace tri::service {

struct ServiceRuntime {
    tri::mode::ModeManager* modeManager{nullptr};
    tri::device::CameraManager* cameraManager{nullptr};
    tri::device_control::CompositeSensorController* compositeController{nullptr};
    tri::media::MainStream* mainStream{nullptr};
};

struct SystemStatus {
    tri::mode::ModeState mode;
    std::vector<tri::device::CameraStatus> cameras;
    tri::device_control::CompositeSensorState compositeSensor;
    tri::media::StreamState mainStreamState{tri::media::StreamState::Stopped};
    tri::media::StreamStats mainStreamStats{};
    std::unordered_map<std::string, std::string> protocol;
};

std::string toString(tri::media::StreamState state);
std::unordered_map<std::string, std::string> flattenStatus(const SystemStatus& status);

} // namespace tri::service
