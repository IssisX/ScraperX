#!/usr/bin/env bash
# Installs the Linux x86_64 Godot editor binary that CI pins at tools/godot/godot,
# verified by SHA-256. Idempotent; concurrent callers (the SessionStart hook and
# the MCP launcher) serialize on a lock. Writes nothing to stdout.
set -euo pipefail
exec 1>&2

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_root}"

# The workflow is the single source for the Godot pin; a second copy here would drift.
workflow=.github/workflows/wo000-delivery-spine.yml
ci_env() {
  local value
  value="$(sed -n -E "s/^  $1: ([^[:space:]]+)\$/\1/p" "${workflow}")"
  if [[ -z "${value}" || "${value}" == *$'\n'* ]]; then
    echo "install-godot: expected exactly one top-level env '$1' in ${workflow}" >&2
    exit 1
  fi
  printf '%s' "${value}"
}
godot_version="$(ci_env GODOT_VERSION)"
godot_sha256="$(ci_env GODOT_BINARY_SHA256)"

godot_dir=tools/godot
godot_bin="${godot_dir}/godot"
godot_stamp="${godot_dir}/godot.zip.sha256"
mkdir -p "${godot_dir}"

exec {lock_fd}> "${godot_dir}/.install.lock"
flock "${lock_fd}"

if [[ -x "${godot_bin}" && "$(cat "${godot_stamp}" 2>/dev/null)" == "${godot_sha256}" ]]; then
  exit 0
fi
# A binary put here by the workflow's own steps has no stamp; accept it when it
# reports the pinned release, as the workflow's own check does.
if [[ -x "${godot_bin}" && "$("${godot_bin}" --headless --version 2>/dev/null)" == "${godot_version/-/.}.official."* ]]; then
  exit 0
fi

staging="$(mktemp -d "${godot_dir}/.staging.XXXXXX")"
trap 'rm -rf -- "${staging}"' EXIT
curl --fail --location --retry 3 --silent --show-error \
  --output "${staging}/godot.zip" \
  "https://github.com/godotengine/godot/releases/download/${godot_version}/Godot_v${godot_version}_linux.x86_64.zip"
echo "${godot_sha256}  ${staging}/godot.zip" | sha256sum --check --quiet
unzip -q "${staging}/godot.zip" -d "${staging}"
chmod +x "${staging}/Godot_v${godot_version}_linux.x86_64"
mv -f "${staging}/Godot_v${godot_version}_linux.x86_64" "${godot_bin}"
printf '%s\n' "${godot_sha256}" > "${godot_stamp}"
