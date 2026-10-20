# List or extract members of a remote zip using HTTP Range requests.
# usage: python3 -I httpzip.py URL list [filter]
#        python3 -I httpzip.py URL get OUTDIR substr [substr...]
import sys, io, os, zipfile, urllib.request
class RangeFile(io.RawIOBase):
    def __init__(self, url):
        self.url = url; self.pos = 0
        req = urllib.request.Request(url, method='HEAD')
        with urllib.request.urlopen(req, timeout=60) as r:
            self.size = int(r.headers['Content-Length'])
    def seekable(self): return True
    def readable(self): return True
    def tell(self): return self.pos
    def seek(self, off, whence=0):
        self.pos = off if whence == 0 else (self.pos + off if whence == 1 else self.size + off)
        return self.pos
    def readinto(self, b):
        n = len(b)
        if self.pos >= self.size or n == 0: return 0
        end = min(self.size, self.pos + n) - 1
        req = urllib.request.Request(self.url, headers={'Range': 'bytes=%d-%d' % (self.pos, end)})
        with urllib.request.urlopen(req, timeout=120) as r:
            data = r.read()
        b[:len(data)] = data; self.pos += len(data); return len(data)
url, cmd = sys.argv[1], sys.argv[2]
f = io.BufferedReader(RangeFile(url), buffer_size=1 << 20)
z = zipfile.ZipFile(f)
if cmd == 'list':
    flt = sys.argv[3] if len(sys.argv) > 3 else ''
    for i in z.infolist():
        if flt in i.filename: print(i.file_size, i.filename)
else:
    out = sys.argv[3]; subs = sys.argv[4:]
    for i in z.infolist():
        if i.is_dir() or not any(s in i.filename for s in subs): continue
        name = os.path.normpath(i.filename)
        if name.startswith('..') or os.path.isabs(name): continue
        dst = os.path.join(out, name); os.makedirs(os.path.dirname(dst), exist_ok=True)
        with z.open(i) as src, open(dst, 'wb') as o: o.write(src.read())
        print('got', i.file_size, name)
