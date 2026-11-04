#!/usr/bin/env bash
# Install the pinned ScraperX Linux x86_64 tools once, then reuse that cache.
# Prints only the absolute toolchain directory on stdout.
set -euo pipefail

# A cache-provided dirname, python3 or sha256sum must never validate itself.
PATH="/usr/local/bin:/usr/bin:/bin"
export PATH

release_tag="scraperx-toolchain-linux-x86_64-v1"
asset="ScraperX-toolchain-linux-x86_64-v1.tar.gz"
repo="IssisX/ScraperX"
# This is an independent trust anchor; a checksum downloaded beside an archive
# does not authenticate that archive if both release assets are replaced.
published_sha256="138c0a94c9ce44cc99cda847e71b7948f0e04a051cf442f17730a8526726f571"
# Independently pins the installed inventory, not a manifest writable inside the cache.
# Derived from the verified published v1 archive on Actions run 41493313123.
published_manifest_sha256="b912841dcc620a2764899d431d2cf34afe8ae4c807b5189768c2370b8da8d1de"
script_dir="$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)"
cache="${SCRAPERX_TOOLCHAIN_CACHE_DIR:-${XDG_CACHE_HOME:-${HOME}/.cache}/scraperx/shared-toolchain-linux-x86_64-v1}"
archive_override=""
verify_only=false

