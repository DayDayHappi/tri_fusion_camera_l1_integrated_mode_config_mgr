#!/usr/bin/env bash
set -e

FILE="src/media/process/FormatConvertNode.cpp"

if [ ! -f "$FILE" ]; then
    echo "[ERROR] missing file: $FILE"
    exit 1
fi

TS="$(date +%Y%m%d_%H%M%S)"
BACKUP="${FILE}.bak_rga_include_${TS}"

cp "$FILE" "$BACKUP"
echo "[BACKUP] $FILE -> $BACKUP"

add_include_after_main() {
    local include_line="$1"

    if grep -qF "$include_line" "$FILE"; then
        echo "[OK] already has: $include_line"
    else
        sed -i "/#include \"media\/process\/FormatConvertNode.h\"/a ${include_line}" "$FILE"
        echo "[PATCH] added: $include_line"
    fi
}

add_include_after_main '#include "hardware/rga/RgaConverter.h"'
add_include_after_main '#include "hardware/rga/RgaTypes.h"'

echo
echo "[CHECK] includes:"
grep -n "#include" "$FILE" | head -30

echo
echo "[DONE] FormatConvertNode.cpp RGA include patch finished."
