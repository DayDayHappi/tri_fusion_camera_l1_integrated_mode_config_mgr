#!/usr/bin/env bash
set -e

FILE="src/mode/ModeSwitchExecutor.cpp"

if [ ! -f "$FILE" ]; then
    echo "[ERROR] missing file: $FILE"
    exit 1
fi

TS="$(date +%Y%m%d_%H%M%S)"
BACKUP="${FILE}.bak_skip_sensor_switch_${TS}"

cp "$FILE" "$BACKUP"
echo "[BACKUP] $FILE -> $BACKUP"

python3 - <<'PY'
from pathlib import Path

path = Path("src/mode/ModeSwitchExecutor.cpp")
text = path.read_text()

if '#include <cstdlib>' not in text:
    text = text.replace(
        '#include "media/pipeline/PipelineBuilder.h"\n',
        '#include "media/pipeline/PipelineBuilder.h"\n\n#include <cstdlib>\n'
    )

old = '''Result<void> ModeSwitchExecutor::switchCompositeSensor(const ModeSwitchPlan& plan) {
    if (!plan.requireCompositeSensorSwitch) return Result<void>::success();

    TRI_LOG_INFO(LogCategory::System) << "switch composite sensor output to "
                                      << device_control::toString(plan.compositeOutput);

    auto ret = context_.compositeController->setOutputMode(plan.compositeOutput);
    if (!ret) return ret;

    return context_.compositeController->waitStable(plan.stableWaitMs);
}'''

new = '''Result<void> ModeSwitchExecutor::switchCompositeSensor(const ModeSwitchPlan& plan) {
    if (!plan.requireCompositeSensorSwitch) return Result<void>::success();

    const char* skipSensorSwitch = std::getenv("TRI_FUSION_SKIP_SENSOR_SWITCH");
    if (skipSensorSwitch != nullptr && skipSensorSwitch[0] == '1') {
        TRI_LOG_INFO(LogCategory::System)
            << "skip composite sensor output switch by env TRI_FUSION_SKIP_SENSOR_SWITCH=1, target="
            << device_control::toString(plan.compositeOutput);
        return Result<void>::success();
    }

    TRI_LOG_INFO(LogCategory::System) << "switch composite sensor output to "
                                      << device_control::toString(plan.compositeOutput);

    auto ret = context_.compositeController->setOutputMode(plan.compositeOutput);
    if (!ret) return ret;

    return context_.compositeController->waitStable(plan.stableWaitMs);
}'''

if old not in text:
    raise SystemExit("[ERROR] target switchCompositeSensor block not found. Please grep current function body.")

text = text.replace(old, new)
path.write_text(text)
print("[OK] ModeSwitchExecutor.cpp patched")
PY

echo
echo "[CHECK] patch result:"
grep -n -A25 -B5 "TRI_FUSION_SKIP_SENSOR_SWITCH" "$FILE"

echo
echo "[DONE] skip sensor switch patch finished."