while (($#)); do
    case "$1" in
        --verify) verify_only=true; shift ;;
        --from-archive)
            (($# >= 2)) || { echo "install: --from-archive requires a file" >&2; exit 2; }
            archive_override="$2"; shift 2 ;;
        *)
            echo "install: unknown argument: $1 (expected --verify or --from-archive FILE)" >&2
            exit 2 ;;
    esac
done

[[ "$(uname -s)" == "Linux" && "$(uname -m)" == "x86_64" ]] || {
    echo "install: Linux x86_64 required; this bundle cannot run on Android/ARM or macOS" >&2
    exit 2
}
command -v flock >/dev/null || { echo "install: flock required" >&2; exit 2; }
command -v python3 >/dev/null || { echo "install: python3 required" >&2; exit 2; }

check() {
    local dir="$1" version
    [[ -x "$dir/bin/cmake" && -x "$dir/bin/ninja" && -x "$dir/bin/godot" &&
       -x "$dir/bin/node" && -f "$dir/mcp/node_modules/godot-mcp-runtime/dist/index.js" &&
       -f "$dir/deps/jolt/Build/CMakeLists.txt" &&
       -f "$dir/deps/godot-cpp/CMakeLists.txt" &&
       -f "$dir/deps/monocypher/src/monocypher.c" ]] || return 1
    "$dir/bin/cmake" --version 2>/dev/null | grep -Eq '^cmake version 3[.]31[.](8|10)$' || return 1
    [[ "$("$dir/bin/ninja" --version 2>/dev/null)" == "1.12.1" ]] || return 1
    "$dir/bin/godot" --headless --version 2>/dev/null | grep -Fq "4.7.stable" || return 1
    "$dir/bin/node" --version 2>/dev/null | grep -Eq '^v22\.' || return 1
    version="$("$dir/bin/node" -p 'require(process.argv[1]).version'         "$dir/mcp/node_modules/godot-mcp-runtime/package.json" 2>/dev/null)" || return 1
    [[ "$version" == "3.8.1" ]]
}

mkdir -p "$(dirname "$cache")"
exec {lock_fd}>> "${cache}.lock"
flock "$lock_fd"
if [[ -d "$cache" ]]; then
    # The expected inventory digest lives in this trusted script, never in the
    # cache being checked. Fail closed before running any cached executable.
    expected_manifest_sha256="$published_manifest_sha256"
    if [[ -n "$archive_override" ]]; then
        [[ -f "$archive_override" && -n "${SCRAPERX_TOOLCHAIN_ARCHIVE_SHA256:-}" &&
           -n "${SCRAPERX_TOOLCHAIN_MANIFEST_SHA256:-}" ]] || {
            echo "install: offline cache reuse requires archive and both SHA-256 trust anchors" >&2
            exit 2
        }
        [[ "$(sha256sum "$archive_override" | cut -d ' ' -f1)" == "$SCRAPERX_TOOLCHAIN_ARCHIVE_SHA256" ]] || {
            echo "install: offline archive differs from caller-provided digest" >&2
            exit 1
        }
        expected_manifest_sha256="$SCRAPERX_TOOLCHAIN_MANIFEST_SHA256"
    fi
    [[ -f "$cache/.scraperx-integrity.json" ]] || {
        echo "install: existing cache lacks integrity manifest at $cache; refusing reuse" >&2
        exit 1
    }
    [[ "$(sha256sum "$cache/.scraperx-integrity.json" | cut -d ' ' -f1)" == "$expected_manifest_sha256" ]] || {
        echo "install: existing cache manifest differs from pinned inventory digest at $cache" >&2
        exit 1
    }
    python3 "$script_dir/integrity.py" verify "$cache" || {
        echo "install: existing cache integrity check failed at $cache" >&2
        exit 1
    }
    check "$cache" || {
        echo "install: existing cache failed validation at $cache; refusing to overwrite it" >&2
        exit 1
    }
    printf '%s\n' "$cache"
    exit 0
fi
if "$verify_only"; then
    echo "install: no installed toolchain at $cache" >&2
    exit 1
fi

tmp="$(mktemp -d "$(dirname "$cache")/.scraperx-toolchain.XXXXXXXX")"
trap 'rm -rf -- "$tmp"' EXIT
if [[ -n "$archive_override" ]]; then
    [[ -f "$archive_override" ]] || { echo "install: archive not found: $archive_override" >&2; exit 1; }
    [[ -n "${SCRAPERX_TOOLCHAIN_ARCHIVE_SHA256:-}" ]] || {
        echo "install: offline archive requires SCRAPERX_TOOLCHAIN_ARCHIVE_SHA256" >&2; exit 2;
    }
    cp -- "$archive_override" "$tmp/$asset"
    printf '%s  %s\n' "$SCRAPERX_TOOLCHAIN_ARCHIVE_SHA256" "$asset" > "$tmp/$asset.sha256"
else
    base="https://github.com/$repo/releases/download/$release_tag"
    command -v curl >/dev/null || { echo "install: curl required" >&2; exit 2; }
    curl --fail --location --retry 3 --silent --show-error --output "$tmp/$asset.sha256" "$base/$asset.sha256"
    [[ "$(cat "$tmp/$asset.sha256")" == "$published_sha256  $asset" ]] || {
        echo "install: published manifest differs from pinned v1 release digest" >&2
        exit 1
    }
    curl --fail --location --retry 3 --silent --show-error --output "$tmp/$asset" "$base/$asset"
fi
(cd "$tmp" && sha256sum --check --strict "$asset.sha256" >&2)
# Reject non-regular archive types, path traversal and links outside the bundle.
python3 "$script_dir/integrity.py" archive "$tmp/$asset"
tar -xzf "$tmp/$asset" -C "$tmp"
check "$tmp/reusable-toolchain" || { echo "install: extracted toolchain failed version and source checks" >&2; exit 1; }
python3 "$script_dir/integrity.py" stamp "$tmp/reusable-toolchain"
expected_manifest_sha256="$published_manifest_sha256"
if [[ -n "$archive_override" ]]; then
    expected_manifest_sha256="${SCRAPERX_TOOLCHAIN_MANIFEST_SHA256:-}"
fi
if [[ -n "$expected_manifest_sha256" ]]; then
    [[ "$(sha256sum "$tmp/reusable-toolchain/.scraperx-integrity.json" | cut -d ' ' -f1)" == "$expected_manifest_sha256" ]] || {
        echo "install: installed inventory differs from expected manifest digest" >&2
        exit 1
    }
fi
mv -- "$tmp/reusable-toolchain" "$cache"
printf '%s\n' "$cache"
