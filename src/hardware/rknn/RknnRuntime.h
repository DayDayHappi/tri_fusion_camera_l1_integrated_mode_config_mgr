#pragma once
#include "foundation/error/Result.h"
#include "hardware/rknn/RknnTypes.h"
namespace tri::hardware::rknn { class RknnRuntime { public: foundation::Result<void> load(const RknnModelInfo&) { return foundation::Result<void>::error(foundation::ErrorCode::Unsupported, "RKNN runtime backend not linked"); } void release() {} }; }
