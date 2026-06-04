#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <sys/time.h>

namespace tri::device {

enum class CameraId {
    Visible,
    CompositeLowThermal,
};

enum class CameraKind {
    VisibleUvc,
    CompositeLowThermalUvc,
};

enum class CameraLifecycleState {
    Uninitialized,
    Initialized,
    Opened,
    Streaming,
    Error,
};

struct CameraFrame {
    CameraId cameraId{CameraId::Visible};
    std::uint64_t sequence{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t pixelformat{0};
    std::vector<std::uint8_t> bytes;
    timeval v4l2Timestamp{};
    std::int64_t receiveTimestampMs{0};
    std::vector<std::uint8_t> metadata;
    std::int64_t metadataTimestampMs{0};
};

std::string toString(CameraId id);
std::string toString(CameraKind kind);
std::string toString(CameraLifecycleState state);

} // namespace tri::device
