#!/usr/bin/env bash
# One-time, CI-owned build of the transferable Ubuntu 24.04/Linux x86_64 bundle.
# Builds a release asset, never modifies Git history or ScraperX game sources.
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"
[[ "$(uname -s)" == "Linux" && "$(uname -m)" == "x86_64" ]] || {
    echo "package: Linux x86_64 required" >&2; exit 2;
}

output_dir="${1:?usage: package.sh OUTPUT_DIRECTORY}"
mkdir -p "$output_dir"
output_dir="$(cd -- "$output_dir" && pwd)"
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT
stage="$work/reusable-toolchain"
mkdir -p "$stage"/{bin,share,deps,mcp}

# The official, versioned Python wheels contain relocatable CMake bin/share
# and Ninja binaries. The live CI workflow remains the Godot SHA authority.
python3 -m venv "$work/venv"
"$work/venv/bin/pip" install --disable-pip-version-check --no-cache-dir     'cmake==3.31.8' 'ninja==1.12.1'
cmake_module="$("$work/venv/bin/python" -c 'import cmake,pathlib; print(pathlib.Path(cmake.__file__).parent)')"
cp -a "$cmake_module/data/bin/." "$stage/bin/"
cp -a "$cmake_module/data/share/." "$stage/share/"
cp -L "$work/venv/bin/ninja" "$stage/bin/ninja"

# CI's own installer checks the exact pinned Godot release ZIP SHA before
# putting the editor at tools/godot/godot.
bash .claude/install-godot.sh
cp -L tools/godot/godot "$stage/bin/godot"
cp -L "$(command -v node)" "$stage/bin/node"

# Resolve exactly the same commit pins as CMakeLists.txt, not a floating
# branch, HEAD, or an independently maintained version list.
cmake -S . -B "$work/source-pins" -G Ninja     -DCMAKE_BUILD_TYPE=Release     -DSCRAPERX_BUILD_BRIDGE=ON     -DSCRAPERX_WITH_JOLT=ON     -DBUILD_TESTING=OFF     -DGODOTCPP_TARGET=template_release
for dependency in jolt godot_cpp monocypher; do
    src="$work/source-pins/_deps/${dependency}-src"
    [[ -d "$src/.git" ]] || { echo "package: missing fetched source: $src" >&2; exit 1; }
    case "$dependency" in
        jolt) dest=jolt ;;
        godot_cpp) dest=godot-cpp ;;
        monocypher) dest=monocypher ;;
    esac
    # The repository commit is still authoritative. Fail if the freshly
    # fetched source is not the commit the current CMake pins.
    case "$dependency" in
        jolt) pin="$(sed -n -E 's/^set\\(SCRAPERX_JOLT_COMMIT "([0-9a-f]{40})".*/\\1/p' CMakeLists.txt)" ;;
        godot_cpp) pin="$(sed -n -E 's/^set\\(SCRAPERX_GODOT_CPP_COMMIT "([0-9a-f]{40})".*/\\1/p' CMakeLists.txt)" ;;
        monocypher) pin="$(sed -n -E 's/^set\\(SCRAPERX_MONOCYPHER_COMMIT "([0-9a-f]{40})".*/\\1/p' CMakeLists.txt)" ;;
    esac
    [[ "${#pin}" -eq 40 && "$(git -C "$src" rev-parse HEAD)" == "$pin" ]] || {
        echo "package: wrong source revision for $dependency (expected $pin)" >&2; exit 1;
    }
    cp -a "$src/." "$stage/deps/$dest/"
    rm -rf -- "$stage/deps/$dest/.git"
done

# Preinstall the exact MCP npm package, including its runtime dependencies.
# The agent runs its bundled Node binary and never needs npx/npm resolution.
printf '%s\n' '{"name":"scraperx-godot-mcp-runtime","private":true,"version":"1.0.0"}'     > "$stage/mcp/package.json"
npm install --prefix "$stage/mcp" --save-exact --omit=dev --no-audit --no-fund     'godot-mcp-runtime@3.8.1'
[[ -f "$stage/mcp/node_modules/godot-mcp-runtime/dist/index.js" ]]

"$stage/bin/cmake" --version | grep -F 'cmake version 3.31.8'
[[ "$("$stage/bin/ninja" --version)" == '1.12.1' ]]
"$stage/bin/godot" --headless --version | grep -F '4.7.stable'
"$stage/bin/node" --version | grep -E '^v22\\.'
[[ "$("$stage/bin/node" -p 'require(process.argv[1]).version'     "$stage/mcp/node_modules/godot-mcp-runtime/package.json")" == '3.8.1' ]]
cat > "$stage/README.txt" <<EOF
ScraperX shared toolchain: Linux x86_64, built on Ubuntu 24.04.
CMake 3.31.8; Ninja 1.12.1; Godot 4.7-stable; Node 22; godot-mcp-runtime 3.8.1.
Jolt, godot-cpp, Monocypher source commits are pinned by ScraperX CMakeLists.txt.
Godot editor ZIP is verified against wo000-delivery-spine.yml.
This bundle is not an Android/ARM build, a compiled ScraperX game,
or a replacement for required physics/gameplay/CI tests.
EOF

asset="ScraperX-toolchain-linux-x86_64-v1.tar.gz"
tar --sort=name --mtime='@0' --owner=0 --group=0 --numeric-owner     -czf "$output_dir/$asset" -C "$work" reusable-toolchain
(cd "$output_dir" && sha256sum "$asset" > "$asset.sha256")

# Exercise the real consumer installer, its exact-version verification,
# integrity check, and cache reuse before publishing any release.
test_cache="$work/installed"
toolchain="$(SCRAPERX_TOOLCHAIN_CACHE_DIR="$test_cache" \
    SCRAPERX_TOOLCHAIN_ARCHIVE_SHA256="$(cut -d' ' -f1 "$output_dir/$asset.sha256")" \
    bash tools/shared-toolchain/install.sh --from-archive "$output_dir/$asset")"
[[ "$toolchain" == "$test_cache" ]]
[[ "$(SCRAPERX_TOOLCHAIN_CACHE_DIR="$test_cache" bash tools/shared-toolchain/install.sh --verify)" == "$test_cache" ]]
printf 'SHARED_TOOLCHAIN_PACKAGE PASS asset=%s bytes=%s sha256=%s\n'     "$asset" "$(stat -c '%s' "$output_dir/$asset")"     "$(cut -d' ' -f1 "$output_dir/$asset.sha256")"
