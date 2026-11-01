# Shared ScraperX engineering toolchain (Linux x86_64)

This is a **binary/source cache for other agents**, not a new gameplay subsystem.
Use it for native tests, Jolt and Godot development, and Godot MCP visual
playtests when the session exposes an MCP client. An MCP server handshake alone
does not prove that the rendered game was viewed.

## Install once per persistent agent environment

From the ScraperX repository checkout:

```bash
toolchain="$(bash tools/shared-toolchain/install.sh)"
export SCRAPERX_TOOLCHAIN_DIR="$toolchain"
export PATH="$toolchain/bin:$PATH"
export GODOT_PATH="$toolchain/bin/godot"
```

The installer downloads the versioned GitHub Release
[`scraperx-toolchain-linux-x86_64-v1`](https://github.com/IssisX/ScraperX/releases/tag/scraperx-toolchain-linux-x86_64-v1),
checks the release SHA-256 manifest, extracts atomically under
`~/.cache/scraperx/shared-toolchain-linux-x86_64-v1`, verifies tools and
sources, and returns the same installed path on subsequent calls without
downloading again. `SCRAPERX_TOOLCHAIN_CACHE_DIR` can direct the cache to a
shared persistent volume. On ephemeral runners, this still requires a download;
the package eliminates repeated compilation and dependency resolution, not
the laws of network storage.

The bundle includes CMake 3.31.10 (3.31.8 in the archived bundle), Ninja 1.12.1, Godot 4.7-stable,
Node 22, godot-mcp-runtime 3.8.1 and the **exact** CMake-pinned Jolt,
godot-cpp and Monocypher source trees. It is built for Ubuntu 24.04 x86_64,
not Android/ARM, macOS or arbitrary Linux ABI combinations. Jolt is a pinned
**source dependency**, not a standalone precompiled library: build it as part
of ScraperX to match the repository's compiler flags and patches.

## Build without re-fetching sources

```bash
cmake -S . -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DSCRAPERX_BUILD_BRIDGE=ON -DSCRAPERX_WITH_JOLT=ON \
  -DGODOTCPP_TARGET=template_release \
  -DFETCHCONTENT_SOURCE_DIR_JOLT="$SCRAPERX_TOOLCHAIN_DIR/deps/jolt" \
  -DFETCHCONTENT_SOURCE_DIR_GODOT_CPP="$SCRAPERX_TOOLCHAIN_DIR/deps/godot-cpp" \
  -DFETCHCONTENT_SOURCE_DIR_MONOCYPHER="$SCRAPERX_TOOLCHAIN_DIR/deps/monocypher"
cmake --build build/host --target scraperx_sim_tests scraperx_native --parallel 4
SCRAPERX_ONLY=matron-found-climb build/host/scraperx_sim_tests
```

The repo's `.mcp.json` launches `.claude/godot-mcp.sh`. With
`SCRAPERX_TOOLCHAIN_DIR` exported, that launcher uses the bundled Godot and
Node/MCP module; without it, the original installation and npx fallbacks
remain available. In a graphical session, run the Godot project with
`godot/` as the project path, inspect actual screenshots and input behavior,
and close the runtime after proof. Headless or scripted native tests do not
replace visual gameplay evidence.

## About the earlier November 1 backup archive

The previous conversation's saved
`ScraperX-reusable-toolchain-linux-x86_64-20261101.tar.gz`
(SHA-256 `00366ce2e81ae6807215077c039c7f81a9199c6c25e733e04018559cd6da743a`)
is a backup with CMake 3.31.8, Ninja, Godot and MCP dependencies,
**but it does not contain `bin/node`**. Do not pass that legacy archive to
this installer as though it had the published v1 layout: the strict
toolchain check will reject it. Agents should use the published release v1
asset, which includes Node 22 and passes the real install/cache test.

The smaller stand-alone MCP archive remains a separately preserved backup.
These old conversation Library archives are not automatically shared with
other agents.

## Release ownership and verification

`.github/workflows/shared-toolchain-release.yml` is a manually triggered,
versioned publishing job. It resolves the
dependency commits from **CMakeLists.txt**, verifies the Godot editor using
the authoritative delivery-spine SHA pin, builds the portable package,
runs the real offline installer and cache-reuse checks, uploads an artifact
and publishes a release only if the exact checks succeed. The workflow does
not modify the ScraperX trunk or push to `main`. If a release with the
versioned tag already exists, it refuses to overwrite it. To change pinned
versions, deliberately cut a new versioned tag and installer pin, never
replace a release in place.

This toolchain is not a proof of ScraperX gameplay correctness. Keep native,
Godot, Android, visual-playtest and independent-review gates separate.
