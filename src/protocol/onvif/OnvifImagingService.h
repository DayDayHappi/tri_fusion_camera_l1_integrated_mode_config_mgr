#pragma once
#include "foundation/error/Result.h"
namespace tri::protocol::onvif { class OnvifImagingService final { public: tri::foundation::Result<void> init() { initialized_ = true; return tri::foundation::Result<void>::success(); } bool initialized() const noexcept { return initialized_; } private: bool initialized_{false}; }; }
