# Godot viewer setup (evidence tool for the Matron audit)

Docs only. No game code, no test, and no world change is part of this file.
The viewer is the way to watch the game run and capture real frames and
debug output instead of arguing from script text alone.

## Verified base

- Trunk branch: `ScraperX-Claude`
- Trunk head verified by `git ls-remote` at setup time:
  `d28d3486efdd1566fa9f52713c5317befd06ba73`
  (`ci: the playtest APK only on a manual dispatch (owner: playtests use
  a local build)`)
- Setup branch (local only, not pushed): `fb-bot/godot-viewer-setup-20261028`
- Worktree used for verification:
  `~/workspace/ScraperX-godot-viewer-setup`
- Godot editor binary, installed by the repo's own script (see step 2):
  `4.7.stable.official.5b4e0cb0f`
- Native dependency pins at this head (`CMakeLists.txt`):
  godot-cpp `507ed9d840c01a3c5b2a39af8bb4000bfac30bf5`,
  Jolt `e77f175595e64cb44218cc9d9d56fc365ad0e36a`,
  Monocypher `ab2b16dd619ad5f6979a4fbe69cfa324a6fcc35f`
- Tool versions observed: cmake 3.28.3, ninja 1.13.2 (at
  `~/.local/bin/ninja`, not on `PATH` by default on this host).

## Why a clean checkout did not run before this setup

A fresh checkout of the trunk head contains only the extension descriptor
`godot/addons/scraperx_native/scraperx_native.gdextension`. It has:

- no `build/` directory,
- no `godot/.godot/extension_list.cfg`,
- no `godot/addons/scraperx_native/bin/linux/libscraperx_native.so`
  (only a `.gitkeep`).

Without the built library and the editor import, the game exits with code
20 (`SCRAPERX_EXTENSION_LOAD_FAILED`). `check_project` passing alone does
not prove the game runs; it only reads project config. The two steps
below close exactly that gap.

## Reproduce from scratch

Run every command from the worktree root unless a step says `godot/`.

### 0. Fresh worktree from the verified trunk head

```sh
git ls-remote https://github.com/IssisX/ScraperX.git refs/heads/ScraperX-Claude
git worktree add ~/workspace/ScraperX-godot-viewer-setup \
  -b fb-bot/godot-viewer-setup-20261028 \
  d28d3486efdd1566fa9f52713c5317befd06ba73
cd ~/workspace/ScraperX-godot-viewer-setup
git status --short   # must be empty before starting
```

Do not reuse a worktree on another branch for trunk claims.

### 1. Put ninja on PATH

```sh
export PATH="$HOME/.local/bin:$PATH"
ninja --version   # observed: 1.13.2
```

### 2. Install the pinned Godot editor in the worktree

```sh
bash .claude/install-godot.sh
./tools/godot/godot --headless --version
# observed: 4.7.stable.official.5b4e0cb0f
```

This downloads the pinned release, checks its SHA-256 against
`.github/workflows/wo000-delivery-spine.yml`, and installs it at
`tools/godot/godot`. That path is gitignored, so `git status` stays clean.

### 3. Configure the host build (same flags as CI)

```sh
cmake -S . -B build/host -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DSCRAPERX_BUILD_BRIDGE=ON \
  -DSCRAPERX_WITH_JOLT=ON \
  -DGODOTCPP_TARGET=template_release
```

Observed on this host: configure finished in about 106 seconds and wrote
`build/host/build.ninja`. FetchContent downloads godot-cpp, Jolt, and
Monocypher at the pins above; that needs network access.

### 4. Build only the native extension target

```sh
cmake --build build/host --target scraperx_native --parallel 2
```

Observed on this host: 1304 build steps in about 19 minutes with
`--parallel 2` (2 CPUs, 7.7 GB RAM, no swap). The result is gitignored:

