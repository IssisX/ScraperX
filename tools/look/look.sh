#!/usr/bin/env bash
# The look suite: stills of a character in her key moments from fixed cameras
# about her body, rendered under xvfb at a small size (README.md here).
#
#   tools/look/look.sh [matron|moves|hold|fatal] [--out DIR] [--size WxH] [--serial]
#
# Writes DIR/<set>_<shot>.png, DIR/<set>_index.tsv, DIR/<set>.log, and
# DIR/index.md over them all. The sets of "matron" run side by side unless
# --serial.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SHOTS="matron"
OUT="${ROOT}/build/look"
SIZE="640x552"
SERIAL=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --out) OUT="$2"; shift 2 ;;
    --size) SIZE="$2"; shift 2 ;;
    --serial) SERIAL=1; shift ;;
    -h|--help) sed -n 2,9p "${BASH_SOURCE[0]}"; exit 0 ;;
    *) SHOTS="$1"; shift ;;
  esac
done
case "${SHOTS}" in
  matron) SETS=(moves hold fatal) ;;
  moves|hold|fatal) SETS=("${SHOTS}") ;;
  *) echo "look.sh: no shot set '${SHOTS}' (matron, moves, hold, fatal)" >&2; exit 2 ;;
esac

# Godot: $GODOT, this tree's tools/godot/godot, or the main checkout's.
GODOT="${GODOT:-}"
if [[ -z "${GODOT}" ]]; then
  for candidate in "${ROOT}/tools/godot/godot" \
      "$(cd "${ROOT}" && cd "$(git rev-parse --git-common-dir)/.." 2>/dev/null && pwd)/tools/godot/godot"; do
    if [[ -x "${candidate}" ]]; then GODOT="${candidate}"; break; fi
  done
fi
[[ -x "${GODOT}" ]] || { echo "look.sh: no Godot (set GODOT or put it at tools/godot/godot)" >&2; exit 2; }
LIB="${ROOT}/godot/addons/scraperx_native/bin/linux/libscraperx_native.so"
[[ -f "${LIB}" ]] || { echo "look.sh: no ${LIB}: build scraperx_native (README.md)" >&2; exit 2; }
mkdir -p "${OUT}"
# As CI makes it.
mkdir -p "${ROOT}/godot/.godot"
printf '%s\n' 'res://addons/scraperx_native/scraperx_native.gdextension' > "${ROOT}/godot/.godot/extension_list.cfg"

W="${SIZE%x*}"; H="${SIZE#*x}"
run_set() {
  local set="$1" start end
  rm -f "${OUT}/${set}"_*.png "${OUT}/${set}_index.tsv"
  start=$(date +%s.%N)
  timeout 900s xvfb-run -a -s "-screen 0 $((W + 64))x$((H + 64))x24" \
    "${GODOT}" --path "${ROOT}/godot" --rendering-method gl_compatibility --audio-driver Dummy \
    --resolution "${SIZE}" --fixed-fps 60 \
    -- "--uitest=look_matron_${set}" "--capture=${OUT}/${set}" >"${OUT}/${set}.log" 2>&1
  end=$(date +%s.%N)
  printf '%.1f\n' "$(echo "${end} - ${start}" | bc)" >"${OUT}/${set}.wall"
}

T0=$(date +%s.%N)
if [[ "${SERIAL}" == 1 || ${#SETS[@]} == 1 ]]; then
  for set in "${SETS[@]}"; do run_set "${set}"; done
else
  for set in "${SETS[@]}"; do run_set "${set}" & done
  wait
fi
TOTAL=$(printf '%.1f' "$(echo "$(date +%s.%N) - ${T0}" | bc)")

# The index over every set: each still, its sim time, her state, what it is
# meant to show, the wall seconds it took (the first of a set counts Godot's
# start), and what was measured of her.
STATUS=0
{
  echo "# Look suite: ${SHOTS} ($(TZ=America/Chicago date '+%Y-%m-%d %-I:%M %p Central'), ${SIZE}, $(git -C "${ROOT}" rev-parse --short HEAD))"
  echo
  echo "Total wall: ${TOTAL} s."
  for set in "${SETS[@]}"; do
    echo
    result=$(grep -m1 -E '^SCRAPERX_UITEST (PASS|FAIL)' "${OUT}/${set}.log" || echo "SCRAPERX_UITEST FAIL look_matron_${set} reason=no result line (see ${set}.log)")
    [[ "${result}" == *" PASS "* ]] || STATUS=1
    echo "## ${set}: $(cat "${OUT}/${set}.wall" 2>/dev/null || echo '?') s wall, ${result#SCRAPERX_UITEST }"
    echo
    echo "| shot | sim s | state | stage | shows | wall s | file | measured |"
    echo "|---|---|---|---|---|---|---|---|"
    if [[ -f "${OUT}/${set}_index.tsv" ]]; then
      awk -F'\t' '{printf "| %s | %s | %s | %s | %s | %s | %s | %s |\n", $1, $2, $3, $4, $5, $6, $7, $8}' "${OUT}/${set}_index.tsv"
    fi
    grep -E "SCRIPT ERROR|Parse Error" "${OUT}/${set}.log" | head -5 | sed 's/^/    /'
  done
} >"${OUT}/index.md"
cat "${OUT}/index.md"
exit "${STATUS}"
