Environment plan job 8 (`03_EXECUTION/ENVIRONMENT/ENVIRONMENT_FIDELITY_PLAN.md`), built on job 6. Look only: no geometry, no collision, no solids change. Stacked on job 6: job 8 is `27f1963` and `9e6ba9e`, with job 6's last commit merged in (`8812d43`).

**Proof:** run 41551219784 on `claude/integrate-env-lane-b-job8-20261102` at `9e6ba9e` (`27f1963` was green on every job in run 41547401190). Rule check: `0 BLOCK, 0 REVIEW`.

## What it does
Things sit on things, the same under either renderer.
- **Solids from the parts themselves.** Every part the batcher lays is entered as a box in a spatial hash (`Occlusion`: 2 m cells, coarse 64 m cells for anything over 48 m). The dressing's solids are shared with the kit's statics, so kit parts are shaded by the dressing too.
- **Per-vertex darkening.** Each near vertex of a box part is darkened by the solids within 1.2 m of it. The nearer they are and the more they face it, the darker. The vertex looks from 0.12 m off its own face, so:
  - a deck under a post's foot is full contact;
  - a flush neighbour in the same plane (two deck plates edge to edge) barely shades, so plate seams stay clean.
- **Dirt and rust bleed at feet.** Where the occluding solid lies beneath the vertex (a floor), the colour is tinted toward dirt and rust bleed. The floors are found from the solids, never per place.
- **Shading stays local.** A vertex's shading spreads across its face to the next vertex, so on a face whose vertices lie more than 1.5 m apart it is scaled down by that ratio. A corner's contact must not dim a whole deck or beam. The first version did, and it was caught at S5 (below).
- **Rings on standing members.** A post, column or leg touching a solid at an end is laid with up to 4 rings along its length, so the foot darkens and a metre up does not.
- **Cost:** baked once into the vertex colours. No shader, no draw call, nothing per frame.
- **Not shaded:** round parts (pipes, rails, drums: about 100 vertices each, for start-up time), parts over 48 m, the far batches and moving bodies.

## Finish facts
- **SSAO off, post foot vs 1 m up** (the 154 m deck's yellow post, solid 2301, rendered pixels on its camera-facing face, 3×3 mean luma):
  - **after: 57.1 at 0.1 m vs 88.3 at 1 m, so the foot is 35% darker;**
  - before (job 6, same camera, SSAO off): 92.9 vs 96.9, 4% darker.
  - Before with SSAO on: 88.0 vs 97.8, 10%.
- **Start-up:** `SCRAPERX_BATCH` now logs the shading time (`shading N ms, M ringed`). Batching rises about **0.45 s** on this container's CPU (headless: about 1.3 → 1.75 s). **Budget: +0.5 s here.** The phone's figure is in its log.
- **Draw calls:** S1 2221 → 2220. Texture memory unchanged.
- **Shade on top of #94** (lane A's finding), S5, the same boxes:
  - shaded deck: job 2 25.8, job 6 26.8, **job 8 26.8**;
  - beam underside: 9.8, 12.6, **12.5**.
  - The first version of job 8 read 22.0 and 7.4, because corner contact spread across wide faces. `9e6ba9e` fixed it.

## Pictures (before = job 6 left, after right; all taken on `9e6ba9e`)
![close](https://raw.githubusercontent.com/IssisX/ScraperX/claude/env-lane-b-pictures/03_EXECUTION/ENVIRONMENT/lane_b_job8/S4_close_ssao_off_before_left_after_right.jpg)
![S5](https://raw.githubusercontent.com/IssisX/ScraperX/claude/env-lane-b-pictures/03_EXECUTION/ENVIRONMENT/lane_b_job8/S5_before_left_after_right.jpg)
![S4 SSAO off](https://raw.githubusercontent.com/IssisX/ScraperX/claude/env-lane-b-pictures/03_EXECUTION/ENVIRONMENT/lane_b_job8/S4_ssao_off_before_left_after_right.jpg)
![S4](https://raw.githubusercontent.com/IssisX/ScraperX/claude/env-lane-b-pictures/03_EXECUTION/ENVIRONMENT/lane_b_job8/S4_before_left_after_right.jpg)
![S9](https://raw.githubusercontent.com/IssisX/ScraperX/claude/env-lane-b-pictures/03_EXECUTION/ENVIRONMENT/lane_b_job8/S9_before_left_after_right.jpg)
![S1](https://raw.githubusercontent.com/IssisX/ScraperX/claude/env-lane-b-pictures/03_EXECUTION/ENVIRONMENT/lane_b_job8/S1_before_left_after_right.jpg)

## Left open
- **Wide faces carry only their corners.** A big deck has no vertex where a post stands in its middle, so the deck around the post's foot is not darkened; the post's own foot is. The same goes for long walls under a ledge. Subdividing floors near contacts would fix it, at more start-up time.
- **The dressing is shaded before the kit exists** (`main.gd` batches the dressing before `_build_kit`), so dressing parts are not darkened by kit parts. Kit parts are darkened by both. Fixing it means moving one line in `main.gd`, which is not lane B's.
- **The renderer A/B (Mobile) was not run here.** The shading is baked into the vertex colours, so it does not depend on the renderer.

## Tests
Presentation only. The game was run, the spots were looked at, and there were no script errors in the log. No native group and no touch scenario reads vertex colours.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_01W3SSN2y21YaG5pMxBbRbAe
