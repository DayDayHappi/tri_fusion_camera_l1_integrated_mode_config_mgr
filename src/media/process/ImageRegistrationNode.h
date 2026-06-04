#pragma once
#include "foundation/error/Result.h"
#include "media/frame/SyncedFrameGroup.h"
namespace tri::media { class ImageRegistrationNode final { public: foundation::Result<SyncedFrameGroup> process(const SyncedFrameGroup& group) { return foundation::Result<SyncedFrameGroup>::ok(group); } }; }
