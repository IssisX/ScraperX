#!/usr/bin/env python3
"""The test objects of live fabrication step 1, made from their pinned CC0
sources (sources.json) into godot/tests/fabricated/, the one place they are
used: the native falsifier's test worlds (tests/sim_tests_fabricated.cpp)
and the presentation's (godot/tests/fabricated_view_test.gd). Never the
player's world.

    build/fab-venv/bin/python3 -I tools/fab/test_objects.py [sources dir] [out dir]
    (defaults: build/fab/sources, godot/tests/fabricated)

Each source is a Poly Haven model as published: a .gltf with its .bin and
textures. Every file is fetched once and checked against its SHA-256 (a
file already there with the right hash is kept; a download with the wrong
one is an error and nothing is written for it), packed losslessly into one
GLB (buffers and images moved into its binary chunk, nothing else changed),
and that GLB is made into the game's object by fab.py's make, with the
recipe's material and size. Needs the dev key (fab.py keygen).
"""
import argparse
import hashlib
import json
import os
import struct
import sys
import time
import urllib.error
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

import fab  # noqa: E402
import glb  # noqa: E402

USER_AGENT = "ScraperX-fab-tool/1 (test assets; tools/fab/test_objects.py)"
KEEP_IMPORT = '[remap]\n\nimporter="keep"\n'


def fetch(url, sha256, path):
    if os.path.exists(path) and hashlib.sha256(open(path, "rb").read()).hexdigest() == sha256:
        return False
    for attempt in range(4):
        try:
            request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(request, timeout=120) as response:
                data = response.read()
            break
        except (ConnectionError, urllib.error.URLError) as error:
            if isinstance(error, urllib.error.HTTPError) or attempt == 3:
                raise
            time.sleep(2.0 ** (attempt + 1))
    digest = hashlib.sha256(data).hexdigest()
    if digest != sha256:
        sys.exit("refused: %s hashes to %s, pinned %s" % (url, digest, sha256))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as handle:
        handle.write(data)
    return True


def pack_glb(folder, gltf_name):
    """A .gltf with external buffers and images, as one GLB carrying them."""
    gltf = json.load(open(os.path.join(folder, gltf_name)))
    chunks, offsets, total = [], [], 0
    for buffer in gltf.get("buffers", []):
        data = open(os.path.join(folder, buffer["uri"]), "rb").read()
        if len(data) < buffer["byteLength"]:
            sys.exit("refused: %s is shorter than its buffer" % buffer["uri"])
        offsets.append(total)
        chunks.append(data + b"\x00" * ((4 - len(data) % 4) % 4))
        total += len(chunks[-1])
    for view in gltf.get("bufferViews", []):
        view["byteOffset"] = view.get("byteOffset", 0) + offsets[view["buffer"]]
        view["buffer"] = 0
    for image in gltf.get("images", []):
        if "uri" not in image:
            continue
        data = open(os.path.join(folder, image.pop("uri")), "rb").read()
        gltf.setdefault("bufferViews", []).append({"buffer": 0, "byteOffset": total, "byteLength": len(data)})
        image["bufferView"] = len(gltf["bufferViews"]) - 1
        chunks.append(data + b"\x00" * ((4 - len(data) % 4) % 4))
        total += len(chunks[-1])
    gltf["buffers"] = [{"byteLength": total}]
    body = json.dumps(gltf, separators=(",", ":"), sort_keys=True).encode("utf-8")
    body += b" " * ((4 - len(body) % 4) % 4)
    binary = b"".join(chunks)
    length = 12 + 8 + len(body) + 8 + len(binary)
    return (struct.pack("<III", glb.GLB_MAGIC, 2, length) + struct.pack("<II", len(body), glb.CHUNK_JSON) + body +
            struct.pack("<II", len(binary), glb.CHUNK_BIN) + binary)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("sources", nargs="?", default=os.path.join(REPO, "build", "fab", "sources"))
    parser.add_argument("out", nargs="?", default=os.path.join(REPO, "godot", "tests", "fabricated"))
    parser.add_argument("--private", default=fab.DEFAULT_PRIVATE)
    parser.add_argument("--public", default=fab.DEFAULT_PUBLIC)
    parser.add_argument("--only", help="one object's name")
    args = parser.parse_args()
    table = json.load(open(os.path.join(HERE, "sources.json")))
    os.makedirs(args.out, exist_ok=True)
    for name, recipe in sorted(table["objects"].items()):
        if args.only and name != args.only:
            continue
        folder = os.path.join(args.sources, name)
        fetched = sum(fetch(entry["url"], entry["sha256"], os.path.join(folder, rel))
                      for rel, entry in sorted(recipe["files"].items()))
        model = os.path.join(args.sources, name + ".glb")
        with open(model, "wb") as handle:
            handle.write(pack_glb(folder, recipe["gltf"]))
        print("%s: %d of %d source files fetched, packed into %s" % (name, fetched, len(recipe["files"]), model))
        fab.make(argparse.Namespace(model=model, name=name, material=recipe["material"], size=recipe["size_m"],
                                    out=args.out, private=args.private, public=args.public, triangles=4000,
                                    texture=512, seed=0))
        made = json.load(open(os.path.join(args.out, name + ".json")))
        made["source"].update({"title": recipe["title"], "author": recipe["author"], "page": recipe["page"],
                               "license": recipe["license"]})
        with open(os.path.join(args.out, name + ".json"), "w") as handle:
            json.dump(made, handle, indent=1, sort_keys=True)
            handle.write("\n")
        for suffix in (".glb", ".json"):
            with open(os.path.join(args.out, name + suffix + ".import"), "w") as handle:
                handle.write(KEEP_IMPORT)


if __name__ == "__main__":
    main()
