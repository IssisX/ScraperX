# The Matron as drawn: probe and scorer

Measures her final drawn body (every skinned part as posed, her hair as the
renderer draws it), not her control points or collision proxies.

## Record (headless, about 3.5 minutes for touch_matron_act at 60 fps)

    tools/godot/godot --headless --fixed-fps 60 --path godot \
        -s "$PWD/tools/matron_probe/probe.gd" -- --uitest=touch_matron_act --probe-out=<dir>

Any touch scenario works; use `--fixed-fps 20` or `30` for the frame-rate
comparison. `--probe-seconds=<s>` stops early. The scenario plays exactly as
in CI (touch_matron_act passes with the probe attached). Output in `<dir>`:
`meta.json`, `mesh.bin`, `frames.csv`, `bones.bin` (about 10 KB a drawn frame).

## Score (numpy; a C compiler makes it about 10x faster)

    python3 tools/matron_probe/score.py <dir> [--stride N] [--records a:b] [--no-replay] [--no-still]
    python3 tools/matron_probe/score.py compare <dir60> <dir30> <dir20>

Writes `<dir>/score/summary.txt`, `frames.csv`, `hair_episodes.csv`,
`skirt_episodes.csv`. About 0.12 s a scored frame: score a window with
`--records` or every Nth frame with `--stride` for a quick look.

How her drawn geometry is computed: the probe dumps each part's vertices,
weights and skin bind poses as the game loaded them, and each frame's posed
bone transforms read after every view pass. The scorer skins on the CPU the way
the renderer does (linear blend: skeleton world x bone pose x bind pose x
vertex). Inside her body: a point behind the nearest drawn vertex's outward
normal by more than 3 mm, not already inside in her bind pose.

Hair causes: "outside the proxies", "segment crossing", "lag" (gone when her
pose is held still and the hair settles, or without the 1/30 s step cap) and
"rest direction" (gone when a copy of `step_hair` pulls toward world down).
The copy is checked against the recording one step at a time (about 4e-5 m,
the game's float32 rounding).

## Status (2026-10-28)

Unfinished: the skirt's baseline is her bind pose (an A-pose), so its numbers
still include layering by design; the "rigid on the head" hair hits are not yet
checked by eye; plants, grips and joints are scored but not yet reviewed. The
footage tool (`tools/matron_film`) is not built yet.
