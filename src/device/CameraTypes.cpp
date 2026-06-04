#include "device/CameraTypes.h"

namespace tri::device {

std::string toString(CameraId id) {
    switch (id) {
        case CameraId::Visible: return "visible";
        case CameraId::CompositeLowThermal: return "composite_low_thermal";
    }
    return "unknown";
}

std::string toString(CameraKind kind) {
    switch (kind) {
        case CameraKind::VisibleUvc: return "visible_uvc";
        case CameraKind::CompositeLowThermalUvc: return "composite_low_thermal_uvc";
    }
    return "unknown";
}

std::string toString(CameraLifecycleState state) {
    switch (state) {
        case CameraLifecycleState::Uninitialized: return "uninitialized";
        case CameraLifecycleState::Initialized: return "initialized";
        case CameraLifecycleState::Opened: return "opened";
        case CameraLifecycleState::Streaming: return "streaming";
        case CameraLifecycleState::Error: return "error";
    }
    return "unknown";
}

} // namespace tri::device
