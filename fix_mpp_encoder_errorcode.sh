#!/usr/bin/env bash
set -e

FILE="src/hardware/mpp/MppEncoder.cpp"

if [ ! -f "$FILE" ]; then
    echo "[ERROR] file not found: $FILE"
    exit 1
fi

TS="$(date +%Y%m%d_%H%M%S)"
BACKUP="${FILE}.bak_${TS}"

cp "$FILE" "$BACKUP"
echo "[BACKUP] $FILE -> $BACKUP"

echo "[PATCH] replace ErrorCode::HardwareError -> ErrorCode::IoError"
sed -i 's/ErrorCode::HardwareError/ErrorCode::IoError/g' "$FILE"

echo "[PATCH] replace ErrorCode::InvalidState -> ErrorCode::NotInitialized"
sed -i 's/ErrorCode::InvalidState/ErrorCode::NotInitialized/g' "$FILE"

echo
echo "[CHECK] remaining unsupported error codes:"
grep -n "ErrorCode::HardwareError\|ErrorCode::InvalidState" "$FILE" || echo "[OK] no HardwareError / InvalidState remains"

echo
echo "[CHECK] modified MPP encoder error returns:"
grep -n "mpp_create encoder failed\|mpp_init encoder failed\|MPP_ENC_SET_PREP_CFG failed\|MPP_ENC_SET_RC_CFG failed\|MPP_ENC_SET_CODEC_CFG failed\|MPP encoder not initialized" "$FILE" || true

echo
echo "[DONE] MppEncoder.cpp error code patch finished."
