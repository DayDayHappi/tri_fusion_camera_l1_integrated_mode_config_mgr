#pragma once

#include "foundation/error/Result.h"
#include "mode/ModeManager.h"

namespace tri::service {

// 第一版 L7 不直接持有 MediaPipeline。
// 媒体链路的创建/停止由 L6 ModeManager/ModeSwitchExecutor 负责，
// 这里仅提供“停止当前模式链路”的服务门面，避免 L7 越层访问 Pipeline。
class MediaService final {
public:
    tri::foundation::Result<void> init(tri::mode::ModeManager* modeManager);
    tri::foundation::Result<void> stopCurrentPipeline();

    bool initialized() const noexcept { return modeManager_ != nullptr; }

private:
    tri::mode::ModeManager* modeManager_{nullptr};
};

} // namespace tri::service
