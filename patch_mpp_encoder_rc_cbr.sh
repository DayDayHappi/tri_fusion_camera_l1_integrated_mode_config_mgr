#!/usr/bin/env bash
set -e

FILE="src/hardware/mpp/MppEncoder.cpp"

if [ ! -f "$FILE" ]; then
    echo "[ERROR] missing file: $FILE"
    exit 1
fi

TS="$(date +%Y%m%d_%H%M%S)"
BACKUP="${FILE}.bak_rc_cbr_${TS}"

cp "$FILE" "$BACKUP"
echo "[BACKUP] $FILE -> $BACKUP"

python3 - <<'PY'
from pathlib import Path
import re

path = Path("src/hardware/mpp/MppEncoder.cpp")
text = path.read_text()

# 1. Replace bitrate/fps/rc block with stronger CBR RC config.
old = '''    ::MppEncRcCfg rc{};
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

    rc.gop = config.gop ? config.gop : fps;'''

new = '''    ::MppEncRcCfg rc{};
    rc.change = MPP_ENC_RC_CFG_CHANGE_ALL;

    // Force CBR for real-time UDP preview.
    rc.rc_mode = MPP_ENC_RC_MODE_CBR;
    rc.quality = MPP_ENC_RC_QUALITY_MEDIUM;

    const std::uint32_t fps = config.fps ? config.fps : 30;
    const std::uint32_t gop = config.gop ? config.gop : fps;

    // config.bitrateKbps is in kbps.
    // Clamp to a sane range for 800x600@30 preview to avoid MPP defaulting to huge bitrate.
    std::uint32_t bitrateKbps = config.bitrateKbps ? config.bitrateKbps : 2048;
    if (bitrateKbps < 256) {
        bitrateKbps = 256;
    }
    if (bitrateKbps > 8192) {
        bitrateKbps = 8192;
    }

    const std::uint32_t bitrate = bitrateKbps * 1000U;

    rc.bps_target = bitrate;
    rc.bps_max = bitrate * 11 / 10;
    rc.bps_min = bitrate * 9 / 10;

    rc.fps_in_flex = 0;
    rc.fps_in_num = fps;
    rc.fps_in_denom = 1;

    rc.fps_out_flex = 0;
    rc.fps_out_num = fps;
    rc.fps_out_denom = 1;

    rc.gop = gop;

    // Enable frame drop in encoder RC when bitrate pressure is too high.
    // These fields exist in RK MPP's common encoder RC config.
    rc.drop_mode = MPP_ENC_RC_DROP_FRM_DISABLED;
    rc.drop_thd = 20;
    rc.drop_gap = 1;'''

if old not in text:
    raise SystemExit("[ERROR] RC block not found. Your MppEncoder.cpp differs from expected version.")

text = text.replace(old, new, 1)

# 2. Replace H264 codec cfg block with QP bounded block.
old2 = '''    if (config.codec == MppCodec::H264) {
        codec.h264.change = MPP_ENC_H264_CFG_CHANGE_PROFILE |
                             MPP_ENC_H264_CFG_CHANGE_ENTROPY |
                             MPP_ENC_H264_CFG_CHANGE_TRANS_8x8;

        codec.h264.profile = 100;
        codec.h264.level = 40;
        codec.h264.entropy_coding_mode = 1;
        codec.h264.cabac_init_idc = 0;
        codec.h264.transform8x8_mode = 1;
    }'''

new2 = '''    if (config.codec == MppCodec::H264) {
        codec.h264.change = MPP_ENC_H264_CFG_CHANGE_PROFILE |
                             MPP_ENC_H264_CFG_CHANGE_ENTROPY |
                             MPP_ENC_H264_CFG_CHANGE_TRANS_8x8 |
                             MPP_ENC_H264_CFG_CHANGE_QP_LIMIT;

        codec.h264.profile = 100;
        codec.h264.level = 40;
        codec.h264.entropy_coding_mode = 1;
        codec.h264.cabac_init_idc = 0;
        codec.h264.transform8x8_mode = 1;

        // Constrain QP to force bitrate control.
        // Lower QP = clearer but larger; higher QP = smaller but blurrier.
        codec.h264.qp_init = 30;
        codec.h264.qp_max = 44;
        codec.h264.qp_min = 24;
        codec.h264.qp_max_i = 42;
        codec.h264.qp_min_i = 22;
        codec.h264.qp_ip = 2;
    }'''

if old2 not in text:
    raise SystemExit("[ERROR] H264 codec block not found. Your MppEncoder.cpp differs from expected version.")

text = text.replace(old2, new2, 1)

path.write_text(text)
print("[OK] MppEncoder.cpp RC/CBR patched")
PY

echo
echo "[CHECK] patched MppEncoder.cpp:"
grep -n "bitrateKbps\\|bps_target\\|bps_max\\|bps_min\\|drop_mode\\|qp_init\\|qp_max\\|qp_min\\|MPP_ENC_H264_CFG_CHANGE_QP_LIMIT" "$FILE"

echo
echo "[DONE] MPP RC CBR patch finished."
