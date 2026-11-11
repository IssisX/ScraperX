#!/usr/bin/env bash
# Install CI's apt packages without depending on one mirror (11-11).
#
# Why: from 14:50 to past 16:10 UTC on 11-11 every proof's host-build and android-build failed in
# `apt-get install` with "Temporary failure resolving 'azure.archive.ubuntu.com'": the runner's
# mirror was unreachable, so no game proof could go green, whoever pushed.
#
# Order: (1) every package already installed (the runner image may carry it): install nothing;
# (2) apt from the runner's own mirror, bounded; (3) the same with every azure mirror host
# rewritten to archive.ubuntu.com; else fail with the packages named. Packages are the same in
# every path, so the build's tools and flags do not change.
#
# Usage: ci/apt_install.sh <package>...
set -euo pipefail

if [ "$#" -eq 0 ]; then
  echo "apt_install: no packages given" >&2
  exit 2
fi

missing=()
for pkg in "$@"; do
  if ! dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q "install ok installed"; then
    missing+=("$pkg")
  fi
done
if [ "${#missing[@]}" -eq 0 ]; then
  echo "apt_install: all already installed: $*"
  exit 0
fi
echo "apt_install: to install: ${missing[*]}"

apt_try() {
  timeout 120 sudo apt-get -o Acquire::Retries=3 -o Acquire::http::Timeout=20 update &&
    timeout 420 sudo apt-get -o Acquire::Retries=3 -o Acquire::http::Timeout=20 install --yes "${missing[@]}"
}

if apt_try; then
  exit 0
fi

echo "apt_install: the runner's mirror failed; retrying on archive.ubuntu.com" >&2
# GitHub's runners list their mirror in /etc/apt/apt-mirrors.txt (sources use mirror+file:), so that file is
# rewritten too (11-11: rewriting only the sources files left every request on the azure host).
for f in /etc/apt/apt-mirrors.txt /etc/apt/sources.list /etc/apt/sources.list.d/*.list /etc/apt/sources.list.d/*.sources; do
  [ -f "$f" ] || continue
  sudo sed -i -E 's#https?://azure\.archive\.ubuntu\.com/#http://archive.ubuntu.com/#g' "$f"
done
if apt_try; then
  exit 0
fi

# Both Ubuntu mirrors unreachable (11-11: archive.ubuntu.com timed out too while packages.microsoft.com
# answered). Only the two build tools have a fallback that changes no compiler flag:
#   ninja-build: the PyPI ninja wheel, pinned by version and sha256 (--require-hashes);
#   ccache:      none, so the compiler launcher is cleared and the build runs uncached (slower, same flags).
# Anything else (the touch job's X libraries) has no fallback and fails here, named.
left=()
for pkg in "${missing[@]}"; do
  case "$pkg" in
    ninja-build)
      req="$(mktemp)"
      echo "ninja==1.11.1.1 --hash=sha256:84502ec98f02a037a169c4b0d5d86075eaf6afc55e1879003d6cab51ced2ea4b" >"$req"
      if timeout 180 python3 -m pip install --user --require-hashes --only-binary=:all: -r "$req"; then
        echo "$HOME/.local/bin" >>"${GITHUB_PATH:-/dev/null}"
        export PATH="$HOME/.local/bin:$PATH"
        ninja --version
      else
        left+=("$pkg")
      fi
      rm -f "$req"
      ;;
    ccache)
      echo "apt_install: no ccache; this build runs without the compiler cache" >&2
      { echo "CMAKE_CXX_COMPILER_LAUNCHER="; echo "CMAKE_C_COMPILER_LAUNCHER="; } >>"${GITHUB_ENV:-/dev/null}"
      ;;
    *)
      left+=("$pkg")
      ;;
  esac
done
if [ "${#left[@]}" -eq 0 ]; then
  exit 0
fi
echo "apt_install: could not install ${left[*]} from either mirror, and they have no fallback" >&2
exit 1
