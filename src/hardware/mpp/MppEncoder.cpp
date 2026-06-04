#include "hardware/mpp/MppEncoder.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <utility>

#include <rockchip/rk_mpi.h>
#include <rockchip/mpp_buffer.h>
#include <rockchip/mpp_frame.h>
#include <rockchip/mpp_packet.h>
#include <rockchip/rk_venc_cfg.h>

bool hasNalType(const std::vector<std::uint8_t>& data, std::uint8_t targetType) {
    for (std::size_t i = 0; i + 5 < data.size(); ++i) {
        bool start4 = data[i] == 0x00 && data[i + 1] == 0x00 &&
                      data[i + 2] == 0x00 && data[i + 3] == 0x01;

        bool start3 = data[i] == 0x00 && data[i + 1] == 0x00 &&
                      data[i + 2] == 0x01;

        if (start4) {
            const std::uint8_t nalType = data[i + 4] & 0x1f;
            if (nalType == targetType) {
                return true;
            }
        } else if (start3) {
            const std::uint8_t nalType = data[i + 3] & 0x1f;
            if (nalType == targetType) {
                return true;
            }
        }
    }

    return false;
}

bool hasH264Sps(const std::vector<std::uint8_t>& data) {
    return hasNalType(data, 7);
}

bool hasH264Idr(const std::vector<std::uint8_t>& data) {
    return hasNalType(data, 5);
}


bool startsWithAnnexB(const std::vector<std::uint8_t>& data) {
    if (data.size() >= 4 &&
        data[0] == 0x00 && data[1] == 0x00 &&
        data[2] == 0x00 && data[3] == 0x01) {
        return true;
    }

    if (data.size() >= 3 &&
        data[0] == 0x00 && data[1] == 0x00 &&
        data[2] == 0x01) {
        return true;
    }

    return false;
}

bool hasSpsNal(const std::vector<std::uint8_t>& data) {
    for (std::size_t i = 0; i + 4 < data.size(); ++i) {
        bool start4 = data[i] == 0x00 && data[i + 1] == 0x00 &&
                      data[i + 2] == 0x00 && data[i + 3] == 0x01;
        bool start3 = data[i] == 0x00 && data[i + 1] == 0x00 &&
                      data[i + 2] == 0x01;

        if (start4) {
            const auto nal = data[i + 4] & 0x1f;
            if (nal == 7) return true; // SPS
        } else if (start3) {
            const auto nal = data[i + 3] & 0x1f;
            if (nal == 7) return true; // SPS
        }
    }

    return false;
}

