# The structure's surface maps: credits

Every map here is made in code by `tools/textures/gen_surfaces.py`
from seeded noise: no photograph, no outside source, so nothing to
licence. The same seeds give the same maps, byte for byte. Each map
tiles in both directions and covers one tile in metres; the batcher
lays broad variation over tens of metres in its vertex colours
(`main_geometry.gd` `Batcher.broad_tint`), so a tile never shows its
repeat at 50 m.

| File | Surface | Map |
|---|---|---|
| `concrete_albedo.png` | poured concrete | albedo (RGB linear multiplier on the part's own colour) |
| `concrete_normal.png` | poured concrete | tangent-space normal map, +Y up the image |
| `concrete_rough.png` | poured concrete | roughness (L linear multiplier) |
| `galvanised_albedo.png` | galvanised steel | albedo (RGB linear multiplier on the part's own colour) |
| `galvanised_normal.png` | galvanised steel | tangent-space normal map, +Y up the image |
| `galvanised_rough.png` | galvanised steel | roughness (L linear multiplier) |
| `mill_scale_albedo.png` | mill-scale steel | albedo (RGB linear multiplier on the part's own colour) |
| `mill_scale_normal.png` | mill-scale steel | tangent-space normal map, +Y up the image |
| `mill_scale_rough.png` | mill-scale steel | roughness (L linear multiplier) |
| `paint_albedo.png` | painted steel | albedo (RGB linear multiplier on the part's own colour) |
| `paint_normal.png` | painted steel | tangent-space normal map, +Y up the image |
| `paint_rough.png` | painted steel | roughness (L linear multiplier) |
| `rust_albedo.png` | rusted steel | albedo (RGB linear multiplier on the part's own colour) |
| `rust_normal.png` | rusted steel | tangent-space normal map, +Y up the image |
| `rust_rough.png` | rusted steel | roughness (L linear multiplier) |
| `timber_albedo.png` | timber | albedo (RGB linear multiplier on the part's own colour) |
| `timber_normal.png` | timber | tangent-space normal map, +Y up the image |
| `timber_rough.png` | timber | roughness (L linear multiplier) |
