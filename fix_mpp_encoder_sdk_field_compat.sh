#!/usr/bin/env bash
set -e

FILE="src/hardware/mpp/MppEncoder.cpp"

if [ ! -f "$FILE" ]; then
    echo "[ERROR] missing file: $FILE"
    exit 1
fi

TS="$(date +%Y%m%d_%H%M%S)"
BACKUP="${FILE}.bak_sdk_field_compat_${TS}"

cp "$FILE" "$BACKUP"
echo "[BACKUP] $FILE -> $BACKUP"

echo "[PATCH] remove unsupported MPP SDK fields"

# 当前 RK3588 SDK 的 MppEncRcCfg 没有 drop_thd
sed -i '/rc\.drop_thd[[:space:]]*=/d' "$FILE"

# 当前 RK3588 SDK 的 MppEncH264Cfg 没有 qp_ip
sed -i '/codec\.h264\.qp_ip[[:space:]]*=/d' "$FILE"

echo
echo "[CHECK] remaining suspicious fields:"
grep -n "drop_thd\|qp_ip" "$FILE" || echo "[OK] drop_thd / qp_ip removed"

echo
echo "[CHECK] current RC/QP config:"
grep -n "drop_mode\|drop_gap\|qp_init\|qp_max\|qp_min\|bps_target\|bps_max\|bps_min" "$FILE" || true

echo
echo "[DONE] MppEncoder.cpp SDK field compatibility patch finished."
