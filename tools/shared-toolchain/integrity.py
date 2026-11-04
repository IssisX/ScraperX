#!/usr/bin/env python3
"""Validate a transferable toolchain archive and detect cache changes."""
from collections import deque
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


def resolve_archive_path(parts, symlinks):
    """Resolve tar paths in extraction order, expanding links before '..'."""
    pending = deque(parts)
    resolved = []
    expansions = 0
    while pending:
        part = pending.popleft()
        if part in ("", "."):
            continue
        if part == "..":
            if len(resolved) <= 1:
                raise ValueError("link escapes archive root through parent traversal")
            resolved.pop()
            continue
        if not resolved and part != ROOT:
            raise ValueError(f"link escapes archive root: {part!r}")
        resolved.append(part)
        name = "/".join(resolved)
        if name in symlinks:
            expansions += 1
            if expansions > len(symlinks) * 2 + 1:
                raise ValueError(f"cyclic archive symlink: {name!r}")
            resolved.pop()
            pending.extendleft(reversed(symlinks[name].split("/")))
    result = "/".join(resolved)
    if not contained(result):
        raise ValueError(f"link escapes archive root: {result!r}")
    return result


def validate_archive(archive_path):
    seen = set()
    symlinks = {}
    hardlinks = []
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
                    symlinks[normalized] = target
                else:
                    hardlinks.append((normalized, target))
                continue
            raise ValueError(f"unsupported archive entry type: {name!r}")

    # GNU tar extracts through directory aliases.  A member under A/ when
    # A -> R would be written under R/ while our symlink table would still
    # record A/member, letting a chained link evade resolution.  Reject such
    # entries regardless of member order; internal npm links remain valid.
    for name in seen:
        parent = posixpath.dirname(name)
        if parent in ("", "."):
            continue
        actual_parent = resolve_archive_path(parent.split("/"), symlinks)
        if actual_parent != parent:
            raise ValueError(
                f"unsafe archive entry {name!r}: parent resolves to {actual_parent!r}"
            )

    # Resolve against every archive symlink, regardless of member order.
    # Lexical normpath is insufficient: l1 -> ., l2 -> l1/.. can climb
    # above ROOT even though each link text looks locally contained.
    for name in seen:
        try:
            resolve_archive_path(name.split("/"), symlinks)
        except ValueError as error:
            raise ValueError(f"unsafe archive entry {name!r}: {error}") from error
    for name, target in symlinks.items():
        try:
            resolve_archive_path(posixpath.dirname(name).split("/") + target.split("/"),
                                 symlinks)
        except ValueError as error:
            raise ValueError(f"link escapes archive root: {name!r} -> {target!r}: {error}") from error
    for name, target in hardlinks:
        try:
            resolve_archive_path(target.split("/"), symlinks)
        except ValueError as error:
            raise ValueError(f"link escapes archive root: {name!r} -> {target!r}: {error}") from error


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
