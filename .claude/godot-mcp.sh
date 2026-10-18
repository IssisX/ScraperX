#!/usr/bin/env bash
# Starts the godot-mcp-runtime MCP server (the "godot" entry in .mcp.json) for
# the Godot project under godot/. stdout is the MCP JSON-RPC channel: nothing in
# this script may write to it.
set -euo pipefail

SERVER_PACKAGE="godot-mcp-runtime@3.8.1"

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

# The server probes for Godot once at startup and caches a miss for its whole
# lifetime, and nothing orders MCP startup after the SessionStart hook. In a
# cloud session, install (or wait on the hook's install of) the pinned binary
# before the server can look for it.
if [[ "${CLAUDE_CODE_REMOTE:-}" == "true" && -z "${GODOT_PATH:-}" ]]; then
  bash "${repo_root}/.claude/install-godot.sh"
fi

# An explicit GODOT_PATH wins; otherwise the SHA-pinned binary that CI and
# .claude/install-godot.sh put under tools/godot; otherwise the server's own
# PATH auto-detection.
if [[ -z "${GODOT_PATH:-}" && -x "${repo_root}/tools/godot/godot" ]]; then
  export GODOT_PATH="${repo_root}/tools/godot/godot"
fi

# run_project refuses to start without DISPLAY or WAYLAND_DISPLAY. A headless
# Linux host gets a private Xvfb at the window override size in project.godot.
# xvfb-run is not used because it merges the child's stderr into stdout.
if [[ "$(uname -s)" != Linux || -n "${DISPLAY:-}" || -n "${WAYLAND_DISPLAY:-}" ]] ||
  ! command -v Xvfb >/dev/null 2>&1; then
  exec npx --yes "${SERVER_PACKAGE}"
fi

xvfb_pid=""
server_pid=""
cleanup() {
  if [[ -n "${xvfb_pid}" ]]; then
    kill "${xvfb_pid}" 2>/dev/null || true
    wait "${xvfb_pid}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

# -displayfd picks a free display number and reports it once the server accepts
# connections, so there is no fixed :99 to collide with and no readiness poll.
exec {display_fd}< <(exec Xvfb -displayfd 1 -screen 0 1296x1114x24 -nolisten tcp 2>/dev/null)
xvfb_pid=$!
display_number=""
if ! read -r -t 15 -u "${display_fd}" display_number || [[ ! "${display_number}" =~ ^[0-9]+$ ]]; then
  echo "godot-mcp.sh: Xvfb did not report a display within 15 s (got '${display_number}')" >&2
  exit 1
fi
exec {display_fd}<&-
export DISPLAY=":${display_number}"

# Background job with stdin passed through explicitly (a non-interactive shell
# would otherwise give it /dev/null), so the trap can still stop Xvfb on exit.
npx --yes "${SERVER_PACKAGE}" <&0 &
server_pid=$!
trap 'kill -TERM "${server_pid}" 2>/dev/null || true' TERM INT HUP

status=0
wait "${server_pid}" || status=$?
# A forwarded signal interrupts the first wait; reap the server before Xvfb goes.
if kill -0 "${server_pid}" 2>/dev/null; then
  status=0
  wait "${server_pid}" || status=$?
fi
exit "${status}"
