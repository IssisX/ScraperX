#!/usr/bin/env python3
"""Fetch the MakeHuman files the body builders read, pinned and checked.

Run with:  python3 -I tools/body/fetch_makehuman.py [dest]   (default build/makehuman)

Every file in sources.json is CC0 1.0 (MakeHuman's assets; each file's header
or its JSON "license" field says so). A plain entry is pinned to one commit of
github.com/makehumancommunity/makehuman by its URL; a "zip" entry is one
member of MakeHuman's CC0 system asset pack, read out of the remote zip with
HTTP range requests (only that member's bytes are downloaded). Every entry is
pinned to its SHA-256. A file already in `dest` with the right hash is kept;
one with the wrong hash is fetched again; a download whose hash is wrong is an
error and nothing is written for it.
"""
import hashlib
import io
import json
import os
import sys
import time
import urllib.error
import urllib.request
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
RETRIES = 4


def sha256(data):
    return hashlib.sha256(data).hexdigest()


class RemoteFile(io.RawIOBase):
    """A remote file read by HTTP range requests, for zipfile to seek in."""

    def __init__(self, url):
        self.url, self.pos = url, 0
        with urllib.request.urlopen(urllib.request.Request(url, method="HEAD"), timeout=60) as response:
            self.size = int(response.headers["Content-Length"])

    def seekable(self):
        return True

    def readable(self):
        return True

    def tell(self):
        return self.pos

    def seek(self, offset, whence=0):
        self.pos = offset if whence == 0 else (self.pos + offset if whence == 1 else self.size + offset)
        return self.pos

    def readinto(self, buf):
        if self.pos >= self.size:
            return 0
        end = min(self.size, self.pos + len(buf)) - 1
        request = urllib.request.Request(self.url, headers={"Range": "bytes=%d-%d" % (self.pos, end)})
        # A dropped connection is tried again, at most RETRIES times, waiting
        # longer each time; anything else (a 404, a bad range) fails at once.
        for attempt in range(RETRIES + 1):
            try:
                with urllib.request.urlopen(request, timeout=120) as response:
                    data = response.read()
                break
            except (ConnectionError, urllib.error.URLError) as error:
                if isinstance(error, urllib.error.HTTPError) or attempt == RETRIES:
                    raise
                time.sleep(2.0 ** (attempt + 1))
        buf[:len(data)] = data
        self.pos += len(data)
        return len(data)


def main():
    dest = sys.argv[1] if len(sys.argv) > 1 else os.path.join("build", "makehuman")
    sources = json.load(open(os.path.join(HERE, "sources.json")))
    fetched = kept = 0
    zips = {}
    for rel, entry in sorted(sources["files"].items()):
        path = os.path.join(dest, rel)
        if os.path.exists(path) and sha256(open(path, "rb").read()) == entry["sha256"]:
            kept += 1
            continue
        if "zip" in entry:
            if entry["zip"] not in zips:
                zips[entry["zip"]] = zipfile.ZipFile(io.BufferedReader(RemoteFile(entry["zip"]), 1 << 20))
            data = zips[entry["zip"]].read(entry["member"])
            where = "%s in %s" % (entry["member"], entry["zip"])
        else:
            with urllib.request.urlopen(entry["url"], timeout=60) as response:
                data = response.read()
            where = entry["url"]
        got = sha256(data)
        if got != entry["sha256"]:
            raise SystemExit("fetch_makehuman: %s: sha256 %s, expected %s (%s)" % (rel, got, entry["sha256"], where))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path + ".part", "wb") as out:
            out.write(data)
        os.replace(path + ".part", path)
        fetched += 1
    print("fetch_makehuman: %d fetched, %d already there, in %s" % (fetched, kept, dest))


if __name__ == "__main__":
    main()
