# The Matron's redesign: prototypes and panel results

## In the game now (2026-10-20)

Her figure in the game is built from this work by
`tools/body/build_matron_body.py` (run `tools/body/fetch_makehuman.py` first;
every input is CC0 and pinned by SHA-256 in `tools/body/sources.json`). It
writes `godot/presentation/life/matron/`, which the game reads at runtime:

- MakeHuman's scanned body at 77 years, 1.80 m and 81 kg (BMI 25): a soft
  low belly, a heavy low bust, thick upper arms, big hands (23 cm) with
  swollen knuckles.
- Skin from MakeHuman's CC0 old woman, regraded pale and bloodless, with age
  spots, veins on the hands and temples, grime in the hands' creases,
  yellowed nails with dirt under them, and wrinkle relief from the skin's own
  creases.
- The uniform made from the body, gone foul: stains (old blood spattered
  down the front, tea-colored fades, grime, sweat at the armpits, her hands'
  smears down her thighs), soot up from a torn hem with threads hanging, a
  ragged sleeve torn half up one arm, two buttons gone, and dark stockings
  laddered and holed to the skin.
- Long, thin, grey hair grown from her scalp as about 1,500 lank strands in
  greasy clumps, combed back toward her nape with loose locks by her face,
  hung on twenty chains of bones that swing with her (`matron_rig.gd`), each
  lock with its own weight and give, so the hair moves in clumps, not as one
  piece, and lifts a little in slow gusts.
- A dull grey-green uniform, so her pale skin, her off-white collar and her
  grey hair stand apart from it.
- Real eyes (MakeHuman's CC0 high-poly eyes, a washed-out grey iris, a
  yellowed bloodshot white, a clear cornea) on their own bones that turn
  onto the player's eyes, darting until she has found you.
- Teeth on her head and jaw, a dark mouth behind them, brows and lashes.

`godot/tests/matron_figure_test.gd` checks that she loads and is dressed,
that her soles stand on the floor, that her eyes meet a viewer within 5
degrees, and that her hair hangs and swings; with `--out=<dir>` it saves a
studio render of every pose. The nurse's cap is left off for now (it read as
a slab over her hair).

What works, in the owner's words (2026-10-20): the sight of her hanging,
grotesque. First: *"to see her ACTUALLY climbing/hanging able to do monkey bar
type moves or other usually parkour moves I have to say that the
instantaneous sight of me seeing the matron hanging and climbing freaked me
out!"*. Then, correcting a reading of that as "her climbing": *"it is the
sight of seeing a grotesque old scary lady hanging. Its not just that shes
climbing, its mostly the way shes contorting or looking grotesque or scary
some other way. It shocks/stuns and scares/freaks humans out to see this for
many reasons but it also hits intelligence centers in the brain not just fear
centers (the way we want it!)"*. Of her look: *"you are definitely on the
right track and seem to understand the type of scary in looking for"*. When
her work resumes, how she looks as she hangs and contorts stays central, more
than the climbing itself: a sight that works on the mind, not only the
nerves.

Kept here so none of it is lost when a session's scratch space goes away
(the owner, 2026-10-20: "PLEASE DON'T LOSE THAT"). This is design work, not
shipped code: nothing under this folder is loaded by the game or built by CI.

## Shared survival: the bay (2026-10-21)

Her one encounter built since: the bay on the 154 m deck, where she holds a
failing walkway leaf up on her palms as bait. As built, with its numbers and
tests: `shared_survival/THE_BAY.md`. The other agent's native core it is built
on, and that agent's own records: `shared_survival/HANDOFF.md`,
`shared_survival/TEST_RESULTS.md`, `shared_survival/PLAN_2026-10-21.md`.

## What is here

> **Archived 2026-10-29:** `panel_results/`, `prototypes/`, `routecraft_evidence/` and the
> shared-survival plan and test log below are out of the tree. Every file, what it was and how
> to get it back is in `03_EXECUTION/ARCHIVE_INDEX.md`.

- `panel_results/` holds what the design workflows returned, verbatim.
  - `wf_a3b89fc2-b78.json` is the redesign's prototype tracks. The body track
    and the technique track finished. The skin, face and eyes track, the scares
    track and the voices track were cut off by the session limit on
    2026-10-20. Their files so far are under `prototypes/`.
  - `wf_81dc0f0c-283.json` is the earlier planning and design panels.
  - `wf_54a11091-1dd.json` is the review of commit c780427 (the knockdown and
    the throw), whose confirmed findings are fixed in 748e1e1.
  - `face_designs.json` holds the three face proposals.
  - `plan_result.json` is the planning workflow's plan.
- `prototypes/body/` is her body from MakeHuman's CC0 scanned body. It covers
  the age, weight and height choices with the measured lineup, a uniform
  generated from the body, a 59-bone rig driven by `matron_view.gd`'s existing
  pivots, and a Godot probe that renders every gallery pose.
  - `tools/build_matron_body.py` builds it. Its sources are pinned by commit
    and SHA-256 in `tools/sources.json`.
  - `out2/matron.glb` is the last build. `renders/` holds the contact sheets.
- `prototypes/elite_tech/` holds the technique probes in the game's own
  renderer (gl_compatibility): skin with a pre-integrated scatter LUT, wet
  eyes with parallax, hair cards with anisotropic light, cloth sheen, micro
  motion (breath, pulse, blink), LODs and fake contact shadow. The shaders are
  in `probe/shaders/` and the builders in `tools/`. `sources.json` lists every
  outside source with its license.
- `prototypes/skin_face_eyes/` is the partial skin, face and eyes
  look-development (two passes, `tools/` and `tools2/`, `probe/` and
  `probe2/`), with its render sheets.
- `prototypes/fear/` is the partial scares track: a fear director probe, its
  bench, and look renders from the pipe and the ladder.
- `prototypes/face_feel/` is the first face check: MakeHuman's head, young and
  aged by its CC0 caucasian-female-old target, rendered side by side
  (`compare.png`) with a pre-integrated skin LUT (`face_feel_lut.py`).
- The shared MakeHuman machinery the in-game bodies are built with is
  `tools/body/` (fetch pinned and checked, rig, GLB writer).

## Left out, and why

- `dl/` folders: downloaded source data. Every tool fetches its sources again,
  pinned and checked.
- The skin track's `work/` and `work2/`: intermediate bakes, about 650 MB, that
  its tools regenerate.
- Single-frame render folders. The contact sheets and the finals are kept, as
  JPEG at 1600 px or less.

## Licenses

Only CC0, public domain, CC-BY (with attribution recorded), MIT, Apache-2.0
and BSD sources are used. Each track's `sources.json` records the URL and the
license of everything it took. MakeHuman's program code (AGPL) is not used,
only its CC0 data files. The tools read those files themselves.
