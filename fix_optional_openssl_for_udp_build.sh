#!/usr/bin/env bash
set -e

FILE="CMakeLists.txt"
TS="$(date +%Y%m%d_%H%M%S)"
BACKUP="${FILE}.bak_optional_openssl_${TS}"

cp "$FILE" "$BACKUP"
echo "[BACKUP] $FILE -> $BACKUP"

python3 - <<'PY'
from pathlib import Path
import re

path = Path("CMakeLists.txt")
text = path.read_text()

# 1. Make OpenSSL optional.
text = text.replace(
    "find_package(OpenSSL REQUIRED)",
    '''find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
else()
    message(WARNING "OpenSSL not found in cross sysroot; GB28181/RTSP OpenSSL-dependent targets will be skipped.")
endif()'''
)

# 2. Remove OpenSSL::Crypto from the whole file.
#    This prevents non-OpenSSL targets from failing configuration/link checks.
text = text.replace("            OpenSSL::Crypto\n", "")
text = text.replace("        OpenSSL::Crypto\n", "")
text = text.replace("    OpenSSL::Crypto\n", "")

# 3. Disable common OpenSSL-dependent executable targets if present.
#    We comment out the entire add_executable + target_link_libraries block
#    for the three targets that previously linked OpenSSL::Crypto.
openssl_targets = [
    "l8_gb28181_register_probe",
    "l8_gb28181_play_probe",
    "tri_fusion_rtsp_main",
]

for target in openssl_targets:
    # Match:
    # add_executable(target ...)
    # target_link_libraries(target ... )
    pattern = re.compile(
        r'(\n\s*add_executable\s*\(\s*' + re.escape(target) + r'\b.*?\)\s*'
        r'\n\s*target_link_libraries\s*\(\s*' + re.escape(target) + r'\b.*?\)\s*)',
        re.S
    )

    def repl(m, target=target):
        block = m.group(1)
        commented = "\n# [disabled for UDP/RGA/MPP cross build: OpenSSL not required]\n"
        commented += "\n".join("# " + line if line.strip() else "#" for line in block.splitlines())
        commented += "\n"
        return commented

    text, n = pattern.subn(repl, text, count=1)
    print(f"[PATCH] disable {target}: {n}")

path.write_text(text)
PY

echo
echo "[CHECK] OpenSSL lines:"
grep -n "OpenSSL\|OPENSSL\|OpenSSL::Crypto" CMakeLists.txt || true

echo
echo "[DONE] CMakeLists.txt patched for optional OpenSSL."
