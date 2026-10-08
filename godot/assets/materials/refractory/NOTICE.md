# Refractory surface textures derived from Worn Brick Floor

Author: Amal Kumar. Publisher: Poly Haven.

Source: https://polyhaven.com/a/worn_brick_floor

License: CC0 1.0 Universal — https://creativecommons.org/publicdomain/zero/1.0/
Publisher license statement: https://polyhaven.com/license

The source diffuse, OpenGL normal and roughness JPEGs are unchanged 4K
downloads, each 4096 × 4096 pixels with 8-bit channels. The folder identifies
their intended game material; the upstream asset is Worn Brick Floor, a worn
clay surface reference rather than a calibrated refractory material.

Only two derived 768 × 512 PNG atlases are shipped. Six actual mortar-free
interiors are copied at their native pixel resolution without resampling,
colour adjustment or normal changes. Each occupies its own 256 × 256 cell;
nearest edge/corner texels fill at least 32 pixels of gutters. Diffuse RGB and
linear roughness in alpha share `brick_colour_roughness.png`. The OpenGL normal
RGB occupies `brick_normal_gl.png`. This packing uses two texture samplers.

`provenance.json` records the source URLs, sizes and SHA-256 digests separately
from the derived files, together with exact crop and atlas rectangles. Original
JPEGs are not retained in the game. To reproduce, download those three files
into a scratch directory and run `python build_atlas.py /path/to/scratch` with
Pillow 12.3.0. The builder checks each original's recorded digest.

The shader projects in the authored body frame, keeps crop aspect/texel scale,
and limits atlas mip LOD to 4 so filtering remains inside each crop's gutters.
Mirrored source samples supply fine detail, with restrained broad colour and
roughness variation and a 0.20 normal strength. Native geometry and outward
normals remain authoritative. Continuity across complementary welded halves
requires the same material key/frame, including after separation. No displacement,
crack propagation or inferred fresh-cut face mask is supplied by this material.

The two atlases total approximately 4 MiB including mipmaps if stored as
uncompressed RGBA8; platform import/compression can alter that allocation.
Desktop rendering does not establish phone performance or final visual quality.
