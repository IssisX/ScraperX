#!/usr/bin/env bash
# Called by the emulator action after SDK installation, before Android boots.
set -euo pipefail
task_output="${1:?Expected emulator evidence output directory}"
mkdir -p "${task_output}"
"${ANDROID_HOME}/emulator/emulator" -accel-check | tee "${task_output}/acceleration.txt"
AVDMANAGER_PATH="$(find "${ANDROID_HOME}/cmdline-tools" -type f -path '*/bin/avdmanager' | sort -V | tail -n 1)"
test -x "${AVDMANAGER_PATH}"
"${AVDMANAGER_PATH}" list device > "${task_output}/available-profiles.txt"
