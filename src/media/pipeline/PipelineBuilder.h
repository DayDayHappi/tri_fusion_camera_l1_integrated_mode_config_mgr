#pragma once

#include <memory>

#include "foundation/config/ConfigTypes.h"
#include "foundation/error/Result.h"
#include "media/pipeline/MediaPipeline.h"
#include "media/pipeline/PipelineContext.h"
#include "media/pipeline/PipelineTypes.h"

namespace tri::media {

class PipelineBuilder final {
public:
    static PipelineConfig visibleOnly(const foundation::MediaConfig& media);

    static PipelineConfig compositeOnly(const foundation::MediaConfig& media,
                                        bool decode,
                                        bool encode);

    // 复合相机 YUYV422 / NV12 原始输入：
    // Capture -> FormatConvertNode(NV12) -> MppH264Encoder -> MainStream
    static PipelineConfig compositeYuyvToH264(const foundation::MediaConfig& media);

    // 自动链路：
    // 当前不再从 MediaConfig 里读 composite.format，因为 MediaConfig 没有这个字段。
    // 这里构建的是“可兼容 YUYV / MJPG 的编码链路”：
    //   YUYV: Capture -> FormatConvertNode -> MppH264Encoder -> MainStream
    //   MJPG: Capture -> MjpegDecoder -> FormatConvertNode -> MppH264Encoder -> MainStream
    static PipelineConfig compositeOnlyAuto(const foundation::MediaConfig& media);

    static PipelineConfig visibleCompositeFusionPlaceholder(const foundation::MediaConfig& media);

    static foundation::Result<std::unique_ptr<MediaPipeline>>
    build(const PipelineConfig& config, PipelineContext context);
};

} // namespace tri::media
