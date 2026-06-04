#pragma once

#include "foundation/config/ConfigTypes.h"
#include "foundation/error/Result.h"
#include "hardware/serial/SerialTypes.h"
#include "hardware/v4l2/V4L2Types.h"

namespace tri::hardware {

foundation::Result<v4l2::CaptureFormat> makeCaptureFormat(const foundation::CameraEndpointConfig& cfg);
foundation::Result<serial::SerialPortConfig> makeSerialPortConfig(const foundation::SerialConfig& cfg);

} // namespace tri::hardware
