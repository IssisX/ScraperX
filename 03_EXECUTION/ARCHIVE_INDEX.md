# Archive index

What left the tree in the documentation cull of 2026-10-29 (313 files, 18,868,546 bytes), and how to get any of it back. Nothing here is lost: every file is in git history.

**Get a file back:** `git show 46e65e2a:<path> > <path>`. For a whole folder: `git checkout 46e65e2a -- <folder>`. `46e65e2a` (`ScraperX-Claude`, 2026-10-29) is the last commit that has every file below; the Back column repeats it on each line.

**Why archived:**
- **output**: machine output (a workflow's verbatim JSON, a generated report or map, a render, model, texture or log). What mattered in it is in the kept document the row names, or a tool makes it again.
- **evidence**: a dated build or test record. CI and the tests are the proof now.
- **handoff**: a dated handoff or plan whose work is in the code and tests, or in the kept document named.
- **draft**: prototype code or a proposal that the game's own code or a kept design replaced. Nothing loads, builds or runs it.

## Documents, logs and workflow output

| Path | Bytes | What it was | Why archived | Back |
|---|---|---|---|---|
| `03_EXECUTION/CLIMB_REDESIGN/panel_results/wf_047fa3a0-b28_partial.json` | 86,068 | glass spares tube design workflow, stopped partway: two maps and one design, verbatim | output; its designs and every idea are in `CLIMB_REDESIGN/glass_tube_designs.md` | `46e65e2a` |
| `03_EXECUTION/CLIMB_REDESIGN/panel_results/wf_de938b84-896.json` | 210,452 | chiller drop design workflow: route map, three designs, judge's scores, verbatim | output; every design and idea is in `CLIMB_REDESIGN/README.md`, the spec in `chiller_drop_spec.md` | `46e65e2a` |
| `03_EXECUTION/ENGINEERING/TWIN_FIRST_CONTACT.md` | 5,391 | handoff of `dot/twin-tower-first-contact` (2026-10-21): the second shaft moved 66 m | handoff; taken whole, as built in `SECOND_TOWER/BROKEN_BRIDGE.md` and `src/sim/twin_tower.*` | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/CLIMB_CAPABILITIES_REPORT.md` | 14,534 | scanner-versus-inventory report generated 2026-10-22 | output; stale, and `tools/matron/climb_inventory_diff.py` writes a fresh one | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/HEARING_CLIMB_HANDOFF.md` | 9,693 | handoff of the dot hearing and ladder-climb work (2026-10-21) | handoff; picked onto `claude/matron-physics`, now in `matron.cpp` and the matron tests | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/MULTI_DECK_INPUT_GEMINI.md` | 6,899 | an outside multi-deck proposal kept as input (2026-10-21); its code references were not real | draft; replaced by `MULTI_DECK_PROPOSAL.md`, which is kept | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/ROUTECRAFT_HANDOFF.md` | 19,199 | handoff of the dot routecraft work (2026-10-22), a checkpoint before integration | handoff; routecraft is in `src/sim/matron_routecraft.*` and its tests | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/panel_results/face_designs.json` | 78,449 | the three face proposals, verbatim workflow output | output; her look is parked (START_HERE), and the owner can still pick from these three | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/panel_results/plan_result.json` | 639,903 | the Matron planning workflow's plan of 2026-10-19, with its designs and gathered facts, verbatim | output; replaced by START_HERE's dated rows and `handoffs/PARKED_FEATURES_2026-10-29.md`; its story spine is summarized in `ACTIVE_GOAL_PROGRESS.md` | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/panel_results/wf_54a11091-1dd.json` | 48,574 | review of commit c780427 (knockdown and throw), verbatim | output; its findings were fixed in 748e1e1 | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/panel_results/wf_81dc0f0c-283.json` | 638,808 | the earlier Matron planning and design panels (2026-10-19), verbatim | output; replaced by START_HERE's dated rows and the kept Matron designs | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/panel_results/wf_a3b89fc2-b78.json` | 45,722 | the redesign's prototype tracks (body and technique finished, three cut off), verbatim | output; the body track became `tools/body/` | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/routecraft_evidence/acceptance-build.log` | 189 | routecraft acceptance build log (2026-10-22) | evidence | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/routecraft_evidence/baseline-full.log` | 40,189 | routecraft's full native suite baseline log (2026-10-22) | evidence | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/routecraft_evidence/build.log` | 189 | routecraft build log (2026-10-22) | evidence | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/routecraft_evidence/focused.log` | 2,220 | routecraft focused test log (2026-10-22) | evidence | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/routecraft_evidence/matron-grab.log` | 2,966 | routecraft's `matron-grab` group log (2026-10-22) | evidence | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/routecraft_evidence/other-ctest.log` | 321 | routecraft's other ctest entries log (2026-10-22) | evidence | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/routecraft_evidence/player-climb.log` | 2,271 | routecraft's player climb group log (2026-10-22) | evidence | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/routecraft_evidence/verification.txt` | 367 | routecraft verification summary (2026-10-22) | evidence | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/shared_survival/PLAN_2026-10-21.md` | 2,471 | the shared-survival adoption plan, its checklist done | handoff; built as the bay (`shared_survival/THE_BAY.md`) | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/shared_survival/TEST_RESULTS.md` | 4,506 | native verification of `dot/matron-shared-survival` (2026-10-21) | evidence | `46e65e2a` |
| `03_EXECUTION/SECOND_TOWER/verified_map.json` | 167,995 | three mappers' code map with file:line evidence (2026-10-21) | output; its load-bearing facts are in `SECOND_TOWER/PROPOSAL.md`, and its line numbers have moved | `46e65e2a` |

## The Matron's redesign prototypes (290 files)

Design probes from 2026-10-20 (body, technique, skin, face and eyes, scares, face check), kept until now so they would not be lost with a session's scratch space. Nothing in the game, its tests, its tools or CI reads them (`MATRON_REDESIGN/README.md`). The body track became `tools/body/`, which builds her figure into `godot/presentation/life/matron/`.

| Path | Bytes | What it was | Why archived | Back |
|---|---|---|---|---|
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/average_bare_rig.json` | 29,429 | generated rig: average body, undressed | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/average_bare_stats.json` | 1,029 | measured stats: average body, undressed | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/average_rig.json` | 30,140 | generated rig: average body | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/average_stats.json` | 1,681 | measured stats: average body | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/lineup_stats.json` | 1,814 | measured lineup of the five bodies | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/matron_bare_rig.json` | 29,408 | generated rig: matron body, undressed | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/matron_bare_stats.json` | 1,062 | measured stats: matron body, undressed | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/matron_rig.json` | 30,058 | generated rig: matron body | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/matron_stats.json` | 1,715 | measured stats: matron body | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/reference_rig.json` | 29,415 | generated rig: reference body | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/reference_stats.json` | 1,014 | measured stats: reference body | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/stick_bare_rig.json` | 29,377 | generated rig: stick body, undressed | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/stick_bare_stats.json` | 1,024 | measured stats: stick body, undressed | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/stick_rig.json` | 30,039 | generated rig: stick body | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/stick_stats.json` | 1,681 | measured stats: stick body | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/thin_bare_rig.json` | 29,405 | generated rig: thin body, undressed | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/thin_bare_stats.json` | 1,051 | measured stats: thin body, undressed | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/thin_rig.json` | 30,100 | generated rig: thin body | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out/thin_stats.json` | 1,704 | measured stats: thin body | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out2/lineup_stats.json` | 1,814 | measured lineup of the five bodies | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out2/matron.glb` | 1,717,104 | built body model (the README's last build) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out2/matron_rig.json` | 30,058 | generated rig: matron body | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/out2/matron_stats.json` | 1,715 | measured stats: matron body | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/assets/average_bare_rig.json` | 29,429 | generated rig: average body, undressed, the probe's copy | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/assets/average_rig.json` | 30,140 | generated rig: average body, the probe's copy | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/assets/cloth_n.png` | 295,788 | generated texture: cloth normal map | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/assets/hair_a.png` | 175,896 | generated texture: hair albedo | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/assets/knit_n.png` | 48,498 | generated texture: knit normal map | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/assets/matron.glb` | 1,717,104 | built body model, the probe's copy | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/assets/matron_bare.glb` | 1,383,308 | built body model, undressed, the probe's copy | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/assets/matron_bare_rig.json` | 29,408 | generated rig: matron body, undressed, the probe's copy | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/assets/matron_rig.json` | 30,058 | generated rig: matron body, the probe's copy | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/assets/reference_rig.json` | 29,415 | generated rig: reference body, the probe's copy | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/assets/skin_a.png` | 660,340 | generated texture: skin albedo | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/assets/skin_n.png` | 542,611 | generated texture: skin normal map | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/assets/stick_bare_rig.json` | 29,377 | generated rig: stick body, undressed, the probe's copy | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/assets/stick_rig.json` | 30,039 | generated rig: stick body, the probe's copy | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/assets/thin_bare_rig.json` | 29,405 | generated rig: thin body, undressed, the probe's copy | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/assets/thin_rig.json` | 30,100 | generated rig: thin body, the probe's copy | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/main.gd` | 20,191 | Godot probe that renders every gallery pose | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/main.tscn` | 149 | the probe's scene | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/presentation/life/matron_body.gd` | 9,159 | the probe's body script | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/presentation/life/matron_view.gd` | 50,229 | a copy of the game's `matron_view.gd` for the probe | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/project.godot` | 373 | the probe's Godot project | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/probe/rig_adapter.gd` | 9,324 | maps the rig onto the game's pivots | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/calib.log` | 146 | body render log (calib) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/compare.log` | 186 | body render log (compare) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/compare_guard_34.jpg` | 73,176 | body render: compare guard 34 | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/compare_guard_front.jpg` | 80,789 | body render: compare guard front | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/compare_sheet.jpg` | 106,005 | body contact sheet: compare | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/compare_shuffle_34.jpg` | 79,596 | body render: compare shuffle 34 | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/compare_shuffle_front.jpg` | 75,976 | body render: compare shuffle front | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/detail.log` | 0 | body render log (detail), empty | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/detail_back.jpg` | 46,810 | body render: detail back | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/detail_face.jpg` | 95,955 | body render: detail face | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/detail_feet.jpg` | 39,212 | body render: detail feet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/detail_sheet.jpg` | 52,961 | body contact sheet: detail | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/detail_upper_34.jpg` | 53,256 | body render: detail upper 34 | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/iter_sheet.jpg` | 50,402 | body contact sheet: iter | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/lineup.log` | 0 | body render log (lineup), empty | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/lineup_front.jpg` | 103,099 | body render: lineup front | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/lineup_sheet.jpg` | 114,384 | body contact sheet: lineup | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/lineup_side.jpg` | 71,303 | body render: lineup side | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/lod.log` | 1,208 | body render log (lod) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/mood.log` | 0 | body render log (mood), empty | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/mood_crawl.jpg` | 30,694 | body render: mood crawl | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/mood_lookback.jpg` | 49,972 | body render: mood lookback | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/mood_sheet.jpg` | 62,300 | body contact sheet: mood | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/mood_shuffle.jpg` | 30,321 | body render: mood shuffle | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/neck.log` | 0 | body render log (neck), empty | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/neck_sheet.jpg` | 61,992 | body contact sheet: neck | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/none.log` | 0 | body render log (none), empty | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_carry.jpg` | 50,950 | body render: pose carry | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_climb.jpg` | 51,646 | body render: pose climb | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_cling.jpg` | 50,500 | body render: pose cling | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_emerge.jpg` | 47,709 | body render: pose emerge | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_fall.jpg` | 53,393 | body render: pose fall | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_guard.jpg` | 40,196 | body render: pose guard | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_hunt_a.jpg` | 50,078 | body render: pose hunt a | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_hunt_b.jpg` | 50,844 | body render: pose hunt b | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_hunt_still.jpg` | 48,740 | body render: pose hunt still | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_on_line.jpg` | 46,177 | body render: pose on line | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_peer.jpg` | 49,581 | body render: pose peer | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_search.jpg` | 42,558 | body render: pose search | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_shuffle.jpg` | 47,156 | body render: pose shuffle | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_turn_lunge_a.jpg` | 51,200 | body render: pose turn lunge a | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_turn_lunge_b.jpg` | 50,961 | body render: pose turn lunge b | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_turn_vault.jpg` | 54,423 | body render: pose turn vault | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_turn_windup.jpg` | 49,986 | body render: pose turn windup | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_unrig.jpg` | 51,295 | body render: pose unrig | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/pose_zoom.jpg` | 49,896 | body render: pose zoom | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/poses.log` | 4,117 | body render log (poses) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/poses_sheet.jpg` | 118,023 | body contact sheet: poses | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/view_front.jpg` | 59,142 | body render: view front | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/view_side.jpg` | 49,875 | body render: view side | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/view_three_quarter.jpg` | 55,800 | body render: view three quarter | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/views.log` | 78 | body render log (views) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/renders/views_sheet.jpg` | 60,376 | body contact sheet: views | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/tools/build_matron_body.py` | 67,625 | the first body builder (now `tools/body/build_matron_body.py`) | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/tools/dbg1.py` | 611 | debug scrap | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/tools/dbg2.py` | 584 | debug scrap | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/tools/dbg3.py` | 671 | debug scrap | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/tools/dbg4.py` | 786 | debug scrap | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/tools/make_textures.py` | 4,469 | the first texture maker | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/tools/mh_target_paths.txt` | 113,846 | generated list of MakeHuman target paths | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/body/tools/sources.json` | 30,796 | the first build's pinned sources (now `tools/body/sources.json`) | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/data/brows/meta.json` | 27 | generated metadata: brows/meta | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/data/eyes.json` | 271 | generated metadata: eyes | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/data/hair/meta.json` | 29 | generated metadata: hair/meta | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/data/head_meta.json` | 621 | generated metadata: head_meta | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/data/lod0/meta.json` | 165 | generated metadata: lod0/meta | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/data/lod1/meta.json` | 163 | generated metadata: lod1/meta | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/data/lod2/meta.json` | 47 | generated metadata: lod2/meta | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/probe/main.gd` | 34,583 | Godot technique probe (skin, eyes, hair, cloth, LOD) | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/probe/main.tscn` | 151 | the probe's scene | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/probe/project.godot` | 491 | the probe's Godot project | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/probe/shaders/cloth.gdshader` | 1,451 | technique probe shader: cloth | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/probe/shaders/eye.gdshader` | 4,052 | technique probe shader: eye | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/probe/shaders/floor_ao.gdshader` | 1,864 | technique probe shader: floor ao | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/probe/shaders/hair.gdshader` | 3,285 | technique probe shader: hair | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/probe/shaders/haze.gdshader` | 1,102 | technique probe shader: haze | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/probe/shaders/skin.gdshader` | 5,812 | technique probe shader: skin | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/alpha/run.log` | 1,439 | technique probe run log (alpha) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/blend/run.log` | 1,632 | technique probe run log (blend) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/blend_basen/run.log` | 1,632 | technique probe run log (blend_basen) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/blend_sheet.jpg` | 50,667 | technique contact sheet: blend | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/blend_sheet_basen.jpg` | 50,069 | technique contact sheet: blend basen | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/blend_sheet_zeronormals_bug.jpg` | 42,115 | technique contact sheet: blend zeronormals bug | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/blend_zeronormals_bug/run.log` | 1,632 | technique probe run log (blend_zeronormals_bug) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/capsule/run.log` | 1,761 | technique probe run log (capsule) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/cloth/run.log` | 1,532 | technique probe run log (cloth) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/dbg/run.log` | 1,477 | technique probe run log (dbg) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/dbg_sheet.jpg` | 21,593 | technique contact sheet: dbg | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/eyes/run.log` | 1,641 | technique probe run log (eyes) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/eyes_sheet.jpg` | 100,485 | technique contact sheet: eyes | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/hair/run.log` | 2,084 | technique probe run log (hair) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/hair_sheet.jpg` | 101,662 | technique contact sheet: hair | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/hero/hero_changed.jpg` | 64,188 | technique hero render: changed | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/hero/hero_dark.jpg` | 69,567 | technique hero render: dark | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/hero/hero_pest.jpg` | 64,344 | technique hero render: pest | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/hero/run.log` | 1,470 | technique probe run log (hero) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/hero_sheet.jpg` | 70,383 | technique contact sheet: hero | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/lod/run.log` | 1,818 | technique probe run log (lod) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/micro/run.log` | 1,859 | technique probe run log (micro) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/micro_sheet.jpg` | 49,441 | technique contact sheet: micro | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/misc_sheet.jpg` | 69,316 | technique contact sheet: misc | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/projection_sheet.jpg` | 62,962 | technique contact sheet: projection | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/quality/run.log` | 1,858 | technique probe run log (quality) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/rig/run.log` | 1,977 | technique probe run log (rig) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/rig_lod_sheet.jpg` | 85,524 | technique contact sheet: rig lod | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/screen/run.log` | 1,623 | technique probe run log (screen) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/skin/run.log` | 1,922 | technique probe run log (skin) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/skin_sheet.jpg` | 139,575 | technique contact sheet: skin | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/ssao/run.log` | 1,611 | technique probe run log (ssao) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/ssao_sheet.jpg` | 45,056 | technique contact sheet: ssao | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/renders/tf/run.log` | 2,157 | technique probe run log (tf) | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/sources.json` | 3,863 | the technique track's outside sources and licences | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/tools/bake_bs_normals.py` | 1,706 | technique build tool: bake_bs_normals.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/tools/build_extra.py` | 12,348 | technique build tool: build_extra.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/tools/build_head.py` | 15,672 | technique build tool: build_head.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/tools/flux.sh` | 761 | technique build tool: flux.sh | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/tools/project_face.py` | 7,445 | technique build tool: project_face.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/tools/rangezip.py` | 1,321 | technique build tool: rangezip.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/tools/run.sh` | 615 | technique build tool: run.sh | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/elite_tech/tools/sheet.py` | 799 | technique build tool: sheet.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/base_front.png` | 56,714 | face check render: base front | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/base_three.png` | 54,690 | face check render: base three | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/compare.png` | 153,391 | young and aged heads side by side | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/face_feel_export.py` | 1,350 | face check script: export | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/face_feel_head.py` | 1,231 | face check script: head | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/face_feel_lut.py` | 980 | face check script: lut | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/face_feel_render.py` | 1,584 | face check script: render | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/old_front.png` | 48,519 | face check render: old front | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/old_three.png` | 48,691 | face check render: old three | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/probe/main.tscn` | 151 | face probe scene | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/probe/probe.gd` | 1,733 | face probe script | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/probe/probe_strip.png` | 281,371 | face probe render strip | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/probe/project.godot` | 332 | face probe Godot project | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/probe/shot_a.png` | 148,694 | face probe shot a | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/probe/shot_b.png` | 136,720 | face probe shot b | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/probe/shot_c.png` | 138,251 | face probe shot c | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/probe/shot_d.png` | 79,388 | face probe shot d | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/probe/skin.gdshader` | 1,604 | face probe skin shader | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/face_feel/probe/skin_lut.png` | 3,045 | pre-integrated skin LUT texture | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/audio/mimic_help_from_shaft.wav` | 277,358 | scares track audio test: mimic help from shaft | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/audio/radio_echo_34m.wav` | 225,488 | scares track audio test: radio echo 34m | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/audio/your_yell_original.wav` | 177,898 | scares track audio test: your yell original | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/godot_probe/bench_fear_director.gd` | 954 | the fear director's bench | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/godot_probe/fear_director.gd` | 15,415 | fear director probe | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/godot_probe/project.godot` | 57 | the probe's Godot project | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/godot_probe/test_fear_director.gd` | 5,235 | the fear director's test | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/out/act_control.log` | 19,815 | fear probe output: act control | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/out/act_run.log` | 68,500 | fear probe output: act run | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/out/act_run_paced.log` | 62,231 | fear probe output: act run paced | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/out/director_bench.txt` | 49 | fear probe output: director bench | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/out/director_test.log` | 2,900 | fear probe output: director test | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/out/main_feel_hooked.gd` | 57,805 | the game's `main_feel.gd` with probe hooks, generated | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/out/screen_size.txt` | 1,859 | fear probe output: screen size | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/out/shaft_voice.txt` | 924 | fear probe output: shaft voice | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/out/staging.log` | 3,949 | fear probe output: staging | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/probes/screen_size.py` | 923 | fear probe script: screen size | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/probes/shaft_voice.py` | 2,470 | fear probe script: shaft voice | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/renders/look_fear_ladder_below_day.jpg` | 253,223 | fear look render: ladder below day | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/renders/look_fear_ladder_below_dusk.jpg` | 172,421 | fear look render: ladder below dusk | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/renders/look_fear_pipe_peek_day.jpg` | 202,992 | fear look render: pipe peek day | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/renders/look_fear_pipe_peek_dusk.jpg` | 136,523 | fear look render: pipe peek dusk | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/fear/renders/look_fear_pipe_peek_far_day.jpg` | 174,575 | fear look render: pipe peek far day | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe/main.gd` | 13,213 | look-dev probe, pass 1 | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe/main.tscn` | 149 | look-dev probe scene, pass 1 | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe/mesh/eyes.json` | 4,032 | look-dev mesh metadata, pass 1: eyes | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe/project.godot` | 368 | look-dev probe project, pass 1 | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe/shaders/cloth.gdshader` | 1,930 | look-dev shader, pass 1: cloth | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe/shaders/eye.gdshader` | 6,069 | look-dev shader, pass 1: eye | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe/shaders/eye_occlusion.gdshader` | 390 | look-dev shader, pass 1: eye occlusion | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe/shaders/hair_card.gdshader` | 339 | look-dev shader, pass 1: hair card | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe/shaders/hair_strand.gdshader` | 451 | look-dev shader, pass 1: hair strand | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe/shaders/skin.gdshader` | 6,374 | look-dev shader, pass 1: skin | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe/shaders/tear.gdshader` | 326 | look-dev shader, pass 1: tear | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe2/main.gd` | 17,846 | look-dev probe, pass 2 | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe2/main.tscn` | 149 | look-dev probe scene, pass 2 | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe2/mesh/eyes.json` | 4,034 | look-dev mesh metadata, pass 2: eyes | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe2/mesh/hair.json` | 208 | look-dev mesh metadata, pass 2: hair | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe2/project.godot` | 369 | look-dev probe project, pass 2 | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe2/shaders/cloth.gdshader` | 1,930 | look-dev shader, pass 2: cloth | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe2/shaders/cornea.gdshader` | 1,459 | look-dev shader, pass 2: cornea | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe2/shaders/eye.gdshader` | 7,490 | look-dev shader, pass 2: eye | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe2/shaders/eye_occlusion.gdshader` | 390 | look-dev shader, pass 2: eye occlusion | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe2/shaders/hair_card.gdshader` | 641 | look-dev shader, pass 2: hair card | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe2/shaders/hair_shell.gdshader` | 4,513 | look-dev shader, pass 2: hair shell | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe2/shaders/hair_strand.gdshader` | 451 | look-dev shader, pass 2: hair strand | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe2/shaders/post.gdshader` | 1,303 | look-dev shader, pass 2: post | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe2/shaders/skin.gdshader` | 7,427 | look-dev shader, pass 2: skin | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/probe2/shaders/tear.gdshader` | 326 | look-dev shader, pass 2: tear | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/albedo_face_crop.jpg` | 168,211 | look-dev render: albedo face crop | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/albedo_face_crop2.jpg` | 166,001 | look-dev render: albedo face crop2 | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/ao_sheet.jpg` | 92,025 | look-dev render: ao sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/diag_sheet.jpg` | 72,612 | look-dev render: diag sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/eyes17_crop.jpg` | 73,841 | look-dev render: eyes17 crop | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/eyes1_sheet.jpg` | 57,634 | look-dev render: eyes1 sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/eyes2_sheet.jpg` | 55,796 | look-dev render: eyes2 sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/eyes3_sheet.jpg` | 50,946 | look-dev render: eyes3 sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/eyes4_sheet.jpg` | 54,870 | look-dev render: eyes4 sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/eyes5_sheet.jpg` | 41,057 | look-dev render: eyes5 sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/eyes6_sheet.jpg` | 82,854 | look-dev render: eyes6 sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/n_sheet.jpg` | 55,377 | look-dev render: n sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/r1_sheet.jpg` | 71,966 | look-dev render: r1 sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/r2_sheet.jpg` | 74,978 | look-dev render: r2 sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/r3_sheet.jpg` | 85,823 | look-dev render: r3 sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/r4_sheet.jpg` | 74,433 | look-dev render: r4 sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/r5_sheet.jpg` | 70,833 | look-dev render: r5 sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/r6_eyes.jpg` | 51,415 | look-dev render: r6 eyes | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/r6_sheet.jpg` | 88,296 | look-dev render: r6 sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/skins_atlas_compare.jpg` | 151,333 | look-dev render: skins atlas compare | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/tt_real_day045.jpg` | 70,628 | look-dev render: tt real day045 | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/tt_real_night045.jpg` | 70,858 | look-dev render: tt real night045 | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/v2a_sheet.jpg` | 71,203 | look-dev render: v2a sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/v2b_sheet.jpg` | 67,496 | look-dev render: v2b sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/v2c_sheet.jpg` | 62,303 | look-dev render: v2c sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/v2d_sheet.jpg` | 68,567 | look-dev render: v2d sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/v2e_sheet.jpg` | 68,812 | look-dev render: v2e sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/v2g_sheet.jpg` | 71,016 | look-dev render: v2g sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/v2h_sheet.jpg` | 67,243 | look-dev render: v2h sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/v2i_sheet.jpg` | 66,647 | look-dev render: v2i sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/renders/v2j_sheet.jpg` | 66,759 | look-dev render: v2j sheet | output | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools/bake_both.sh` | 407 | look-dev tool, pass 1: bake_both.sh | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools/compose_final.py` | 1,362 | look-dev tool, pass 1: compose_final.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools/geom.py` | 5,094 | look-dev tool, pass 1: geom.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools/httpzip.py` | 1,899 | look-dev tool, pass 1: httpzip.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools/imgops.py` | 4,594 | look-dev tool, pass 1: imgops.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools/mhload.py` | 2,592 | look-dev tool, pass 1: mhload.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools/raster.py` | 7,378 | look-dev tool, pass 1: raster.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools/render.sh` | 410 | look-dev tool, pass 1: render.sh | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools/shape_try.py` | 3,265 | look-dev tool, pass 1: shape_try.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools/sheet.py` | 468 | look-dev tool, pass 1: sheet.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools/skin_lut.py` | 1,565 | look-dev tool, pass 1: skin_lut.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools/stage1_mesh.py` | 2,815 | look-dev tool, pass 1: stage1_mesh.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools/stage2_tex.py` | 15,003 | look-dev tool, pass 1: stage2_tex.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools/stage3_export.py` | 16,185 | look-dev tool, pass 1: stage3_export.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools/strip.py` | 1,051 | look-dev tool, pass 1: strip.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools2/geom.py` | 5,095 | look-dev tool, pass 2: geom.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools2/imgops.py` | 4,594 | look-dev tool, pass 2: imgops.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools2/mhload.py` | 3,453 | look-dev tool, pass 2: mhload.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools2/raster.py` | 7,378 | look-dev tool, pass 2: raster.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools2/render.sh` | 411 | look-dev tool, pass 2: render.sh | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools2/shape_try2.py` | 3,711 | look-dev tool, pass 2: shape_try2.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools2/sheet.py` | 899 | look-dev tool, pass 2: sheet.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools2/skin_lut.py` | 1,565 | look-dev tool, pass 2: skin_lut.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools2/stage1_mesh.py` | 2,816 | look-dev tool, pass 2: stage1_mesh.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools2/stage2_tex.py` | 19,137 | look-dev tool, pass 2: stage2_tex.py | draft | `46e65e2a` |
| `03_EXECUTION/MATRON_REDESIGN/prototypes/skin_face_eyes/tools2/stage3_export.py` | 19,285 | look-dev tool, pass 2: stage3_export.py | draft | `46e65e2a` |

## Kept, but bloated: condense next

Live files left whole in this pass. Each line says what to cut and what must stay.

- `03_EXECUTION/ACTIVE_GOAL_PROGRESS.md` (195,164 B): a progress log of 2026-10-09 to 10-22, but the tree's only copy of owner calls still in force: the Matron's stages and the story spine (2026-10-19, lines 1849-1886), the goal of 2026-10-19 night (1490), his running to-do (2121), the pressure ideas he has not picked (2201) and the decisions waiting on him (2214-2252). Move those to START_HERE rows or one short story document, then archive the log.
- `03_EXECUTION/MATRON_REDESIGN/MULTI_DECK_INVENTORY.md` (156,235 B): its region and edge tables repeat `multi_deck_inventory.json`, the exam key `tools/matron/climb_inventory_diff.py` reads. Keep the runtime results and the UNVERIFIED list; let the JSON carry the rest.
- `03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md` (89,297 B): code and slices cite §§2-3 (the rules and the chain). §§4-12, the as-built cycle contracts and goalsets (about 60 KB), are in code and tests and can come here.
- `03_EXECUTION/KERNEL/` `WO-000` to `WO-013` and `03_EXECUTION/TEMPLATES/KERNEL_WORK_ORDER_TEMPLATE.md` (111,943 B): finished work orders whose content is in code and tests. Kept only because TDD §24, the Execution Protocol, `PLANNING/ASCENT_PRE_RESOLUTION.md` and START_HERE §§4 and 6 name each file. Archive them together once those pointers cite this index (authority edits: the owner's call).
- `03_EXECUTION/ASCENT/AS-002_LEGAL_FORTY.md` (43,951 B), `AS-010_MIDSTACK_SERVICE.md` (39,609 B), `AS-003_HOOK5_RACK.md` (36,267 B), `AS-006_CW_PIN.md` (29,733 B): built slices whose derivations and result records repeat the code. Cut each to its objective, the facts the code cites (AS-003 §8.4.3, for one) and its falsifiers.
- `03_EXECUTION/ASCENT/AS-004_NEEDLE_SEAT.md` (31,334 B) and `AS-005_CAGE_OR_SKIN.md` (15,061 B): PLANNED, not next (the stair stays), and AS-005 still quotes another branch's world. Archive them when START_HERE's ascent ledger and `ASCENT_PRE_RESOLUTION.md` §5 drop them.
- `03_EXECUTION/MATRON_REDESIGN/HER_SOUND_VARIATION_SPEC.md` (33,273 B): the live L6 spec. Once L6 lands, cut it to the rules the code follows and the clip shortlist.
- `03_EXECUTION/CLIMB_REDESIGN/chiller_drop_spec.md` (32,357 B): built and merged 2026-10-21. Cut it to the as-built facts, the deviations and what the code cites.
- `03_EXECUTION/handoffs/INTEGRATOR_HANDOFF_2026-10-28.md` (14,672 B): a dated handoff that still carries the landing order. Archive it when the next handoff or the issue #3 board replaces it, after its CI facts and local tooling notes move to `CLAUDE.md` or `REPO_MAP.md`.
- `03_EXECUTION/MATRON_REDESIGN/shared_survival/HANDOFF.md` (11,563 B): fold its integration contract (ownership, lifetime, threading, what throws) into `src/sim/shared_survival.hpp`, then archive it.
- `03_EXECUTION/MATRON_REDESIGN/README.md` (6,919 B): its "What is here" and "Left out" sections now describe archived files. Keep "In the game now" and the owner's words on her look.

