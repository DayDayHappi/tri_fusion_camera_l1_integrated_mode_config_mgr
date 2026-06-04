#include "media/pipeline/PipelineBuilder.h"

namespace tri::media {
using tri::foundation::Result;

PipelineConfig PipelineBuilder::visibleOnly(const foundation::MediaConfig& media) {
    PipelineConfig cfg;
    cfg.type = PipelineType::VisibleOnly;
    cfg.requireVisible = true;
    cfg.requireComposite = false;

    // 当前先保持原可见光单路行为。
    // 后续如果可见光也是 YUYV，可单独改成 visibleYuyvToH264。
    cfg.useDecoder = false;
    cfg.useEncoder = false;
    cfg.useFormatConvert = false;
    cfg.encoderInputFormat = VideoPixelFormat::Nv12;
    cfg.passthroughEncoded = true;

    cfg.media = media;
    return cfg;
}

PipelineConfig PipelineBuilder::compositeOnly(const foundation::MediaConfig& media,
                                              bool decode,
                                              bool encode) {
    PipelineConfig cfg;
    cfg.type = PipelineType::CompositeOnly;
    cfg.requireVisible = false;
    cfg.requireComposite = true;

    cfg.useDecoder = decode;
    cfg.useEncoder = encode;
    cfg.useFormatConvert = encode;
    cfg.encoderInputFormat = VideoPixelFormat::Nv12;

    // 只有输入本身已经是编码帧，并且不需要编码时，才允许 passthrough。
    cfg.passthroughEncoded = !encode;

    cfg.media = media;
    return cfg;
}

PipelineConfig PipelineBuilder::compositeYuyvToH264(const foundation::MediaConfig& media) {
    PipelineConfig cfg;
    cfg.type = PipelineType::CompositeOnly;
    cfg.requireVisible = false;
    cfg.requireComposite = true;

    // YUYV422 是裸帧，不需要 MJPEG 解码。
    cfg.useDecoder = false;

    // YUYV422 不能直接进入 MainStream，必须编码成 H264。
    cfg.useEncoder = true;

    // MPP H264 编码器要求 NV12 输入，所以 YUYV422 需要先转 NV12。
    cfg.useFormatConvert = true;
    cfg.encoderInputFormat = VideoPixelFormat::Nv12;

    // 禁止把 YUYV 原始帧伪装成 EncodedFrame。
    cfg.passthroughEncoded = false;

    cfg.media = media;
    return cfg;
}

PipelineConfig PipelineBuilder::compositeOnlyAuto(const foundation::MediaConfig& media) {
    PipelineConfig cfg;
    cfg.type = PipelineType::CompositeOnly;
    cfg.requireVisible = false;
    cfg.requireComposite = true;

    // 这里不要再访问 media.composite.format。
    // 当前 MediaConfig 里没有 composite 字段，复合相机输入格式来自 camera.yaml。
    //
    // 这条自动链路兼容：
    // 1. YUYV422:
    //      Capture -> FormatConvertNode(NV12) -> MppH264Encoder -> MainStream
    //
    // 2. MJPG:
    //      Capture -> MjpegDecoder -> FormatConvertNode(NV12) -> MppH264Encoder -> MainStream
    //
    // 对 YUYV422 来说，prepareFrameForPipeline() 会跳过 decoder，只进入 FormatConvertNode。
    // 对 MJPG 来说，会先走 decoder。
    cfg.useDecoder = true;
    cfg.useEncoder = true;
    cfg.useFormatConvert = true;
    cfg.encoderInputFormat = VideoPixelFormat::Nv12;
    cfg.passthroughEncoded = false;

    cfg.media = media;
    return cfg;
}

PipelineConfig PipelineBuilder::visibleCompositeFusionPlaceholder(const foundation::MediaConfig& media) {
    PipelineConfig cfg;
    cfg.type = PipelineType::VisibleCompositeFusion;
    cfg.requireVisible = true;
    cfg.requireComposite = true;

    cfg.useDecoder = true;
    cfg.useEncoder = true;
    cfg.useFormatConvert = true;
    cfg.encoderInputFormat = VideoPixelFormat::Nv12;
    cfg.passthroughEncoded = false;

    cfg.media = media;
    return cfg;
}

Result<std::unique_ptr<MediaPipeline>> PipelineBuilder::build(const PipelineConfig& config,
                                                              PipelineContext context) {
    auto pipeline = std::make_unique<MediaPipeline>();
    auto ret = pipeline->init(config, context);
    if (!ret) {
        return Result<std::unique_ptr<MediaPipeline>>::error(ret.status().code(),
                                                             ret.status().describe());
    }

    return Result<std::unique_ptr<MediaPipeline>>::ok(std::move(pipeline));
}

} // namespace tri::media