```sh
ls -la godot/addons/scraperx_native/bin/linux/libscraperx_native.so
# observed: 7,233,872 bytes, ELF 64-bit LSB shared object, x86-64
```

Do not build the full test set for a viewer setup; the extension target
already builds the simulation and delivery libraries it links.

### 5. Editor import (registers the extension)

```sh
cd godot
../tools/godot/godot --headless --editor --quit
cd ..
```

Observed: exit code 0 in about 9 seconds. Afterwards this file exists
and names the extension:

```sh
cat godot/.godot/extension_list.cfg
# res://addons/scraperx_native/scraperx_native.gdextension
```

The import also writes `godot/.godot/` (gitignored) plus generated files
next to sources: 91 `.uid` files and 40 `.ogg.import` files at this head.
See Cleanup below; none of them are committed.

### 6. Prove the game starts (direct run, no bridge needed)

```sh
cd godot
SCRAPERX_PREVIEW=1 timeout 15 ../tools/godot/godot --headless --path .
echo "exit: $?"
cd ..
```

Observed: exit code 124, which is only `timeout` stopping a game that was
still running after 15 seconds. The output carries both success markers
and no error lines:

```text
Godot Engine v4.7.stable.official.5b4e0cb0f
SCRAPERX_AUDIO_VOICE lines=57
SCRAPERX_EXTENSION_LOADED api=4.7 authority=scraperx_sim work_order=WO-006
SCRAPERX_VIEWPORT size=2160x2160 aspect=1.000 fov=82.0 far=2600
SCRAPERX_AUDIO_BANK clips=63 msec=3366 driver=Dummy
SCRAPERX_MATRON_BANK clips=19 streams=29 msec=287
```

A run that exits quickly with code 20 means step 4 or step 5 was skipped
or failed; do not work around it, rebuild and re-import instead.
Note: the headless viewport here is 2160x2160; the bridge run recorded
elsewhere reports 2160x1856. The window size differs by runner; the two
marker lines are the check, not the exact viewport size.

## Verify the bridge run (still to be run by the coordinator)

The direct run above proves the extension loads and the game stays up.
The bridge run itself was NOT run from this setup session, because this
session has no bridge tool access; label it untested until it is run.
From an agent with bridge access, in this worktree:

```sh
python3 ~/workspace/custom-connectors/custom.godot-mcp/bin/client.py start
# then: list-tools (expect 39 tools)
# then: run_project with projectPath "<worktree>/godot", background true
# then: get_debug_output — expect the same SCRAPERX_EXTENSION_LOADED and
#   SCRAPERX_VIEWPORT markers, and the game still running, not exited
# then: take_screenshot / get_ui_elements as needed for the audit
# then: stop_project when done, to free the slot
```

If `run_project` reports the game exited, check `get_debug_output`
first; do not retry blindly. A code 20 exit means the setup in steps
4-5 did not hold in the worktree the bridge is pointed at — most often
the bridge is pointed at a different worktree than the one just built.

## Cleanup before finishing

Generated files are never committed. After verification, from the
worktree root:

```sh
rm -rf godot/.godot
git clean -f godot
git status --short   # must show only this file (until it is committed)
```

`git clean` removes untracked files only, which is exactly the generated
set. Do NOT delete by extension with `find ... -delete`: this repo tracks
99 `.import` files for existing assets, and a blind delete removes those
too. If that ever happens, restore them with `git restore godot` and
re-check `git status` before doing anything else.

What may stay on disk locally, gitignored and uncommitted: `build/`
(656 MB at this head), `tools/godot/`, and the built
`godot/addons/scraperx_native/bin/linux/libscraperx_native.so`. Keeping
them makes the next viewer run a re-import instead of a rebuild. None of
them belong in any commit from this setup.

## Never commit from this setup

- No game code, no test file, no world geometry.
- No `.uid` files, no `.import` files, no `godot/.godot/` files.
- No `build/`, `tools/godot/`, or `*.so` files.
- Only this file, in a docs-only commit.
