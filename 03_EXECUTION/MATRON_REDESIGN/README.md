# The Matron's redesign: prototypes and panel results

Kept here so none of it is lost when a session's scratch space goes away
(the owner, 2026-10-20: "PLEASE DON'T LOSE THAT"). This is design work, not
shipped code: nothing under this folder is loaded by the game or built by CI.

## What is here

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
