#include "service/ServiceTypes.h"

#include "device/CameraTypes.h"
#include "device_control/CompositeSensorControlTypes.h"
#include "mode/WorkMode.h"

namespace tri::service {

std::string toString(tri::media::StreamState state) {
    switch (state) {
        case tri::media::StreamState::Stopped: return "STOPPED";
        case tri::media::StreamState::Running: return "RUNNING";
        case tri::media::StreamState::Error: return "ERROR";
    }
    return "UNKNOWN";
}

std::unordered_map<std::string, std::string> flattenStatus(const SystemStatus& status) {
    std::unordered_map<std::string, std::string> out;
    out["mode.current"] = tri::mode::toString(status.mode.currentMode);
    out["mode.previous"] = tri::mode::toString(status.mode.previousMode);
    out["mode.stage"] = tri::mode::toString(status.mode.stage);
    out["mode.switching"] = status.mode.switching ? "true" : "false";
    out["mode.switch_count"] = std::to_string(status.mode.switchCount);
    out["mode.error_count"] = std::to_string(status.mode.errorCount);
    out["mode.last_error"] = status.mode.lastError;

    out["composite_sensor.initialized"] = status.compositeSensor.initialized ? "true" : "false";
    out["composite_sensor.serial_online"] = status.compositeSensor.serialOnline ? "true" : "false";
    out["composite_sensor.current_mode"] = tri::device_control::toString(status.compositeSensor.currentMode);
    out["composite_sensor.error_count"] = std::to_string(status.compositeSensor.errorCount);
    out["composite_sensor.last_error"] = status.compositeSensor.lastError;

    out["stream.main.state"] = toString(status.mainStreamState);
    out["stream.main.frames_in"] = std::to_string(status.mainStreamStats.framesIn);
    out["stream.main.frames_dropped"] = std::to_string(status.mainStreamStats.framesDropped);
    out["stream.main.last_pts_ms"] = std::to_string(status.mainStreamStats.lastPtsMs);

    out["camera.count"] = std::to_string(status.cameras.size());
    for (std::size_t i = 0; i < status.cameras.size(); ++i) {
        const auto prefix = "camera." + std::to_string(i) + ".";
        const auto& c = status.cameras[i];
        out[prefix + "id"] = tri::device::toString(c.id);
        out[prefix + "kind"] = tri::device::toString(c.kind);
        out[prefix + "state"] = tri::device::toString(c.state);
        out[prefix + "enabled"] = c.enabled ? "true" : "false";
        out[prefix + "opened"] = c.opened ? "true" : "false";
        out[prefix + "streaming"] = c.streaming ? "true" : "false";
        out[prefix + "online"] = c.online ? "true" : "false";
        out[prefix + "frames_read"] = std::to_string(c.framesRead);
        out[prefix + "dropped_frames"] = std::to_string(c.droppedFrames);
        out[prefix + "last_error"] = c.lastErrorMessage;
    }

    for (const auto& kv : status.protocol) {
        out["protocol." + kv.first] = kv.second;
    }
    return out;
}

} // namespace tri::service
