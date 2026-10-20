#!/usr/bin/env python3
"""Fetch the MakeHuman files the body builders read, pinned and checked.

Run with:  python3 -I tools/body/fetch_makehuman.py [dest]   (default build/makehuman)

Every file in sources.json is CC0 1.0 (MakeHuman's assets; each file's header
or its JSON "license" field says so), pinned to one commit of
github.com/makehumancommunity/makehuman and to its SHA-256. A file already in
`dest` with the right hash is kept; one with the wrong hash is fetched again;
a download whose hash is wrong is an error and nothing is written for it.
"""
import hashlib
import json
import os
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def main():
    dest = sys.argv[1] if len(sys.argv) > 1 else os.path.join("build", "makehuman")
    sources = json.load(open(os.path.join(HERE, "sources.json")))
    fetched = kept = 0
    for rel, entry in sorted(sources["files"].items()):
        path = os.path.join(dest, rel)
        if os.path.exists(path) and sha256(open(path, "rb").read()) == entry["sha256"]:
            kept += 1
            continue
        with urllib.request.urlopen(entry["url"], timeout=60) as response:
            data = response.read()
        got = sha256(data)
        if got != entry["sha256"]:
            raise SystemExit("fetch_makehuman: %s: sha256 %s, expected %s (%s)" % (rel, got, entry["sha256"], entry["url"]))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path + ".part", "wb") as out:
            out.write(data)
        os.replace(path + ".part", path)
        fetched += 1
    print("fetch_makehuman: %d fetched, %d already there, in %s" % (fetched, kept, dest))


if __name__ == "__main__":
    main()
