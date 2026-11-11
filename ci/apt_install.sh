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
  timeout 240 sudo apt-get -o Acquire::Retries=3 -o Acquire::http::Timeout=20 update &&
    timeout 420 sudo apt-get -o Acquire::Retries=3 -o Acquire::http::Timeout=20 install --yes "${missing[@]}"
}

if apt_try; then
  exit 0
fi

echo "apt_install: the runner's mirror failed; retrying on archive.ubuntu.com" >&2
for f in /etc/apt/sources.list /etc/apt/sources.list.d/*.list /etc/apt/sources.list.d/*.sources; do
  [ -f "$f" ] || continue
  sudo sed -i -E 's#https?://azure\.archive\.ubuntu\.com/#http://archive.ubuntu.com/#g' "$f"
done
if apt_try; then
  exit 0
fi

echo "apt_install: could not install ${missing[*]} from either mirror" >&2
exit 1
