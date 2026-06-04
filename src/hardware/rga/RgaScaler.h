#pragma once
#include "foundation/error/Result.h"
#include "hardware/rga/RgaTypes.h"
namespace tri::hardware::rga { class RgaScaler { public: foundation::Result<void> scale(const RgaImageDesc&, const RgaImageDesc&) { return foundation::Result<void>::error(foundation::ErrorCode::Unsupported, "RGA scaler backend not linked"); } }; }
