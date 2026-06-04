#!/usr/bin/env bash
set -e

FILE="src/hardware/mpp/MppEncoder.cpp"

if [ ! -f "$FILE" ]; then
    echo "[ERROR] missing file: $FILE"
    exit 1
fi

TS="$(date +%Y%m%d_%H%M%S)"
BACKUP="${FILE}.bak_use_mpp_enc_cfg_v3_${TS}"
cp "$FILE" "$BACKUP"
echo "[BACKUP] $FILE -> $BACKUP"

python3 - <<'PY'
from pathlib import Path
import re

path = Path("src/hardware/mpp/MppEncoder.cpp")
text = path.read_text()

if "mpp_enc_cfg_set_s32" in text and "MPP_ENC_SET_CFG" in text and "use MppEncCfg" in text:
    print("[SKIP] MppEncCfg block already exists")
    raise SystemExit(0)

if "#include <rockchip/rk_venc_cfg.h>" not in text:
    if "#include <rockchip/mpp_packet.h>" in text:
        text = text.replace(
            "#include <rockchip/mpp_packet.h>\n",
            "#include <rockchip/mpp_packet.h>\n#include <rockchip/rk_venc_cfg.h>\n",
            1
        )
    elif "#include <rockchip/rk_mpi.h>" in text:
        text = text.replace(
            "#include <rockchip/rk_mpi.h>\n",
            "#include <rockchip/rk_mpi.h>\n#include <rockchip/rk_venc_cfg.h>\n",
            1
        )
    else:
        text = "#include <rockchip/rk_venc_cfg.h>\n" + text

if "#include <iostream>" not in text:
    if "#include <cstring>" in text:
        text = text.replace("#include <cstring>\n", "#include <cstring>\n#include <iostream>\n", 1)
    else:
        text = "#include <iostream>\n" + text

# 兼容 MppEncPrepCfg prep{} 和 ::MppEncPrepCfg prep{}
m = re.search(r'(?m)^[ \t]*(?:::)?MppEncPrepCfg[ \t]+prep\{\};', text)
if not m:
    raise SystemExit("[ERROR] cannot find MppEncPrepCfg prep{} block")

start = m.start()

# 结束点：保留 ctx_ = ctx; 以及后面的成员赋值
m2 = re.search(r'(?m)^[ \t]*ctx_[ \t]*=[ \t]*ctx;', text[start:])
if not m2:
    raise SystemExit("[ERROR] cannot find ctx_ = ctx; after MPP config block")

end = start + m2.start()

new_block = r'''    const std::uint32_t fps = config.fps ? config.fps : 30;
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

'''

text = text[:start] + new_block + text[end:]
path.write_text(text)

print("[OK] replaced deprecated PREP/RC/CODEC config block with MppEncCfg + MPP_ENC_SET_CFG")
PY

echo
echo "[CHECK]"
grep -n "rk_venc_cfg\|use MppEncCfg\|mpp_enc_cfg_set_s32\|MPP_ENC_SET_CFG\|MppEncPrepCfg\|MPP_ENC_SET_RC_CFG" "$FILE" | head -120

echo
echo "[DONE]"
