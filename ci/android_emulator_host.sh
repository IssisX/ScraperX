#!/usr/bin/env bash
# Called by the emulator action after SDK installation, before Android boots.
set -euo pipefail
task_output="${1:?Expected emulator evidence output directory}"
mkdir -p "${task_output}"
python3 - "${task_output}/host-resources.json" <<'PY'
import json, os, sys
from pathlib import Path
record = {'logical_cpus': os.cpu_count(), 'allowed_cpus': sorted(os.sched_getaffinity(0))}
for path in ('/proc/meminfo', '/sys/fs/cgroup/cpu.max', '/sys/fs/cgroup/memory.max'):
    source = Path(path)
    if source.exists():
        record[path] = source.read_text()
Path(sys.argv[1]).write_text(json.dumps(record, indent=2) + '\n')
PY
"${ANDROID_HOME}/emulator/emulator" -accel-check | tee "${task_output}/acceleration.txt"
AVDMANAGER_PATH="$(find "${ANDROID_HOME}/cmdline-tools" -type f -path '*/bin/avdmanager' | sort -V | tail -n 1)"
test -x "${AVDMANAGER_PATH}"
"${AVDMANAGER_PATH}" list device > "${task_output}/available-profiles.txt"
cp "${ANDROID_AVD_HOME}/test.avd/config.ini" "${task_output}/avd-config.ini"
if [[ "${2:-}" == '7.6in Foldable' ]]; then
  python3 - "${task_output}/avd-config.ini" <<'PY'
import sys
from pathlib import Path
config = dict(line.strip().split('=', 1) for line in Path(sys.argv[1]).read_text().splitlines() if '=' in line)
expected = {'hw.sensor.hinge.count': '1', 'hw.displayRegion.0.1.width': '884',
            'hw.sensor.hinge.ranges': '0-180'}
for key, value in expected.items():
    if config.get(key) != value:
        raise SystemExit(f'Foldable AVD expected {key}={value}; observed {config.get(key)!r}')
if config.get('hw.sensor.hinge') not in ('yes', 'true'):
    raise SystemExit('Foldable AVD must provide an actual hinge sensor')
PY
fi
