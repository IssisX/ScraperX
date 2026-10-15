# Live checkpoint — 2026-10-15: parkour and tower rendering

Repo `/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo`, branch **ChatGPT**, published HEAD `0db3c5dcfe4df28e0f2504bd6052b49fc740e97a`. Preserve all dirty/untracked work. Owner selected current ChatGPT branch, despite original prompt mentioning Claude. No permission/settings/model audits, no repeated accepted tests. No more large wheels.

## Durable scope and new owner requests

Read `OWNER_DELIVERY_BACKLOG_20261015.md` at startup. It registers rendering, full parkour, ground/ring terrain, atmosphere, forklift/modular tools, crane freight between levels, delivery, refactoring and deferred canonical work. Forklift/crane design in `FORKLIFT_AND_CRANE_ROUTE_SYSTEM_20261015.md`; mapped ring sectors in `RING_UTILIZATION_DESIGN_20261015.md`. User explicitly rejects relying on conversation memory.

## Active work

Owner requests complete researched parkour/climbing, fluent controls/presentation and playgrounds throughout ground/rings/rooftops. Phase0 research/spec COMPLETE; phase1 real running/late takeoff ACTIVE, not yet accepted. Plans/spec: `PARKOUR_PHASED_PLAN_20261015.md`, `PARKOUR_MOVEMENT_SPEC_20261015.md`, state `PARKOUR_PHASE_STATE_20261015.json`. All eight later deliverable phases remain; no partial completion claim.

New urgent steering: looking bottom-to-top/top-to-bottom causes severe device stalls/force-closes; reduce excessive render submissions, target <1000. Root owns runtime spatial batching in `godot/presentation/static_world_batch.gd`, Main integration and Kit static-owner selection. Collision export runs BEFORE runtime batching; source primitive meshes/metadata retained, render layers0 prevent duplicates. Moving native bodies stay independent. Local triplanar materials use identical-geometry MultiMesh to preserve object coordinates; compatible other opaque geometry is merged in44m cells. Shader/translucent/live subtrees excluded.

Actual matched Godot4.7 softpipe160x120, production shadow atlas: baseline4311draws ground-up/3597top-down; first candidate1634/1922; final stress1036/933, shipping view/detail distances1036/766. Useful reduction; upward strict sub1000 still36over, downward meets it. Evidence `/data/data/com.termux/files/usr/tmp/scraperx-tower-render-20261015/comparison-receipt.json`; no phone crash-fix claim. Root regression tests PASS under actual GL: exported collision entries unchanged, transformed triangles/UVs retained, local-projected instancing poses exact, moving/translucent/shader exclusions. Headless Dummy MultiMesh readback gave zero transforms; actual GL test correct. Probe agent finding remaining submission owners before further changes.

## Takeoff integration

Root authored `simulation/parkour_intent.cpp` plus tick/world/checkpoint/rigid-support integration and public tests. Passive plant acquired only from fresh actual bottom contact at a trailing edge; real anchor/clock retained,8tick intent ceiling, actual reach/traction can truncate. No airborne impulse/contact manufacture. Baseline publiclate fails useful launch: peakvy.188m/s despite actual force/work910J, old1.2m/s target too slow.

High→xhigh adviser identified separate activated-plant command profile; worker implemented private CPP flag,5.5m/s authored no-load target-rate bound, nominal fixed-pose deltaU<=5000W*h, same3500N/k/c/.3m/.25s/originalclock/1285J total conservative debit. Completed target holds under original expiry/validity. Legacy begin unchanged. Primitive full focused suite PASS, receipt `/data/data/com.termux/files/usr/tmp/scraperx-planted-burst-20261015/result-receipt.json`; public late acceptance FAIL: peakvy.714369m/s, command1050J; primitive gain does not complete phase1. Tester finished; no further observer/tuning. Preserve SAME persistent `evidence/parkour-2026-10-15/coyote-window.json` reassessment/public replay. No source edits assigned. Guard never silently resets/renames. Current phase1basearchive3b59851f... at `/data/data/com.termux/files/usr/tmp/scraperx-parkour-phase1-20261015/libscraperx_sim.a`; correct Jolt8711a784... soft-velocity path. Replace physical_foot_push.cpp.o with new profile object; no Readback ABI change. Do not mix changed world.hpp/Snapshot with stale closures/bridge.

## World content

West-apron parkour builder integrated Main but NOT yet native delivered:59newsolids5paint, routes x[-77.5,-47.94],z[-100.5,-52.5],y0..2.6. Actual exported1459boxes/666hulls in private phase1 `world_solids_parkour.inc`, zero unknowns; live native include not yet replaced. Need native rebuild + meaningful public jump/crouch/recovery route before collision claim. Ring architect mapped6 differentiated process/arrival/recovery spaces; proposed first330/341/352cart staging/recovery preserving indispensable cart rise, no extra wheels/ladders. These are design reservations, not verified empty footprints or implementation.

Existing production mantle/topout already uses real finite hands/foot; do not re-delete retired gravity-off path retained only regression fixtures. Actual next movement gap includes forced <.2m/s stop at receiver, no far-side physical vault/palm owner, no wall-run/roll/slide full implementation yet. Details `PARKOUR_CLIMB_VAULT_AUDIT_20261015.md`.

## Published delivery and preserved deferred scope

0db3c5d tangible84-box receiving yard, spatial ambience, stance interpolation accepted native/Godot/export. Candidate run38780294948 GREEN artifact11994939579; local `/storage/emulated/0/Download/ScraperX-builds/0db3c5d-candidate/ScraperX-ChatGPT.apk` SHA72fb1063057ec9698e79bdb63f4018b29ef0db2dbfa77ff881351c0ab87bc727 matches provenance. Package/signing lineage/version verified. Full run38780294851 was running at last observation; no fullGREEN/device claim. Shell worker watches builds, not AI agents. New parkour/rendering work uncommitted/unpublished.

Preserve hookgate/bridge/tool WIP and earlier failures. Hinge40 trace numerically identical, control unresolved: NOT integrated/PASS. No catch/hook debugging treadmill. Full canonical scope, remaining native failures and exact-source fullGREEN APK remain pending. Approved actual push command `git -C /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo push origin ChatGPT`; never confuse local commit with publication. Historical details/evidence preserved elsewhere.
