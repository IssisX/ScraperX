# Labelled strip: strip.py OUT CROP LABEL1=file1 LABEL2=file2 ...  (center crops, optional @x,y offset)
import sys
from PIL import Image, ImageDraw, ImageFont
out = sys.argv[1]; c = int(sys.argv[2])
items = [a.split('=', 1) for a in sys.argv[3:]]
try:
    font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 18)
except Exception:
    font = ImageFont.load_default()
tiles = []
for lab, f in items:
    off = (0, 0)
    if '@' in f: f, o = f.split('@'); off = tuple(int(v) for v in o.split(','))
    im = Image.open(f).convert('RGB'); w, h = im.size
    if w < c or h < c:
        im = im.resize((c, c), Image.LANCZOS); w, h = c, c
    cx, cy = w // 2 + off[0], h // 2 + off[1]
    t = im.crop((cx - c // 2, cy - c // 2, cx + c // 2, cy + c // 2))
    d = ImageDraw.Draw(t); d.rectangle((0, c - 26, c, c), fill=(0, 0, 0)); d.text((6, c - 23), lab, fill=(255, 230, 120), font=font)
    tiles.append(t)
W = Image.new('RGB', (c * len(tiles), c))
for i, t in enumerate(tiles): W.paste(t, (i * c, 0))
W.save(out)
print(out, W.size)
