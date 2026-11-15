# The Matron's body and maps: credits

Built by `tools/body/build_matron_body.py` (run it to rebuild them) from
MakeHuman's CC0 1.0 scanned body: the hm08 base mesh, macro and measure
targets, its default skeleton and weights, the face-age targets, and from
its system asset pack the old woman's skin, the long01 hair, eyebrow010,
eyelashes02, the high-poly eyes and the base teeth. Every input is pinned
by SHA-256 in `tools/body/sources.json` (MakeHuman,
`github.com/makehumancommunity/makehuman`, commit
`a8bc2d54ff0ac92e78ff71431b1023eda42bf482`). No MakeHuman program code
(AGPL) is used: the formats are read by the build script.

The build also writes `matron_rig.json` and `matron_stats.json` (text, not
shipped binary assets).

| File | What it is | Source | Author / generator | Licence |
|---|---|---|---|---|
| `matron.glb` | Her body, skinned, one mesh per part | MakeHuman hm08 + targets, via `build_matron_body.py` | MakeHuman | CC0 1.0 |
| `skin_a.jpg` | Skin albedo, MakeHuman UV layout | MakeHuman old woman's skin, via `build_matron_body.py` | MakeHuman | CC0 1.0 |
| `skin_h.png` | Wrinkle height, MakeHuman UV layout | MakeHuman old woman's skin, via `build_matron_body.py` | MakeHuman | CC0 1.0 |
| `hair_a.png` | Hair albedo (long01) | MakeHuman system asset pack, via `build_matron_body.py` | MakeHuman | CC0 1.0 |
| `eye_a.jpg` | Eye albedo (high-poly eyes) | MakeHuman system asset pack, via `build_matron_body.py` | MakeHuman | CC0 1.0 |
| `teeth_a.jpg` | Teeth albedo (base teeth) | MakeHuman system asset pack, via `build_matron_body.py` | MakeHuman | CC0 1.0 |
| `brow_a.png` | Eyebrow albedo (eyebrow010) | MakeHuman system asset pack, via `build_matron_body.py` | MakeHuman | CC0 1.0 |
| `lash_a.png` | Eyelash albedo (eyelashes02) | MakeHuman system asset pack, via `build_matron_body.py` | MakeHuman | CC0 1.0 |
| `cloth_n.png` | Cloth normal map (uniform) | MakeHuman system asset pack, via `build_matron_body.py` | MakeHuman | CC0 1.0 |
| `knit_n.png` | Knit normal map | MakeHuman system asset pack, via `build_matron_body.py` | MakeHuman | CC0 1.0 |
| `hair_seat.bin` | One seating of the hair mesh: each vertex follows one bone, and the vertices that were inside the cloth are moved out along that cloth's normal (loaded by `matron_view.gd` `_seat_hair`) | Measured from the drawn body; committed 2026-10-31 (`bffb8b38a`) with its loader. **No generating script is in the repo** — the generator is not on record; tracked on #448. | Project | Project-owned, no outside source |