namespace tri::hardware::mpp {

using tri::foundation::ErrorCode;
using tri::foundation::Result;

namespace {

::MppCodingType toMppCoding(MppCodec codec) {
    switch (codec) {
        case MppCodec::H264:
            return MPP_VIDEO_CodingAVC;
        case MppCodec::H265:
            return MPP_VIDEO_CodingHEVC;
        default:
            return MPP_VIDEO_CodingUnused;
    }
}

} // namespace

MppEncoder::~MppEncoder() {
    release();
}

Result<void> MppEncoder::init(const MppEncoderConfig& config) {
    release();

    config_ = config;

    std::cerr
        << "[MPP] encoder init config:"
        << " width=" << config.width
        << " height=" << config.height
        << " horStride=" << config.horStride
        << " verStride=" << config.verStride
        << " fps=" << config.fps
        << " gop=" << config.gop
        << " bitrateKbps=" << config.bitrateKbps
        << std::endl;

    ::MppCtx ctx = nullptr;
    ::MppApi* mpi = nullptr;

    ::MPP_RET ret = ::mpp_create(&ctx, &mpi);
    if (ret != MPP_OK) {
        return Result<void>::error(ErrorCode::IoError, "mpp_create encoder failed");
    }

    ret = ::mpp_init(ctx, MPP_CTX_ENC, toMppCoding(config.codec));
    if (ret != MPP_OK) {
        ::mpp_destroy(ctx);
        return Result<void>::error(ErrorCode::IoError, "mpp_init encoder failed");
    }

    const std::uint32_t fps = config.fps ? config.fps : 30;
    const std::uint32_t gop = config.gop ? config.gop : fps;

    std::uint32_t bitrateKbps = config.bitrateKbps ? config.bitrateKbps : 2048;
    if (bitrateKbps < 256) {
        bitrateKbps = 256;
    }
    if (bitrateKbps > 8192) {
        bitrateKbps = 8192;
    }

    const std::uint32_t bitrate = bitrateKbps * 1000U;
    const std::uint32_t horStride = config.horStride ? config.horStride : config.width;
    const std::uint32_t verStride = config.verStride ? config.verStride : config.height;

    std::cerr
        << "[MPP] use MppEncCfg:"
        << " width=" << config.width
        << " height=" << config.height
        << " horStride=" << horStride
        << " verStride=" << verStride
        << " fps=" << fps
        << " gop=" << gop
        << " bitrateKbps=" << bitrateKbps
        << " bitrate=" << bitrate
        << std::endl;

    ::MppEncCfg encCfg = nullptr;
    ret = mpp_enc_cfg_init(&encCfg);
    if (ret != MPP_OK || encCfg == nullptr) {
        mpp_destroy(ctx);
        return Result<void>::error(ErrorCode::IoError, "mpp_enc_cfg_init failed");
    }

    mpp_enc_cfg_set_s32(encCfg, "prep:width", static_cast<int>(config.width));
    mpp_enc_cfg_set_s32(encCfg, "prep:height", static_cast<int>(config.height));
    mpp_enc_cfg_set_s32(encCfg, "prep:hor_stride", static_cast<int>(horStride));
    mpp_enc_cfg_set_s32(encCfg, "prep:ver_stride", static_cast<int>(verStride));
    mpp_enc_cfg_set_s32(encCfg, "prep:format", static_cast<int>(MPP_FMT_YUV420SP));

    mpp_enc_cfg_set_s32(encCfg, "rc:mode", static_cast<int>(MPP_ENC_RC_MODE_CBR));
    mpp_enc_cfg_set_s32(encCfg, "rc:bps_target", static_cast<int>(bitrate));
    mpp_enc_cfg_set_s32(encCfg, "rc:bps_max", static_cast<int>(bitrate * 11 / 10));
    mpp_enc_cfg_set_s32(encCfg, "rc:bps_min", static_cast<int>(bitrate * 9 / 10));

    mpp_enc_cfg_set_s32(encCfg, "rc:fps_in_flex", 0);
    mpp_enc_cfg_set_s32(encCfg, "rc:fps_in_num", static_cast<int>(fps));
    mpp_enc_cfg_set_s32(encCfg, "rc:fps_in_denorm", 1);
    mpp_enc_cfg_set_s32(encCfg, "rc:fps_out_flex", 0);
    mpp_enc_cfg_set_s32(encCfg, "rc:fps_out_num", static_cast<int>(fps));
    mpp_enc_cfg_set_s32(encCfg, "rc:fps_out_denorm", 1);
    mpp_enc_cfg_set_s32(encCfg, "rc:gop", static_cast<int>(gop));

    mpp_enc_cfg_set_s32(encCfg, "codec:type", static_cast<int>(toMppCoding(config.codec)));

    if (config.codec == MppCodec::H264) {
        mpp_enc_cfg_set_s32(encCfg, "h264:profile", 100);
        mpp_enc_cfg_set_s32(encCfg, "h264:level", 40);
        mpp_enc_cfg_set_s32(encCfg, "h264:qp_init", 30);
        mpp_enc_cfg_set_s32(encCfg, "h264:qp_max", 44);
        mpp_enc_cfg_set_s32(encCfg, "h264:qp_min", 24);
        mpp_enc_cfg_set_s32(encCfg, "h264:qp_max_i", 42);
        mpp_enc_cfg_set_s32(encCfg, "h264:qp_min_i", 22);
    }

    ret = mpi->control(ctx, MPP_ENC_SET_CFG, encCfg);
    mpp_enc_cfg_deinit(encCfg);

    if (ret != MPP_OK) {
        mpp_destroy(ctx);
        return Result<void>::error(ErrorCode::IoError, "MPP_ENC_SET_CFG failed");
    }

    ctx_ = ctx;
    mpi_ = mpi;

    h264ExtraInfo_.clear();

if (config.codec == MppCodec::H264) {
::MppPacket extra = nullptr;
MPP_RET hdrRet = mpi->control(ctx, MPP_ENC_GET_EXTRA_INFO, &extra);

if (hdrRet == MPP_OK && extra != nullptr) {
    void* ptr = mpp_packet_get_pos(extra);
    const std::size_t len = mpp_packet_get_length(extra);

    if (ptr && len > 0) {
        const auto* p = static_cast<const std::uint8_t*>(ptr);
        h264ExtraInfo_.assign(p, p + len);
    }

    mpp_packet_deinit(&extra);
}

    std::cerr << "[MPP] h264 extra info bytes="
              << h264ExtraInfo_.size() << std::endl;
}
    initialized_ = true;

    return Result<void>::success();
}

Result<MppEncodedPacket> MppEncoder::encodeNv12(const std::uint8_t* data,
                                                std::size_t size,
                                                std::uint64_t ptsMs) {
    if (!initialized_ || ctx_ == nullptr || mpi_ == nullptr) {
        return Result<MppEncodedPacket>::error(ErrorCode::NotInitialized,
                                               "MPP encoder not initialized");
    }

    const std::uint32_t horStride = config_.horStride ? config_.horStride : config_.width;
    const std::uint32_t verStride = config_.verStride ? config_.verStride : config_.height;

    const std::size_t expected =
        static_cast<std::size_t>(horStride) *
        static_cast<std::size_t>(verStride) * 3 / 2;

    if (data == nullptr || size < expected) {
        return Result<MppEncodedPacket>::error(ErrorCode::InvalidArgument,
                                               "NV12 input too small");
    }

    auto* ctx = static_cast<::MppCtx>(ctx_);
    auto* mpi = static_cast<::MppApi*>(mpi_);

    ::MppBuffer buffer = nullptr;
    ::MPP_RET ret = ::mpp_buffer_get(nullptr, &buffer, expected);
    if (ret != MPP_OK || buffer == nullptr) {
        return Result<MppEncodedPacket>::error(ErrorCode::IoError,
                                               "mpp_buffer_get failed");
    }

    std::memcpy(::mpp_buffer_get_ptr(buffer), data, expected);

    ::MppFrame frame = nullptr;
    ret = ::mpp_frame_init(&frame);
    if (ret != MPP_OK || frame == nullptr) {
        ::mpp_buffer_put(buffer);
        return Result<MppEncodedPacket>::error(ErrorCode::IoError,
                                               "mpp_frame_init failed");
    }

    ::mpp_frame_set_width(frame, config_.width);
    ::mpp_frame_set_height(frame, config_.height);
    ::mpp_frame_set_hor_stride(frame, horStride);
    ::mpp_frame_set_ver_stride(frame, verStride);
    ::mpp_frame_set_fmt(frame, MPP_FMT_YUV420SP);
    ::mpp_frame_set_buffer(frame, buffer);
    ::mpp_frame_set_pts(frame, static_cast<std::int64_t>(ptsMs));

    ret = mpi->encode_put_frame(ctx, frame);

    ::mpp_frame_deinit(&frame);
    ::mpp_buffer_put(buffer);

    if (ret != MPP_OK) {
        return Result<MppEncodedPacket>::error(ErrorCode::IoError,
                                               "encode_put_frame failed");
    }

    ::MppPacket packet = nullptr;
    ret = mpi->encode_get_packet(ctx, &packet);
    if (ret != MPP_OK) {
        return Result<MppEncodedPacket>::error(ErrorCode::IoError,
                                               "encode_get_packet failed");
    }

    if (packet == nullptr) {
        return Result<MppEncodedPacket>::error(ErrorCode::IoError,
                                               "encode_get_packet returned null packet");
    }

    void* ptr = ::mpp_packet_get_pos(packet);
    const std::size_t len = ::mpp_packet_get_length(packet);

MppEncodedPacket out;
out.codec = config_.codec;
out.ptsMs = ptsMs;


const auto* p = static_cast<const std::uint8_t*>(ptr);

out.data.assign(p, p + len);
out.keyFrame = false;

if (config_.codec == MppCodec::H264) {
    out.keyFrame = hasH264Idr(out.data);
}
/*
 * GStreamer 等效设计：
 * h264parse config-interval=1
 *
 * 这里采用更强策略：每个 IDR 前都补 SPS/PPS。
 * 这样 ffplay / VLC / GB28181 平台晚启动时，也能在下一个 IDR 快速解码。
 */
if (config_.codec == MppCodec::H264 &&
    prependExtraInfoOnKeyFrame_ &&
    out.keyFrame &&
    !h264ExtraInfo_.empty() &&
    !hasSpsNal(out.data)) {
    std::vector<std::uint8_t> merged;
    merged.reserve(h264ExtraInfo_.size() + out.data.size());

    merged.insert(merged.end(), h264ExtraInfo_.begin(), h264ExtraInfo_.end());
    merged.insert(merged.end(), out.data.begin(), out.data.end());

    out.data.swap(merged);
}

    ::mpp_packet_deinit(&packet);

    return Result<MppEncodedPacket>::ok(std::move(out));
}

void MppEncoder::release() {
    if (ctx_ != nullptr) {
        auto* ctx = static_cast<::MppCtx>(ctx_);
        ::mpp_destroy(ctx);
    }

    ctx_ = nullptr;
    mpi_ = nullptr;
    initialized_ = false;
}



} // namespace tri::hardware::mpp
