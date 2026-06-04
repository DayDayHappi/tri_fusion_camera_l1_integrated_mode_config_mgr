#!/usr/bin/env bash
set -e

FILE="src/hardware/mpp/MppEncoder.cpp"

if [ ! -f "$FILE" ]; then
    echo "[ERROR] missing file: $FILE"
    exit 1
fi

TS="$(date +%Y%m%d_%H%M%S)"
BACKUP="${FILE}.bak_print_config_${TS}"
cp "$FILE" "$BACKUP"
echo "[BACKUP] $FILE -> $BACKUP"

python3 - <<'PY'
from pathlib import Path

path = Path("src/hardware/mpp/MppEncoder.cpp")
text = path.read_text()

needle = '''config_ = config;'''
insert = '''config_ = config;

    TRI_LOG_INFO(tri::foundation::LogCategory::System)
        << "MPP encoder init config:"
        << " width=" << config.width
        << " height=" << config.height
        << " horStride=" << config.horStride
        << " verStride=" << config.verStride
        << " fps=" << config.fps
        << " gop=" << config.gop
        << " bitrateKbps=" << config.bitrateKbps;'''

if insert in text:
    print("[SKIP] config print already exists")
elif needle in text:
    text = text.replace(needle, insert, 1)
else:
    raise SystemExit("[ERROR] cannot find 'config_ = config;' in MppEncoder.cpp")

path.write_text(text)
print("[OK] MPP config print patched")
PY

grep -n "MPP encoder init config" "$FILE"
