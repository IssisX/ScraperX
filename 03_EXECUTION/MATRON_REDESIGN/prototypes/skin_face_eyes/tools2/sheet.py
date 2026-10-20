# usage: sheet.py OUT.png W CROP(x0,y0,x1,y1 in 0..1 or 'full') img1[:label] img2[:label] ...
import sys
from PIL import Image, ImageDraw
out, W, crop = sys.argv[1], int(sys.argv[2]), sys.argv[3]
tiles = []
for a in sys.argv[4:]:
    p, _, lab = a.partition('::')
    im = Image.open(p).convert('RGB')
    if crop != 'full':
        x0, y0, x1, y1 = [float(v) for v in crop.split(',')]
        w, h = im.size; im = im.crop((int(x0 * w), int(y0 * h), int(x1 * w), int(y1 * h)))
    im = im.resize((W, int(W * im.size[1] / im.size[0])), Image.LANCZOS)
    d = ImageDraw.Draw(im)
    if lab:
        d.rectangle((0, im.size[1] - 16, W, im.size[1]), fill=(0, 0, 0)); d.text((4, im.size[1] - 14), lab, fill=(255, 225, 90))
    tiles.append(im)
H = max(t.size[1] for t in tiles)
S = Image.new('RGB', (W * len(tiles), H))
for i, t in enumerate(tiles): S.paste(t, (i * W, 0))
S.save(out)
print(out, S.size)
