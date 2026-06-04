#pragma once
#include "foundation/error/Result.h"
#include "hardware/rga/RgaTypes.h"
namespace tri::hardware::rga { class RgaBlitter { public: foundation::Result<void> blit(const RgaImageDesc&, const RgaImageDesc&) { return foundation::Result<void>::error(foundation::ErrorCode::Unsupported, "RGA blitter backend not linked"); } }; }
