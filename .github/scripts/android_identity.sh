#!/usr/bin/env bash
# Map a known branch to a stable Android identity, then check the built APK.
#
# A branch name is not an application id and not a display name. Hyphens and
# capitals are illegal in a package segment, and a raw branch string is a bad
# launcher label. Only the explicit map below is applied. An unmapped branch
# fails the build instead of inventing a suffix.
#
# The Godot Android preset already owns both values:
#   package/unique_name  -> manifest package (application id)
#   package/name         -> launcher label (godot-project-name string)
# This script rewrites those two lines. It does not add another name resource.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
preset="${root}/godot/export_presets.cfg"

identity_for() {
  case "$1" in
    ScraperX-Grok)
      printf '%s\n' "ScraperX-Grok" "com.cory.scraperx.grok"
      ;;
    Gemini)
      printf '%s\n' "ScraperX-Gemini" "com.cory.scraperx.gemini"
      ;;
    ScraperX-Claude)
      printf '%s\n' "ScraperX-Claude" "com.cory.scraperx.claude"
      ;;
    ChatGPT)
      printf '%s\n' "ScraperX-ChatGPT" "com.cory.scraperx.chatgpt"
      ;;
    *)
      echo "No Android identity is mapped for branch '${1}'." >&2
      echo "Add an explicit label and application id. Do not derive one from the branch name." >&2
      return 1
      ;;
  esac
}

apply_identity() {
  local branch="$1"
  local label application_id mapped
  mapped="$(mktemp)"
  identity_for "${branch}" > "${mapped}"
  label="$(sed -n '1p' "${mapped}")"
  application_id="$(sed -n '2p' "${mapped}")"
  rm -f "${mapped}"
  LABEL="${label}" APPLICATION_ID="${application_id}" PRESET="${preset}" python3 - <<'PY'
import os
import pathlib
import re
import sys

preset = pathlib.Path(os.environ["PRESET"])
text = preset.read_text()
label = os.environ["LABEL"]
application_id = os.environ["APPLICATION_ID"]
if not re.fullmatch(r"[a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*)+", application_id):
    sys.exit(f"refusing application id {application_id!r}")
updated, name_count = re.subn(
    r'^package/unique_name="[^"]*"\s*$',
    f'package/unique_name="{application_id}"',
    text,
    count=1,
    flags=re.M,
)
updated, label_count = re.subn(
    r'^package/name="[^"]*"\s*$',
    f'package/name="{label}"',
    updated,
    count=1,
    flags=re.M,
)
if name_count != 1 or label_count != 1:
    sys.exit(
        f"expected one package/unique_name and one package/name, "
        f"rewrote {name_count} and {label_count}"
    )
preset.write_text(updated)
PY
  printf 'ANDROID_LABEL=%s\nANDROID_APPLICATION_ID=%s\n' "${label}" "${application_id}"
}

verify_apk() {
  local apk="$1"
  local application_id="$2"
  local label="$3"
  local aapt badging
  if [[ ! -s "${apk}" ]]; then
    echo "APK is missing: ${apk}" >&2
    return 1
  fi
  aapt="$(find "${ANDROID_HOME}/build-tools" -type f -name aapt | sort -V | tail -n 1)"
  if [[ -z "${aapt}" ]]; then
    echo "aapt was not found under ${ANDROID_HOME}/build-tools" >&2
    return 1
  fi
  badging="$(mktemp)"
  "${aapt}" dump badging "${apk}" > "${badging}"
  if ! grep -F "package: name='${application_id}'" "${badging}" >/dev/null; then
    echo "APK application id is not ${application_id}" >&2
    grep -F "package: name=" "${badging}" >&2 || true
    return 1
  fi
  if ! grep -F "application-label:'${label}'" "${badging}" >/dev/null; then
    echo "APK launcher label is not ${label}" >&2
    grep -F "application-label" "${badging}" >&2 || true
    return 1
  fi
  grep -F "package: name='${application_id}'" "${badging}"
  grep -F "application-label:'${label}'" "${badging}"
}

case "${1:-}" in
  apply)
    apply_identity "${2:-${GITHUB_REF_NAME:-}}"
    ;;
  verify)
    verify_apk "${2:?apk}" "${3:?application id}" "${4:?label}"
    ;;
  *)
    echo "usage: android_identity.sh apply [branch] | verify <apk> <application-id> <label>" >&2
    exit 2
    ;;
esac
