#!/usr/bin/env bash
set -e

FILE="tests/l8_udp_yuyv_mainstream_probe.cpp"

if [ ! -f "$FILE" ]; then
    echo "[ERROR] missing file: $FILE"
    exit 1
fi

TS="$(date +%Y%m%d_%H%M%S)"
BACKUP="${FILE}.bak_bind_mpp_encoder_${TS}"

cp "$FILE" "$BACKUP"
echo "[BACKUP] $FILE -> $BACKUP"

python3 - <<'PY'
from pathlib import Path

path = Path("tests/l8_udp_yuyv_mainstream_probe.cpp")
text = path.read_text()

# 1. Add MPP encoder include.
include_line = '#include "hardware/mpp/MppEncoder.h"\n'

if include_line not in text:
    lines = text.splitlines(True)
    insert_idx = 0

    # Insert after existing hardware includes if possible,
    # otherwise after foundation includes.
    for i, line in enumerate(lines):
        if line.startswith('#include "hardware/'):
            insert_idx = i + 1

    if insert_idx == 0:
        for i, line in enumerate(lines):
            if line.startswith('#include "foundation/'):
                insert_idx = i + 1

    lines.insert(insert_idx, include_line)
    text = ''.join(lines)
    print("[PATCH] added hardware/mpp/MppEncoder.h include")
else:
    print("[OK] MppEncoder include already exists")

# 2. Add concrete MPP encoder object before ModeRuntimeContext.
needle = "tri::mode::ModeRuntimeContext modeRuntime;"
if needle not in text:
    raise SystemExit("[ERROR] cannot find ModeRuntimeContext declaration")

object_line = "    tri::hardware::mpp::MppEncoder mppEncoder;\n\n"

if "tri::hardware::mpp::MppEncoder mppEncoder;" not in text:
    text = text.replace(
        "    " + needle,
        object_line + "    " + needle,
        1
    )
    print("[PATCH] added mppEncoder object")
else:
    print("[OK] mppEncoder object already exists")

# 3. Bind runtime.encoder to mppEncoder.
old = "modeRuntime.encoder = nullptr;"
new = "modeRuntime.encoder = &mppEncoder;"

if old in text:
    text = text.replace(old, new, 1)
    print("[PATCH] modeRuntime.encoder = &mppEncoder")
elif new in text:
    print("[OK] modeRuntime.encoder already bound to mppEncoder")
else:
    raise SystemExit("[ERROR] cannot find modeRuntime.encoder assignment")

path.write_text(text)
PY

echo
echo "[CHECK] result:"
grep -n "MppEncoder.h\|MppEncoder mppEncoder\|modeRuntime.encoder" "$FILE"

echo
echo "[DONE] l8 udp probe now binds MPP encoder."
