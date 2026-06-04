#!/usr/bin/env bash
set -e

FILE="src/hardware/mpp/MppEncoder.cpp"

if [ ! -f "$FILE" ]; then
    echo "[ERROR] missing file: $FILE"
    exit 1
fi

TS="$(date +%Y%m%d_%H%M%S)"
BACKUP="${FILE}.bak_fix_print_cerr_${TS}"
cp "$FILE" "$BACKUP"
echo "[BACKUP] $FILE -> $BACKUP"

python3 - <<'PY'
from pathlib import Path
import re

path = Path("src/hardware/mpp/MppEncoder.cpp")
text = path.read_text()

# 确保有 iostream
if "#include <iostream>" not in text:
    # 优先插到 include 区域后面
    text = text.replace("#include <cstring>\n", "#include <cstring>\n#include <iostream>\n", 1) \
        if "#include <cstring>\n" in text else "#include <iostream>\n" + text

pattern = re.compile(
    r'''TRI_LOG_INFO\(tri::foundation::LogCategory::System\)\s*
\s*<< "MPP encoder init config:"\s*
\s*<< " width=" << config\.width\s*
\s*<< " height=" << config\.height\s*
\s*<< " horStride=" << config\.horStride\s*
\s*<< " verStride=" << config\.verStride\s*
\s*<< " fps=" << config\.fps\s*
\s*<< " gop=" << config\.gop\s*
\s*<< " bitrateKbps=" << config\.bitrateKbps;''',
    re.MULTILINE
)

replacement = '''std::cerr
        << "[MPP] encoder init config:"
        << " width=" << config.width
        << " height=" << config.height
        << " horStride=" << config.horStride
        << " verStride=" << config.verStride
        << " fps=" << config.fps
        << " gop=" << config.gop
        << " bitrateKbps=" << config.bitrateKbps
        << std::endl;'''

new_text, n = pattern.subn(replacement, text, count=1)

if n == 0:
    raise SystemExit("[ERROR] cannot find TRI_LOG_INFO MPP config print block")

path.write_text(new_text)
print("[OK] replaced TRI_LOG_INFO config print with std::cerr")
PY

echo
echo "[CHECK]"
grep -n "MPP.*encoder init config\|std::cerr\|#include <iostream>" "$FILE"

echo
echo "[DONE]"
