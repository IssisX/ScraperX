#!/usr/bin/env python3
"""Check every shipped binary asset in the godot tree has a record line.

Walks godot/ outside addons, .godot and tests, and requires every binary
file to be named in a record file in its own folder:

  * CREDITS.md or CANDIDATES.md — the file's name in backticks;
  * generation_receipt.json — a key that is the file's stem;
  * manifest.json — a cues key that is the file's stem.

Record files in a folder cover its subfolders too (the clean takes in
audio/story_radio/source/ are covered by the receipt and manifest in
audio/story_radio/). A file still fails unless its own name is recorded.

addons/ holds third-party editor code, not shipped assets; .godot/ is the
derived import cache; tests/ is excluded from the APK by the Android export
preset (export_filter all_resources, exclude_filter tests/*), so its
fabricated fixtures are not shipped. *.import sidecars are text, not assets.

Usage: python3 ci/verify_asset_credits.py [godot-dir]
Exit 0 prints ASSET_CREDITS PASS; exit 1 prints ASSET_CREDITS FAIL and the
unrecorded files, one per line.
"""

import json
import re
import sys
from pathlib import Path

# Binary asset kinds that ship in the APK. A new kind belongs here, not in a
# per-folder exception: if it ships, it needs a record.
BINARY_EXTS = {
    ".ogg", ".wav", ".mp3", ".flac",
    ".png", ".jpg", ".jpeg", ".webp", ".svg",
    ".glb", ".gltf", ".fbx", ".obj",
    ".bin", ".exr", ".hdr",
    ".ttf", ".otf",
    ".mp4", ".webm",
}

SKIP_DIRS = {"addons", ".godot", "tests"}

BACKTICK = re.compile(r"`([^`\s/]+)`")


def recorded_names(directory: Path, root: Path) -> set:
    """Filenames and stems the folder's record files name.

    Records are per asset folder, and a subfolder belongs to its folder:
    the files in audio/story_radio/source/ are covered by the receipt and
    manifest in audio/story_radio/. So the record files of every ancestor
    up to the godot root count. A new file still fails unless its own name
    is recorded somewhere above it.
    """
    names = set()
    current = directory
    while True:
        for record in ("CREDITS.md", "CANDIDATES.md"):
            path = current / record
            if path.is_file():
                names.update(BACKTICK.findall(path.read_text()))
        for record in ("generation_receipt.json",):
            path = current / record
            if path.is_file():
                receipt = json.loads(path.read_text())
                if isinstance(receipt, list):
                    names.update(entry["name"] for entry in receipt if isinstance(entry, dict) and "name" in entry)
                else:
                    names.update(receipt.keys())
        manifest = current / "manifest.json"
        if manifest.is_file():
            names.update(json.loads(manifest.read_text()).get("cues", {}).keys())
        if current == root:
            break
        current = current.parent
    return names


def verify(godot: Path) -> tuple:
    """Return (files, unrecorded) for the binary assets under godot/."""
    assets = []
    for path in sorted(godot.rglob("*")):
        if not path.is_file():
            continue
        if any(part in SKIP_DIRS for part in path.relative_to(godot).parts):
            continue
        if path.suffix.lower() in BINARY_EXTS:
            assets.append(path)
    unrecorded = []
    cache = {}
    for path in assets:
        directory = path.parent
        if directory not in cache:
            cache[directory] = recorded_names(directory, godot)
        names = cache[directory]
        if path.name not in names and path.stem not in names:
            unrecorded.append(str(path.relative_to(godot)))
    return assets, unrecorded


if __name__ == "__main__":
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / "godot"
    files, missing = verify(root)
    if missing:
        print(f"ASSET_CREDITS FAIL files={len(files)} recorded={len(files) - len(missing)} unrecorded={len(missing)}")
        for name in missing:
            print(f"  unrecorded: {name}")
        raise SystemExit(1)
    print(f"ASSET_CREDITS PASS files={len(files)} recorded={len(files)} unrecorded=0")
