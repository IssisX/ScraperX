#!/usr/bin/env python3
"""Validate a transferable toolchain archive and detect cache changes."""
import hashlib
import json
import os
from pathlib import Path
import posixpath
import stat
import sys
import tarfile

ROOT = "reusable-toolchain"
MANIFEST = ".scraperx-integrity.json"


def contained(path):
    return path == ROOT or path.startswith(ROOT + "/")


def validate_archive(archive_path):
    seen = set()
    with tarfile.open(archive_path, "r:gz") as archive:
        for member in archive:
            name = member.name.rstrip("/")
            normalized = posixpath.normpath(name)
            if (not name or name.startswith("/") or ".." in name.split("/")
                    or not contained(normalized) or normalized in seen):
                raise ValueError(f"unsafe or duplicate archive entry: {member.name!r}")
            seen.add(normalized)
            if member.isdir() or member.isfile():
                continue
            if member.issym() or member.islnk():
                target = member.linkname
                if not target or target.startswith("/"):
                    raise ValueError(f"unsafe link {name!r} -> {target!r}")
                if member.issym():
                    target = posixpath.normpath(posixpath.join(posixpath.dirname(name), target))
                else:
                    target = posixpath.normpath(target)
                if not contained(target):
                    raise ValueError(f"link escapes archive root: {name!r} -> {member.linkname!r}")
                continue
            raise ValueError(f"unsupported archive entry type: {name!r}")


def inventory(root):
    result = {}
    def scan(directory):
        for entry in sorted(os.scandir(directory), key=lambda e: e.name):
            relative = os.path.relpath(entry.path, root)
            if relative == MANIFEST:
                continue
            mode = entry.stat(follow_symlinks=False).st_mode
            if stat.S_ISLNK(mode):
                result[relative] = ["link", os.readlink(entry.path)]
            elif stat.S_ISDIR(mode):
                result[relative] = ["dir"]
                scan(entry.path)
            elif stat.S_ISREG(mode):
                digest = hashlib.sha256()
                with open(entry.path, "rb") as stream:
                    for block in iter(lambda: stream.read(1024 * 1024), b""):
                        digest.update(block)
                result[relative] = ["file", digest.hexdigest()]
            else:
                raise ValueError(f"unsupported installed entry type: {relative!r}")
    scan(root)
    return result


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ("archive", "stamp", "verify"):
        raise ValueError("usage: integrity.py archive ARCHIVE | stamp DIR | verify DIR")
    operation, location = sys.argv[1:]
    if operation == "archive":
        validate_archive(location)
        return
    root = Path(location)
    manifest = root / MANIFEST
    if operation == "stamp":
        if manifest.exists():
            raise ValueError(f"archive unexpectedly contains reserved {MANIFEST}")
        manifest.write_text(json.dumps(inventory(root), sort_keys=True, separators=(",", ":")) + "\n")
    elif operation == "verify":
        if not manifest.is_file():
            raise ValueError(f"cache lacks integrity manifest: {manifest}")
        expected = json.loads(manifest.read_text())
        if inventory(root) != expected:
            raise ValueError(f"installed cache differs from verified archive: {root}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, tarfile.TarError, json.JSONDecodeError) as error:
        print(f"toolchain integrity: {error}", file=sys.stderr)
        sys.exit(1)
