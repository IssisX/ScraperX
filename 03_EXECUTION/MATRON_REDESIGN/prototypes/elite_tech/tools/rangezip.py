"""Extract chosen members from a remote zip with HTTP Range reads (own code).
usage: python3 -I rangezip.py URL OUTDIR substr [substr...]   (no substr: list)"""
import io, os, sys, urllib.request, zipfile
class R(io.RawIOBase):
    def __init__(s, url):
        s.url, s.pos = url, 0
        with urllib.request.urlopen(urllib.request.Request(url, method="HEAD"), timeout=60) as r:
            s.size = int(r.headers["Content-Length"])
    def seekable(s): return True
    def readable(s): return True
    def tell(s): return s.pos
    def seek(s, o, w=0):
        s.pos = o if w == 0 else (s.pos + o if w == 1 else s.size + o); return s.pos
    def readinto(s, b):
        if s.pos >= s.size: return 0
        end = min(s.size, s.pos + len(b)) - 1
        with urllib.request.urlopen(urllib.request.Request(s.url, headers={"Range": "bytes=%d-%d" % (s.pos, end)}), timeout=120) as r:
            d = r.read()
        b[:len(d)] = d; s.pos += len(d); return len(d)
url, out, subs = sys.argv[1], sys.argv[2], sys.argv[3:]
z = zipfile.ZipFile(io.BufferedReader(R(url), 1 << 20))
for n in z.namelist():
    if not subs:
        print(n); continue
    if any(s in n for s in subs) and not n.endswith("/"):
        p = os.path.join(out, os.path.basename(n)); open(p, "wb").write(z.read(n)); print("got", n, os.path.getsize(p))
