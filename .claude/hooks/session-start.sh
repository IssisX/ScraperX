#!/usr/bin/env bash
# Cloud-session provisioning for the "godot" MCP server in .mcp.json: the Godot
# editor binary CI pins, the Linux GDExtension, the extension registration CI
# writes before a runtime run, and a warm npx cache for the server package.
# Hook stdout becomes session context, so all tool output goes to stderr.
set -euo pipefail
exec 3>&1 1>&2

if [[ "${CLAUDE_CODE_REMOTE:-}" != "true" ]]; then
  exit 0
fi

cd "${CLAUDE_PROJECT_DIR:-$(dirname -- "${BASH_SOURCE[0]}")/../..}"

bash .claude/install-godot.sh
godot_reported="$(tools/godot/godot --headless --version)"

# Same configuration as the workflow's host build; ninja makes a rerun a no-op.
cmake -S . -B build/host -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DSCRAPERX_BUILD_BRIDGE=ON \
  -DSCRAPERX_WITH_JOLT=ON \
  -DGODOTCPP_TARGET=template_release
cmake --build build/host --target scraperx_native --parallel "$(nproc)"
test -s godot/addons/scraperx_native/bin/linux/libscraperx_native.so

# A project that has never been opened in the editor has no extension list, and
# without it Godot does not load the GDExtension at runtime.
mkdir -p godot/.godot
printf '%s\n' 'res://addons/scraperx_native/scraperx_native.gdextension' \
  > godot/.godot/extension_list.cfg

# Start and stop the server once through its launcher, with stdin closed so it
# exits at once: this fills the npx cache and proves the launcher can start.
bash .claude/godot-mcp.sh < /dev/null > /dev/null

echo "godot MCP server ready: Godot ${godot_reported} at tools/godot/godot, ScraperX GDExtension built, godot-mcp-runtime cached." >&3
