# usage: sheet.py OUT.png crop(px) img1 img2 ...   (center crop each, tile horizontally)
import sys
from PIL import Image
out = sys.argv[1]; c = int(sys.argv[2]); fs = sys.argv[3:]
ims = []
for f in fs:
    im = Image.open(f).convert('RGB'); w, h = im.size
    cx, cy = w // 2, h // 2
    ims.append(im.crop((cx - c // 2, cy - c // 2, cx + c // 2, cy + c // 2)))
W = Image.new('RGB', (c * len(ims), c))
for i, im in enumerate(ims): W.paste(im, (i * c, 0))
W.save(out)
