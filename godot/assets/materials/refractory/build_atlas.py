"""Rebuild the native-resolution atlases from the three credited 4K JPEGs.

Download the source_files URLs in provenance.json into a scratch directory,
then run: python build_atlas.py /path/to/source-directory
Requires Pillow; the original JPEGs are deliberately not shipped in the game.
"""

import hashlib
import json
from pathlib import Path
import sys

from PIL import Image


asset_dir = Path(__file__).resolve().parent
source_dir = Path(sys.argv[1])
provenance = json.loads((asset_dir / "provenance.json").read_text())
sources = {}
for source in provenance["source_files"]:
    path = source_dir / source["file"]
    if hashlib.sha256(path.read_bytes()).hexdigest() != source["sha256"]:
        raise ValueError(f"Source digest mismatch: {path}")
    sources[source["role"]] = Image.open(path).convert("RGB")

colour = Image.new("RGBA", tuple(provenance["atlas_size_px"]))
normal = Image.new("RGB", colour.size)
for crop in provenance["crops"]:
    x, y, width, height = crop["source_rect_px"]
    ax, ay, _, _ = crop["atlas_rect_px"]
    cx, cy = crop["cell_origin_px"]
    bounds = (x, y, x + width, y + height)
    diffuse = sources["diffuse"].crop(bounds)
    roughness = sources["roughness"].crop(bounds).getchannel("R")
    detail_normal = sources["normal_opengl"].crop(bounds)
    packed = diffuse.convert("RGBA")
    packed.putalpha(roughness)
    # Fill the complete cell with nearest-edge texels, with no resampling of
    # the source interior. Corners extend the corresponding corner texel.
    for py in range(cy, cy + 256):
        sy = min(max(py - ay, 0), height - 1)
        for px in range(cx, cx + 256):
            sx = min(max(px - ax, 0), width - 1)
            colour.putpixel((px, py), packed.getpixel((sx, sy)))
            normal.putpixel((px, py), detail_normal.getpixel((sx, sy)))

colour.save(asset_dir / "brick_colour_roughness.png", compress_level=9)
normal.save(asset_dir / "brick_normal_gl.png", compress_level=9)
