#include "media/pipeline/MediaPipeline.h"
#include <string>
#include <utility>
#include "foundation/event/Event.h"
#include "foundation/log/Logger.h"
#include "foundation/time/Clock.h"
#include "media/decode/DecoderFactory.h"
#include "media/encode/EncoderFactory.h"

namespace tri::media {
using tri::foundation::ErrorCode;
using tri::foundation::Event;
using tri::foundation::EventType;
using tri::foundation::LogCategory;
using tri::foundation::Result;
using tri::foundation::Status;

MediaPipeline::~MediaPipeline() { stop(); }

Result<void> MediaPipeline::init(PipelineConfig config, PipelineContext context) {
    if (state_ == PipelineState::Running) return Result<void>::error(ErrorCode::Busy, "pipeline is running");
    config_ = std::move(config);
    context_ = context;
    stats_ = PipelineStats{};
    graph_.clear();

    auto ret = validateContext();
    if (!ret) return ret;

    if (config_.requireVisible) {
        visibleCapture_ = std::make_unique<VisibleCaptureNode>();
        ret = visibleCapture_->init(context_.cameraManager);
        if (!ret) return ret;
        graph_.addNode("VisibleCaptureNode");
    }
    if (config_.requireComposite) {
        compositeCapture_ = std::make_unique<CompositeCaptureNode>();
        ret = compositeCapture_->init(context_.cameraManager);
        if (!ret) return ret;
        graph_.addNode("CompositeCaptureNode");
    }

    if (config_.useDecoder) {
        if (context_.mjpegDecoder != nullptr) decoder_ = DecoderFactory::createMppMjpeg(context_.mjpegDecoder);
        else decoder_ = DecoderFactory::createPassthroughMjpeg();
        ret = decoder_->init();
        if (!ret) return ret;
        graph_.addNode(context_.mjpegDecoder ? "MppMjpegDecoder" : "MjpegDecoderPassthrough");
    }

if (config_.useEncoder && config_.useFormatConvert) {
    formatConvert_ = std::make_unique<FormatConvertNode>(config_.encoderInputFormat);
    graph_.addNode(std::string("FormatConvertNode(") +
                   toString(config_.encoderInputFormat) + ")");
}

if (config_.useEncoder) {
    encoder_ = EncoderFactory::createMppH264(context_.encoder);
    ret = encoder_->init(context_.mainStream->profile());
    if (!ret) return ret;
    graph_.addNode("MppH264Encoder");
} else if (config_.passthroughEncoded) {
        graph_.addNode("EncodedPassthrough");
    }

    graph_.addNode("MainStream");
    state_ = PipelineState::Initialized;
    return Result<void>::success();
}

Result<void> MediaPipeline::validateContext() const {
    if (context_.cameraManager == nullptr) return Result<void>::error(ErrorCode::InvalidArgument, "pipeline camera manager is null");
    if (context_.mainStream == nullptr) return Result<void>::error(ErrorCode::InvalidArgument, "pipeline main stream is null");
    if (config_.requireVisible && context_.cameraManager->visibleCamera() == nullptr) {
        return Result<void>::error(ErrorCode::VisibleCameraOffline, "visible camera is not available");
    }
    if (config_.requireComposite && context_.cameraManager->compositeCamera() == nullptr) {
        return Result<void>::error(ErrorCode::CompositeCameraOffline, "composite camera is not available");
    }
    if (config_.useEncoder && context_.encoder == nullptr) {
        return Result<void>::error(ErrorCode::EncoderInitFailed, "encoder is required but not provided");
    }
    return Result<void>::success();
}

Result<void> MediaPipeline::start() {
    if (state_ != PipelineState::Initialized && state_ != PipelineState::Stopped) {
        return Result<void>::error(ErrorCode::NotInitialized, "pipeline is not initialized");
    }
    auto ret = startCaptureNodes();
    if (!ret) return ret;
    context_.mainStream->start();
    stopFlag_ = false;
    worker_ = std::thread([this] { runLoop(); });
    state_ = PipelineState::Running;
    publishEvent(EventType::PipelineStarted, "pipeline_started");
    return Result<void>::success();
}

Result<void> MediaPipeline::startCaptureNodes() {
    if (visibleCapture_) {
        auto ret = visibleCapture_->start();
        if (!ret) return ret;
    }
    if (compositeCapture_) {
        auto ret = compositeCapture_->start();
        if (!ret) {
            if (visibleCapture_) (void)visibleCapture_->stop();
            return ret;
        }
    }
    return Result<void>::success();
}

void MediaPipeline::stopCaptureNodes() {
    if (visibleCapture_) (void)visibleCapture_->stop();
    if (compositeCapture_) (void)compositeCapture_->stop();
}

void MediaPipeline::stop() {
    stopFlag_ = true;
    if (worker_.joinable()) worker_.join();
    stopCaptureNodes();
    if (decoder_) decoder_->release();
    if (encoder_) encoder_->release();
    if (context_.mainStream != nullptr) context_.mainStream->stop();
    if (state_ == PipelineState::Running || state_ == PipelineState::Initialized) {
        publishEvent(EventType::PipelineStopped, "pipeline_stopped");
    }
    state_ = PipelineState::Stopped;
}

void MediaPipeline::runLoop() {
    while (!stopFlag_) {
        auto ret = processOneFrame();
        if (!ret) {
            recordError(ret.status());
        }
    }
}

Result<void> MediaPipeline::processOneFrame() {
    VideoFrame frame;
    if (config_.requireComposite && compositeCapture_) {
        auto ret = compositeCapture_->capture(config_.captureTimeoutMs);
        if (!ret) return Result<void>::error(ret.status().code(), ret.status().describe());
        frame = std::move(ret.value());
    } else if (config_.requireVisible && visibleCapture_) {
        auto ret = visibleCapture_->capture(config_.captureTimeoutMs);
        if (!ret) return Result<void>::error(ret.status().code(), ret.status().describe());
        frame = std::move(ret.value());
    } else {
        return Result<void>::error(ErrorCode::PipelineBuildFailed, "pipeline has no capture node");
    }

    ++stats_.capturedFrames;
    auto prepared = prepareFrameForPipeline(frame);
    if (!prepared) return prepared;
    return publishFrame(frame);
}

Result<void> MediaPipeline::prepareFrameForPipeline(VideoFrame& frame) {
    if (frame.format == VideoPixelFormat::Unknown) {
        return Result<void>::error(ErrorCode::Unsupported,
                                   "captured frame pixel format is unknown");
    }

    if (frame.format == VideoPixelFormat::Mjpeg) {
        if (decoder_) {
            auto decoded = decoder_->decode(frame);
            if (!decoded) {
                return Result<void>::error(ErrorCode::MjpegDecodeFailed,
                                           decoded.status().describe());
            }

            frame = std::move(decoded.value());
            ++stats_.decodedFrames;
        } else if (encoder_) {
            return Result<void>::error(ErrorCode::MjpegDecodeFailed,
                "MJPG input requires decoder before MPP encoder");
        } else {
            return Result<void>::success();
        }
    }

    if (encoder_) {
        return prepareFrameForEncoder(frame);
    }

    return Result<void>::success();
}

Result<void> MediaPipeline::prepareFrameForEncoder(VideoFrame& frame) {
    if (frame.format == config_.encoderInputFormat) {
        return Result<void>::success();
    }

    if (!formatConvert_) {
        return Result<void>::error(ErrorCode::Unsupported,
            std::string("encoder input format mismatch: captured=") +
            toString(frame.format) +
            ", required=" +
            toString(config_.encoderInputFormat));
    }

    auto converted = formatConvert_->process(frame);
    if (!converted) {
        return Result<void>::error(converted.status().code(),
                                   converted.status().describe());
    }

    frame = std::move(converted.value());
    return Result<void>::success();
}

Result<void> MediaPipeline::publishFrame(const VideoFrame& frame) {
    if (encoder_) {
        auto encoded = encoder_->encode(frame);
        if (!encoded) {
            return Result<void>::error(encoded.status().code(),
                                       encoded.status().describe());
        }

        ++stats_.encodedFrames;
        return context_.mainStream->push(std::move(encoded.value()));
    }

    if (!config_.passthroughEncoded) {
        return Result<void>::error(ErrorCode::Unsupported,
            "pipeline has no encoder and passthrough is disabled");
    }

    if (frame.format != VideoPixelFormat::Mjpeg) {
        return Result<void>::error(ErrorCode::Unsupported,
            std::string("raw frame ") + toString(frame.format) +
            " cannot be published as encoded MainStream");
    }

    EncodedFrame encoded;
    encoded.codec = hardware::mpp::MppCodec::Mjpeg;
    encoded.data = frame.data;
    encoded.ptsMs = frame.ptsMs == 0 ? foundation::Clock::nowMs() : frame.ptsMs;
    encoded.sequence = frame.sequence;

    ++stats_.encodedFrames;
    return context_.mainStream->push(std::move(encoded));
}

void MediaPipeline::recordError(const Status& status) {
    ++stats_.errors;
    stats_.lastError = status.describe();
    TRI_LOG_WARN(LogCategory::Media) << "pipeline error: " << stats_.lastError;
    if (status.code() != ErrorCode::Timeout && status.code() != ErrorCode::SerialReadTimeout) {
        state_ = PipelineState::Error;
    }
}

void MediaPipeline::publishEvent(EventType type, const char* name) {
    if (context_.eventBus == nullptr) return;
    Event ev;
    ev.type = type;
    ev.name = name;
    ev.fields["pipeline_type"] = toString(config_.type);
    ev.fields["state"] = std::to_string(static_cast<int>(state_));
    context_.eventBus->publish(ev);
}

} // namespace tri::media
