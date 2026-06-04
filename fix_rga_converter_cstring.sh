#!/usr/bin/env bash
set -e

FILE="src/hardware/rga/RgaConverter.cpp"

if [ ! -f "$FILE" ]; then
    echo "[ERROR] missing file: $FILE"
    exit 1
fi

TS="$(date +%Y%m%d_%H%M%S)"
BACKUP="${FILE}.bak_add_cstring_${TS}"

cp "$FILE" "$BACKUP"
echo "[BACKUP] $FILE -> $BACKUP"

if grep -q '#include <cstring>' "$FILE"; then
    echo "[OK] <cstring> already included"
else
    # 在 #include <string> 后面插入 #include <cstring>
    sed -i '/#include <string>/a #include <cstring>' "$FILE"
    echo "[PATCH] added #include <cstring>"
fi

echo
echo "[CHECK] includes:"
grep -n "#include" "$FILE" | head -20

echo
echo "[DONE] RgaConverter.cpp patched."
