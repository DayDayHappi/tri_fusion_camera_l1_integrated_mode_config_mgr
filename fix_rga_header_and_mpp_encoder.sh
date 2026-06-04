#!/usr/bin/env bash
set -e

SDK_ROOT="/home/alientek/tronlong/RK3588/rk3588_linux_release"
SYSROOT="$HOME/rk3588_sysroot"

echo "[INFO] SDK_ROOT=${SDK_ROOT}"
echo "[INFO] SYSROOT=${SYSROOT}"

# ============================================================
# 1. Fix RGA headers: copy rga.h and related headers.
# ============================================================
echo
echo "[STEP 1] searching rga.h in SDK ..."

RGA_H_PATH="$(find "${SDK_ROOT}/external/linux-rga" -name "rga.h" 2>/dev/null | head -n 1 || true)"

if [ -z "${RGA_H_PATH}" ]; then
    echo "[ERROR] rga.h not found under ${SDK_ROOT}/external/linux-rga"
    echo "[HINT] run: find ${SDK_ROOT} -name 'rga.h' 2>/dev/null"
    exit 1
fi

RGA_H_DIR="$(dirname "${RGA_H_PATH}")"

echo "[FOUND] rga.h: ${RGA_H_PATH}"
echo "[COPY] RGA core headers: ${RGA_H_DIR} -> ${SYSROOT}/usr/include/rga"
mkdir -p "${SYSROOT}/usr/include/rga"
cp -r "${RGA_H_DIR}/"* "${SYSROOT}/usr/include/rga/"

echo "[CHECK] RGA headers:"
find "${SYSROOT}/usr/include/rga" -maxdepth 2 -name "im2d.h" -o -name "im2d_type.h" -o -name "rga.h"

# ============================================================
# 2. Patch MppEncoder.cpp.
# ============================================================
echo
echo "[STEP 2] patch src/hardware/mpp/MppEncoder.cpp ..."

FILE="src/hardware/mpp/MppEncoder.cpp"
TS="$(date +%Y%m%d_%H%M%S)"
BACKUP="${FILE}.bak_mpp_sdk_compat_${TS}"

if [ ! -f "$FILE" ]; then
    echo "[ERROR] missing file: $FILE"
    exit 1
fi

cp "$FILE" "$BACKUP"
echo "[BACKUP] $FILE -> $BACKUP"

cat > "$FILE" <<'CPP'
#include "hardware/mpp/MppEncoder.h"

#include <cstdint>
#include <cstring>
#include <utility>

#include <rockchip/rk_mpi.h>
#include <rockchip/mpp_buffer.h>
#include <rockchip/mpp_frame.h>
#include <rockchip/mpp_packet.h>

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

    ::MppEncPrepCfg prep{};
    prep.change = MPP_ENC_PREP_CFG_CHANGE_INPUT | MPP_ENC_PREP_CFG_CHANGE_FORMAT;
    prep.width = config.width;
    prep.height = config.height;
    prep.hor_stride = config.horStride ? config.horStride : config.width;
    prep.ver_stride = config.verStride ? config.verStride : config.height;
    prep.format = MPP_FMT_YUV420SP;

    ret = mpi->control(ctx, MPP_ENC_SET_PREP_CFG, &prep);
    if (ret != MPP_OK) {
        ::mpp_destroy(ctx);
        return Result<void>::error(ErrorCode::IoError, "MPP_ENC_SET_PREP_CFG failed");
    }

    ::MppEncRcCfg rc{};
    rc.change = MPP_ENC_RC_CFG_CHANGE_ALL;
    rc.rc_mode = MPP_ENC_RC_MODE_CBR;
    rc.quality = MPP_ENC_RC_QUALITY_MEDIUM;

    const std::uint32_t bitrate = config.bitrateKbps ? config.bitrateKbps * 1000 : 2000 * 1000;
    const std::uint32_t fps = config.fps ? config.fps : 30;

    rc.bps_target = bitrate;
    rc.bps_max = bitrate * 17 / 16;
    rc.bps_min = bitrate * 15 / 16;

    rc.fps_in_flex = 0;
    rc.fps_in_num = fps;
    rc.fps_in_denom = 1;

    rc.fps_out_flex = 0;
    rc.fps_out_num = fps;
    rc.fps_out_denom = 1;

    rc.gop = config.gop ? config.gop : fps;

    ret = mpi->control(ctx, MPP_ENC_SET_RC_CFG, &rc);
    if (ret != MPP_OK) {
        ::mpp_destroy(ctx);
        return Result<void>::error(ErrorCode::IoError, "MPP_ENC_SET_RC_CFG failed");
    }

    ::MppEncCodecCfg codec{};
    codec.coding = toMppCoding(config.codec);

    if (config.codec == MppCodec::H264) {
        codec.h264.change = MPP_ENC_H264_CFG_CHANGE_PROFILE |
                             MPP_ENC_H264_CFG_CHANGE_ENTROPY |
                             MPP_ENC_H264_CFG_CHANGE_TRANS_8x8;

        codec.h264.profile = 100;
        codec.h264.level = 40;
        codec.h264.entropy_coding_mode = 1;
        codec.h264.cabac_init_idc = 0;
        codec.h264.transform8x8_mode = 1;
    }

    ret = mpi->control(ctx, MPP_ENC_SET_CODEC_CFG, &codec);
    if (ret != MPP_OK) {
        ::mpp_destroy(ctx);
        return Result<void>::error(ErrorCode::IoError, "MPP_ENC_SET_CODEC_CFG failed");
    }

    ctx_ = ctx;
    mpi_ = mpi;
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
    out.keyFrame = false;

    if (ptr != nullptr && len > 0) {
        out.data.resize(len);
        std::memcpy(out.data.data(), ptr, len);
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
CPP

echo "[DONE] MppEncoder.cpp patched."

# ============================================================
# 3. Quick checks.
# ============================================================
echo
echo "[CHECK] remaining problematic tokens:"
grep -R "fps_in_denorm\|fps_out_denorm\|mpp_packet_is_intra\|ErrorCode::HardwareError\|ErrorCode::InvalidState" -n src/hardware/mpp/MppEncoder.cpp || echo "[OK] no problematic tokens in MppEncoder.cpp"

echo
echo "[CHECK] SDK RGA rga.h:"
find "${SYSROOT}/usr/include/rga" -name "rga.h"

echo
echo "[DONE] patch finished."
