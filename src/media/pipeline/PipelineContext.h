#pragma once

#include "device/CameraManager.h"
#include "foundation/event/EventBus.h"
#include "hardware/mpp/MppDecoder.h"
#include "hardware/mpp/MppEncoder.h"
#include "media/stream/MainStream.h"

namespace tri::media {

struct PipelineContext {
    device::CameraManager* cameraManager{nullptr};
    MainStream* mainStream{nullptr};
    foundation::EventBus* eventBus{nullptr};
    hardware::mpp::MppDecoder* mjpegDecoder{nullptr};
    hardware::mpp::MppEncoder* encoder{nullptr};
};

} // namespace tri::media
