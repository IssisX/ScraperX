# Resting assembly CPU cost — 2026-10-17

The owner's slow-everywhere report led to native measurement independent of
Godot rendering. A prepared idle census at native386/476/566 found198active
rigid bodies and127active constraints. The reclaim assembly alone contained
103active bodies:84refractory pieces plus feed gate in one essentially still
contact island, and18still hinge-connected bodies. Kit disables sleeping by
default. Body counts locate the lifecycle issue; they do not measure its exact
share of frame time. One read-only Sol/high worker gathered the bounded profile
and census; root selected, implemented and tested this repair.

Only this assembly now permits ordinary Jolt sleep. Existing native commands
activate its bearing/drives, attachments activate support, restore activates
restored bodies, fracture activates released pieces, and contact activation
remains solver-owned. No distance/view culling or scripted freeze is used.
Geometry, mass/inertia, contacts, friction, force/work/energy limits, solver
iterations, substeps and90Hz are unchanged. No graphics quality was reduced.

Inactive motor force is not published as a new instantaneous force receipt.
The original force-times-actual-displacement work integral remains intact,
including the final moving step before sleep. Later sleeping steps have no
travel and therefore spend no command work from a retained impulse. The terminal
sleep step may report instantaneous force0 even though a force acted during it;
that readout is not proof that no work or force occurred.

## Executed evidence

[Exact source hashes, command arrays and actual outputs](IDLE_PHYSICS_COMMANDS.json).
[Standalone ordinary Simulation benchmark](idle_cost.cpp). Host: LinuxARM64,
GNU C++17 Release under Ubuntu proot; source cdab7cd/native944e0e1 plus these two
owner edits. Isolated clean headers avoid unrelated dirty grapple ABI changes.
Each benchmark waits for actual readiness, then advances360neutral90Hz ticks.
Both final runs reached readiness386 with0deaths.

| Per-tick metric | Original | Candidate |
|---|---:|---:|
| WALL mean |9.49294ms|7.38421ms|
| WALL p50 |8.13750ms|7.07182ms|
| WALL p95 |14.08010ms|8.95307ms|
| Process CPU mean |12.82080ms|9.25239ms|

Mean WALL reduced22.2%, median13.1%, process CPU27.8% in this pair. Scheduler,
phone thermal state and proot affect host timing. This is not foreground APK FPS
or a guarantee of a complete slowdown fix. A preliminary stable paired run
showed16.46%mean WALL reduction. Its unfinished route was stopped after root
corrected a precursor force guard that would have omitted final sleep-step work;
only the final exact source route below counts as acceptance.

`gravity_reclaim_route_tests`: exit0. Actual output ends
`PASS RECLAIM approach/finite-feed/loaded-gravity-ascent/brake/330exit/actual-discharge/empty-return/explicit-checkpoint-retry/second-loaded-ascent/late-miss319-recovery/Tower-exit initial_staging_only=1`.
Inventory falls2052.7kg to130.038kg,4actual fractures occur, loaded ascent/exit,
real discharge, empty return, full checkpoint restore, second load and recovery
remain functional. Existing geometric/physical/energy expectations were unchanged.
A Sol/high read-only mechanics review found no supported blocking source defect.

## Limits and delivery

The source review does not prove every jam/contact-removal sequence. Existing
command, contact, attachment and restore wake paths are retained; broad always-on
sleep changes are outside this patch. Other idle mechanisms and renderer/bridge
costs remain separate measured work. Matching Android source/build/artifact and
required full proof are delivery obligations. Phone FPS/heat/feel remain
unverified until a matching foreground play/report; no whole-game green claim.

The combined build also moves the clipped BRAKE caption inside the safe area,
a defect visible in the actual cdab7cd CI cab capture. The responsive-layout
check now measures all three rendered stick captions and576layouts pass again.
