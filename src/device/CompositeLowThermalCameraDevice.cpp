#include "device/CompositeLowThermalCameraDevice.h"
#include "foundation/log/Logger.h"
#include "foundation/time/Clock.h"
#include "hardware/common/HardwareConfig.h"
#include "hardware/uvc/UvcDevice.h"

#include <cstring>

namespace tri::device {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

CompositeLowThermalCameraDevice::~CompositeLowThermalCameraDevice() { close(); }

Result<void> CompositeLowThermalCameraDevice::rememberError(ErrorCode code, const std::string& message) {
    status_.state = CameraLifecycleState::Error;
    status_.lastErrorCode = code;
    status_.lastErrorMessage = message;
    TRI_LOG_ERROR(tri::foundation::LogCategory::Uvc) << "composite camera error: " << message;
    return Result<void>::error(code, message);
}

void CompositeLowThermalCameraDevice::clearError() {
    status_.lastErrorCode = ErrorCode::Ok;
    status_.lastErrorMessage.clear();
}

Result<void> CompositeLowThermalCameraDevice::init(const tri::foundation::CameraEndpointConfig& config) {
    config_ = config;
    status_ = CameraStatus{};
    status_.id = CameraId::CompositeLowThermal;
    status_.kind = CameraKind::CompositeLowThermalUvc;
    status_.enabled = config.enable;
    capability_ = CameraCapability{};
    capability_.name = config.name;
    capability_.nodes.videoNode = config.videoNode;
    capability_.nodes.metadataNode = config.metadataNode;

    if (!config.enable) {
        status_.configured = true;
        status_.state = CameraLifecycleState::Initialized;
        return Result<void>::success();
    }

    auto fmt = tri::hardware::makeCaptureFormat(config);
    if (!fmt) return rememberError(fmt.status().code(), fmt.status().describe());
    captureFormat_ = fmt.value();
    capability_.configuredFormat = captureFormat_;
    status_.configured = true;
    status_.state = CameraLifecycleState::Initialized;
    clearError();
    return Result<void>::success();
}

void CompositeLowThermalCameraDevice::tryOpenMetadataNode() {
    status_.metadataAvailable = false;
    capability_.hasMetadataNode = false;
    if (config_.metadataNode.empty()) return;

    auto metaInfo = tri::hardware::uvc::UvcDevice::inspectNode(config_.metadataNode);
    if (!metaInfo) {
        TRI_LOG_WARN(tri::foundation::LogCategory::Uvc) << "metadata node inspect failed, ignored: " << metaInfo.status().describe();
        return;
    }
    if (!tri::hardware::uvc::UvcDevice::isMetadataNode(metaInfo.value())) {
        TRI_LOG_WARN(tri::foundation::LogCategory::Uvc) << config_.metadataNode << " is not a UVC metadata node, ignored";
        return;
    }
    auto ret = metadataReader_.openNode(config_.metadataNode);
    if (!ret) {
        TRI_LOG_WARN(tri::foundation::LogCategory::Uvc) << "metadata node open failed, ignored: " << ret.status().describe();
        return;
    }
    status_.metadataAvailable = true;
    capability_.hasMetadataNode = true;
}

Result<void> CompositeLowThermalCameraDevice::open() {
    if (!status_.configured) return rememberError(ErrorCode::NotInitialized, "composite camera is not initialized");
    if (!status_.enabled) return rememberError(ErrorCode::InvalidArgument, "composite camera is disabled");
    if (v4l2_.isOpen()) return Result<void>::success();

    auto nodeInfo = tri::hardware::uvc::UvcDevice::inspectNode(config_.videoNode);
    if (!nodeInfo) return rememberError(nodeInfo.status().code(), nodeInfo.status().describe());
    if (!tri::hardware::uvc::UvcDevice::isVideoCaptureNode(nodeInfo.value())) {
        return rememberError(ErrorCode::Unsupported, config_.videoNode + " is not a UVC video capture node");
    }

    auto ret = v4l2_.openDevice(config_.videoNode);
    if (!ret) return rememberError(ret.status().code(), ret.status().describe());

    auto cap = v4l2_.queryCapability();
    if (cap) {
        capability_.driver = cap.value().driver;
        capability_.card = cap.value().card;
        capability_.busInfo = cap.value().busInfo;
    }
    auto formats = v4l2_.enumFormats();
    if (formats) capability_.formats = formats.value();

    ret = v4l2_.setFormat(captureFormat_);
    if (!ret) { v4l2_.closeDevice(); return rememberError(ret.status().code(), ret.status().describe()); }
    ret = v4l2_.requestBuffers(4);
    if (!ret) { v4l2_.closeDevice(); return rememberError(ret.status().code(), ret.status().describe()); }

    capability_.hasVideoNode = true;
    status_.opened = true;
    status_.online = true;
    status_.state = CameraLifecycleState::Opened;
    clearError();

    tryOpenMetadataNode();
    TRI_LOG_INFO(tri::foundation::LogCategory::Uvc) << "composite camera opened: video=" << config_.videoNode
                                                     << " metadata=" << config_.metadataNode;
    return Result<void>::success();
}

Result<void> CompositeLowThermalCameraDevice::start() {
    if (!v4l2_.isOpen()) {
        auto ret = open();
        if (!ret) return ret;
    }
    auto ret = v4l2_.startStream();
    if (!ret) return rememberError(ret.status().code(), ret.status().describe());
    status_.streaming = true;
    status_.state = CameraLifecycleState::Streaming;
    clearError();
    return Result<void>::success();
}

void CompositeLowThermalCameraDevice::tryAttachMetadata(CameraFrame& frame) {
    if (!status_.metadataAvailable || !metadataReader_.isOpen()) return;
    auto pkt = metadataReader_.readPacket(1, 4096);
    if (!pkt) {
        if (pkt.status().code() != ErrorCode::Timeout) {
            TRI_LOG_WARN(tri::foundation::LogCategory::Uvc) << "metadata read ignored: " << pkt.status().describe();
        }
        return;
    }
    frame.metadata = std::move(pkt.value().bytes);
    frame.metadataTimestampMs = pkt.value().timestampMs;
    status_.metadataPacketsRead++;
}

Result<CameraFrame> CompositeLowThermalCameraDevice::readFrame(int timeoutMs) {
    if (!v4l2_.streaming()) {
        return Result<CameraFrame>::error(ErrorCode::NotInitialized, "composite camera stream is not started");
    }
    auto view = v4l2_.dequeueBuffer(timeoutMs);
    if (!view) {
        status_.droppedFrames++;
        status_.lastErrorCode = view.status().code();
        status_.lastErrorMessage = view.status().describe();
        return Result<CameraFrame>::error(view.status().code(), view.status().describe());
    }

    CameraFrame frame;
    frame.cameraId = CameraId::CompositeLowThermal;
    frame.sequence = ++sequence_;
    frame.width = captureFormat_.width;
    frame.height = captureFormat_.height;
    frame.pixelformat = captureFormat_.pixelformat;
    frame.v4l2Timestamp = view.value().timestamp;
    frame.receiveTimestampMs = tri::foundation::Clock::nowMs();
    frame.bytes.resize(view.value().bytesUsed);
    if (view.value().bytesUsed > 0 && view.value().data != nullptr) {
        std::memcpy(frame.bytes.data(), view.value().data, view.value().bytesUsed);
    }

    tryAttachMetadata(frame);

    auto ret = v4l2_.enqueueBuffer(view.value().index);
    if (!ret) {
        return Result<CameraFrame>::error(ret.status().code(), ret.status().describe());
    }
    status_.framesRead++;
    clearError();
    return Result<CameraFrame>::ok(std::move(frame));
}

Result<void> CompositeLowThermalCameraDevice::stop() {
    if (!v4l2_.isOpen()) return Result<void>::success();
    auto ret = v4l2_.stopStream();
    status_.streaming = false;
    status_.state = status_.opened ? CameraLifecycleState::Opened : CameraLifecycleState::Initialized;
    if (!ret) return rememberError(ret.status().code(), ret.status().describe());
    return Result<void>::success();
}

void CompositeLowThermalCameraDevice::close() {
    metadataReader_.closeNode();
    if (v4l2_.isOpen()) {
        (void)stop();
        v4l2_.closeDevice();
    }
    status_.opened = false;
    status_.streaming = false;
    status_.online = false;
    status_.metadataAvailable = false;
    status_.state = status_.configured ? CameraLifecycleState::Initialized : CameraLifecycleState::Uninitialized;
}

} // namespace tri::device
