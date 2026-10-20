"""Contact sheet: python3 -I sheet.py OUT.png COLS SIZE img1 label1 img2 label2 ..."""
import sys
from PIL import Image, ImageDraw
out, cols, size = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
items = list(zip(sys.argv[4::2], sys.argv[5::2]))
rows = (len(items) + cols - 1) // cols
sheet = Image.new("RGB", (cols * size, rows * (size + 22)), (12, 12, 14))
d = ImageDraw.Draw(sheet)
for k, (p, lab) in enumerate(items):
    im = Image.open(p).convert("RGB")
    w, h = im.size
    s = min(w, h)
    im = im.crop(((w - s) // 2, (h - s) // 2, (w - s) // 2 + s, (h - s) // 2 + s)).resize((size, size), Image.LANCZOS)
    x, y = (k % cols) * size, (k // cols) * (size + 22)
    sheet.paste(im, (x, y + 22))
    d.text((x + 6, y + 5), lab, fill=(230, 230, 220))
sheet.save(out)
print(out, sheet.size)
