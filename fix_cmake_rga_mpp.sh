#!/usr/bin/env bash
set -e

FILE="CMakeLists.txt"
TS="$(date +%Y%m%d_%H%M%S)"
BACKUP="${FILE}.bak_rga_mpp_${TS}"

cp "$FILE" "$BACKUP"
echo "[BACKUP] $FILE -> $BACKUP"

python3 - <<'PY'
from pathlib import Path
import re

path = Path("CMakeLists.txt")
text = path.read_text()

# Replace tri_fusion_hardware include dirs.
pattern_inc = re.compile(
    r'target_include_directories\s*\(\s*tri_fusion_hardware\s+PUBLIC\s+.*?\)',
    re.S
)

replacement_inc = '''target_include_directories(tri_fusion_hardware PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SYSROOT}/usr/include
    ${CMAKE_SYSROOT}/usr/include/rga
    ${CMAKE_SYSROOT}/usr/include/rockchip
)'''

text, n_inc = pattern_inc.subn(replacement_inc, text, count=1)

if n_inc != 1:
    raise SystemExit("[ERROR] failed to replace target_include_directories(tri_fusion_hardware ...)")

# Replace tri_fusion_hardware link libs.
pattern_link = re.compile(
    r'target_link_libraries\s*\(\s*tri_fusion_hardware\s+PUBLIC\s+.*?\)',
    re.S
)

replacement_link = '''target_link_libraries(tri_fusion_hardware PUBLIC
    pthread
    rga
    rockchip_mpp
)'''

text, n_link = pattern_link.subn(replacement_link, text, count=1)

if n_link != 1:
    raise SystemExit("[ERROR] failed to replace target_link_libraries(tri_fusion_hardware ...)")

path.write_text(text)
print("[OK] CMakeLists.txt updated for RGA / MPP")
PY

echo
echo "[CHECK] tri_fusion_hardware cmake blocks:"
grep -n -A8 -B2 "target_include_directories(tri_fusion_hardware" CMakeLists.txt
echo
grep -n -A8 -B2 "target_link_libraries(tri_fusion_hardware" CMakeLists.txt
