#!/usr/bin/env bash
set -e

FILE="src/protocol/udp/UdpMpegTsPublisher.cpp"

if [ ! -f "$FILE" ]; then
    echo "[ERROR] missing file: $FILE"
    exit 1
fi

TS="$(date +%Y%m%d_%H%M%S)"
BACKUP="${FILE}.bak_ffmpeg_diag_${TS}"
cp "$FILE" "$BACKUP"
echo "[BACKUP] $FILE -> $BACKUP"

python3 - <<'PY'
from pathlib import Path

path = Path("src/protocol/udp/UdpMpegTsPublisher.cpp")
text = path.read_text()

# Add includes.
for inc in [
    "#include <fcntl.h>\n",
    "#include <sstream>\n",
]:
    if inc not in text:
        marker = "#include <iostream>\n"
        if marker in text:
            text = text.replace(marker, marker + inc, 1)
        else:
            text = text.replace("#include <cstring>\n", "#include <cstring>\n" + inc, 1)

# Insert command logging before execvp.
old = '''        const auto args = buildFfmpegArgs();

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);

        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        ::execvp(argv[0], argv.data());'''

new = '''        const auto args = buildFfmpegArgs();

        // Redirect ffmpeg stderr to a file so we can diagnose why ffmpeg exits.
        int logFd = ::open("/tmp/tri_fusion_ffmpeg_stderr.log",
                           O_CREAT | O_WRONLY | O_TRUNC,
                           0644);
        if (logFd >= 0) {
            ::dup2(logFd, STDERR_FILENO);
            ::close(logFd);
        }

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);

        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        ::execvp(argv[0], argv.data());'''

if old not in text:
    raise SystemExit("[ERROR] cannot find execvp block")

text = text.replace(old, new, 1)

# Add parent-side ffmpeg command log after pipe close block.
old2 = '''    ::close(pipefd[0]);
    writeFd_ = pipefd[1];

    return Result<void>::success();'''

new2 = '''    ::close(pipefd[0]);
    writeFd_ = pipefd[1];

    {
        const auto args = buildFfmpegArgs();
        std::ostringstream oss;
        for (const auto& arg : args) {
            if (!oss.str().empty()) {
                oss << " ";
            }
            oss << arg;
        }
        TRI_LOG_INFO(LogCategory::System)
            << "ffmpeg command: " << oss.str()
            << ", stderr=/tmp/tri_fusion_ffmpeg_stderr.log";
    }

    return Result<void>::success();'''

if old2 not in text:
    raise SystemExit("[ERROR] cannot find parent pipe block")

text = text.replace(old2, new2, 1)

path.write_text(text)
print("[OK] ffmpeg diagnostic logging patched")
PY

echo
echo "[CHECK]"
grep -n "ffmpeg command\|tri_fusion_ffmpeg_stderr\|#include <fcntl.h>\|#include <sstream>" "$FILE"
