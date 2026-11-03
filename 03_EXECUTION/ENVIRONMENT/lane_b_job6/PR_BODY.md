Environment plan job 6 (`03_EXECUTION/ENVIRONMENT/ENVIRONMENT_FIDELITY_PLAN.md`), built on job 2 (#94). Look only: no geometry, no collision, no solids change.

**Proof:** run 41551175000 on `claude/integrate-env-lane-b-job6-20261102` at `beefac6` (the first commit `ab7be1c` was green on every job in run 41545796116). Rule check: `0 BLOCK, 0 REVIEW` on both commits.

## What changed
- **Six real surfaces**: rust, concrete, paint, galvanising, mill scale and timber. Each has a colour, a roughness and a relief map at a known size: 512 texels a metre on the steel, so they hold up at arm's length. Before, every surface was one 256 px noise normal map over 4.5 m (57 texels a metre).
  - Rust: fine grain, pits, black magnetite specks, lifted flakes of brighter oxide.
  - Concrete: sand, aggregate flecks, bugholes, slow mottle.
  - Paint: orange peel, settled dirt, small clustered chips down to dark steel, rough where the gloss is gone.
  - Galvanising: spangle crystals, each tilted a little, so the sky breaks up across it; zinc carbonate blotches.
  - Mill scale: crazed skin, lifted in patches where rust has got under it.
  - Timber: growth rings and fibre running along the plank, raised grain, drying checks, sun-greyed streaks.
- **Made in code** (`tools/textures/gen_surfaces.py`, numpy only, seeded, byte for byte). No outside source, so nothing goes on the licences page.
- **Value ladder kept.** Each colour map's linear mean is held at 0.723, the mean of the grime map it replaces, so job 2's measured values still hold.
- **Batches unchanged.** The colour map multiplies the part's own colour (still in the vertices), so there is still one material per finish.
- **Mapped along each part, not triplanar.** Batched parts get UVs in metres along their own axes (`Batcher.surface_frame`), with the u axis along the face's longer side:
  - timber grain runs along each plank, and each plank gets its own place in the map;
  - a brace's texture runs along the brace;
  - one texture read per map instead of three, which is cheaper on the phone.
  - Tangents follow Godot's own convention (checked against a `BoxMesh`: tangent = dP/du, binormal = −dP/dv).
- **No repeat at 50 m.** Broad variation is laid in the vertex colours (`Batcher.broad_tint`): each part a shade off its neighbours, plus a slow drift of value over about 18 m.
- **Unbatched parts** (moving bodies, loose stock, the summit frame) are mapped triplanar in object space, so a surface rides with its body.
- **Not changed:** asphalt (`yard_ground.gd`) and rock (`mountain_mesh.gd`) are lane C's; the meadow and the climber's arm materials keep their old maps.

## Shade on top of #94 (lane A's finding: shaded steel decks near black)
S5 (221 m ring, out), mean luma of the same boxes, HIGH, 10:30:
- **Shaded mill-scale deck: 25.8 with job 2 → 26.8 with job 6.**
- Shaded beam underside: 9.8 → 12.6.
- Without job 2's sky reflections, lane A measured this deck at 1.45.

At S5 the mill scale's lifted patches repeated every metre across the big deck and read as camouflage in its sky reflection. `beefac6` makes them smaller and sparser. The S5 picture is after that fix; the other pictures were taken before it, and they show mill scale only as dark beams.

## Numbers (local, Godot MCP runtime, HIGH, 10:30)
- **Draw calls:** S1 2220 → 2221, S3 300 → 300. Batch counts unchanged (dressing 423, kit statics 289).
- **Texture memory:** S3 119.8 → 121.7 MB, S1 119.8 → 123.8 MB. **Budget: +6 MB.**
- **Phone (ETC2) import:** about 0.7 MB a surface, 4.2 MB for all six.
- **APK artifact:** 41.5 → 44.7 MB (+3.2 MB; runs 41509320097 and 41545796116).
- **Start-up:** batching rises about 0.3 s on this container's CPU (headless, three runs each alternated: about 0.95 → 1.3 s). The cost is the per-vertex UVs. The phone's own figure is now in the log as `SCRAPERX_BATCH ... ms`.

## Pictures (before left, after right)
![close](https://raw.githubusercontent.com/IssisX/ScraperX/claude/env-lane-b-pictures/03_EXECUTION/ENVIRONMENT/lane_b_job6/S4_close_before_left_after_right.jpg)
![S5](https://raw.githubusercontent.com/IssisX/ScraperX/claude/env-lane-b-pictures/03_EXECUTION/ENVIRONMENT/lane_b_job6/S5_before_left_after_right.jpg)
![S4](https://raw.githubusercontent.com/IssisX/ScraperX/claude/env-lane-b-pictures/03_EXECUTION/ENVIRONMENT/lane_b_job6/S4_before_left_after_right.jpg)
![S12](https://raw.githubusercontent.com/IssisX/ScraperX/claude/env-lane-b-pictures/03_EXECUTION/ENVIRONMENT/lane_b_job6/S12_before_left_after_right.jpg)
![S7](https://raw.githubusercontent.com/IssisX/ScraperX/claude/env-lane-b-pictures/03_EXECUTION/ENVIRONMENT/lane_b_job6/S7_before_left_after_right.jpg)
![S2](https://raw.githubusercontent.com/IssisX/ScraperX/claude/env-lane-b-pictures/03_EXECUTION/ENVIRONMENT/lane_b_job6/S2_before_left_after_right.jpg)
![S1](https://raw.githubusercontent.com/IssisX/ScraperX/claude/env-lane-b-pictures/03_EXECUTION/ENVIRONMENT/lane_b_job6/S1_before_left_after_right.jpg)

(The S4 "after" caught a LOADING label and the arms from a continued save. The view is the same.)

## Left open
- **The summit deck** is unbatched (the summit frame is drawn at any distance), so it gets no broad variation. It now reads as fine rust plate, not carpet, but evenly. Its salmon cast is value and exposure (job 2 and lane A).
- **Timber still reads warm and new under this sun.** The palette colour and the exposure set that.
- **Start-up +0.3 s.** It could be cut by caching the frames, or by moving batching into the native layer, if his phone shows it.

## Tests
Presentation only. Every spot was looked at, with no script errors in the log. No native group and no touch scenario reads material values or UVs. The solids export runs before batching and is untouched.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_01W3SSN2y21YaG5pMxBbRbAe
