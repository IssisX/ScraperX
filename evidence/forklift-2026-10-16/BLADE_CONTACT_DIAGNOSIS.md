# Blade contact diagnosis — 2026-10-16

Production-owner native discriminator completed; real detach succeeded without changing any production source, support predicate, contact tolerance, force cap, pose, or velocity during operation.

## Supported cause

Mast relative zero does not make the production blades level in the world. The physical chassis pitches rearward on its native suspension. Continued lowering against an edge of the stand increases that pitch and loads the joint; persisted contact alone does not put the blade bottom centers within the existing3mm condition. Exact rays hit the correct static entity2447, excluding wrong static identity or coordinate-frame mismatch in this fixture.

A loaded neutral RED (trial3) had world blade +Z Y=0.0193457, mast relative=0.000086844, both center gaps≈11.95mm, actual static2447 contact persisted1469 times, native tool linear/angular speeds below0.00007, lift force -4089N, owner refusal10. Finite negative tilt leveled the blades but retained excessive coupling load: center gaps0.065mm, refusal12. Further lowering is consequently the wrong recovery after overpressing.

## Causal GREEN

From the same authored production yard and same initial transform, level before loading the stand. Native mast relative -0.00790254 compensated chassis +Z Y0.00812558, leaving blade +Z Y0.000431215. Fine lowering at input-0.01, with finite tilt feedback clamp(-20*world_blade_axis_Y,-0.15,+0.15), found the existing native eligibility window. Two subsequent45tick neutral periods retained refusal0 and `try_detach()` returned1. No support predicate changed.

The diagnostic initially raised to0.35m, drove forward at input0.35 from chassis(-88,.65,-64.97) to nominal z-67.20, then180ticks finite brake settled nearz-67.386. It lowered450ticks at input-0.15 with45neutral ticks every90, leaving centers≈20.7mm above the stand. It leveled using native tilt before fine lowering. The final fine loop checked actual owner refusal every tick, paused45ticks when None appeared, and accepted only if None persisted. One early None observation did not survive neutral; continued fine lowering found the stable window. This shows why eligibility must be checked after neutral settling.

## Exact samples

Loaded RED:
```
RED tick=1835 chassis=-87.9993,0.615776,-67.3796 tool=-87.9996,0.244532,-69.1169 qlift=0.0749082 mast_angle=8.6844e-05 blade_axisY=0.0193457 chassis_axisY=0.0195375 velocity=6.88999e-05 omega=3.96558e-05 lift_force=-4089.44 refusal=10 contact=1 bedID=8388617 bedEntity=2447 static=1 added=1 persist=1469
part=0 bottom=-87.9996,0.304521,-69.1157 gapToTop=0.104521 exact_ray_hit=0 exact_gap=999
part=1 bottom=-87.9996,0.804427,-69.1061 gapToTop=0.604427 exact_ray_hit=0 exact_gap=999
part=2 bottom=-87.5196,0.244535,-69.1171 gapToTop=0.0445348 exact_ray_hit=0 exact_gap=999
part=3 bottom=-88.4796,0.244529,-69.1167 gapToTop=0.0445287 exact_ray_hit=0 exact_gap=999
part=4 bottom=-87.5199,0.211955,-70.0179 gapToTop=0.0119553 exact_ray_hit=1 exact_gap=0.0119553
part=5 bottom=-88.4799,0.211949,-70.0175 gapToTop=0.0119492 exact_ray_hit=1 exact_gap=0.0119492
```

Level after overloading (support valid, still joint loaded):
```
LEVEL tick=2871 chassis=-87.9994,0.621318,-67.3777 tool=-87.9996,0.249844,-69.108 qlift=0.0749246 mast_angle=-0.0252615 blade_axisY=0.000245714 chassis_axisY=0.0257234 velocity=3.54139e-06 omega=1.15326e-05 lift_force=-4108.59 refusal=12 contact=1 bedID=8388617 bedEntity=2447 static=1 added=1 persist=5613
part=0 bottom=-87.9996,0.309844,-69.108 gapToTop=0.109844 exact_ray_hit=0 exact_gap=999
part=1 bottom=-87.9996,0.809844,-69.1078 gapToTop=0.609844 exact_ray_hit=0 exact_gap=999
part=2 bottom=-87.5196,0.249844,-69.1081 gapToTop=0.0498442 exact_ray_hit=0 exact_gap=999
part=3 bottom=-88.4796,0.249844,-69.1079 gapToTop=0.0498441 exact_ray_hit=0 exact_gap=999
part=4 bottom=-87.5198,0.200065,-70.0081 gapToTop=6.53148e-05 exact_ray_hit=1 exact_gap=6.531e-05
part=5 bottom=-88.4798,0.200065,-70.0079 gapToTop=6.52701e-05 exact_ray_hit=1 exact_gap=6.52707e-05
```

Successful final neutral state:
```
GREEN_candidate tick=3333 chassis=-87.998,0.608749,-67.3862 tool=-87.9985,0.252642,-69.1167 qlift=0.113186 mast_angle=-0.00790254 blade_axisY=0.000431215 chassis_axisY=0.00812558 velocity=0.000976648 omega=0.00122362 lift_force=1099.04 refusal=0 contact=1 bedID=8388617 bedEntity=2447 static=1 added=1 persist=7461
part=0 bottom=-87.9985,0.312642,-69.1167 gapToTop=0.112642 exact_ray_hit=0 exact_gap=999
part=1 bottom=-87.9984,0.812642,-69.1164 gapToTop=0.612642 exact_ray_hit=0 exact_gap=999
part=2 bottom=-87.5185,0.252592,-69.1169 gapToTop=0.0525921 exact_ray_hit=0 exact_gap=999
part=3 bottom=-88.4785,0.252692,-69.1165 gapToTop=0.0526921 exact_ray_hit=0 exact_gap=999
part=4 bottom=-87.5189,0.20298,-70.0169 gapToTop=0.00298018 exact_ray_hit=1 exact_gap=0.00298018
part=5 bottom=-88.4789,0.20308,-70.0165 gapToTop=0.00308021 exact_ray_hit=1 exact_gap=0.0030802
detach=1
```

## Provenance and limits

Private workspace: `/data/data/com.termux/files/usr/tmp/scraperx-fork-blade-contact-probe-20261016`.

Reproduce: `python /data/data/com.termux/files/usr/tmp/scraperx-fork-blade-contact-probe-20261016/build.py`

`receipt.json` records exact compile command, source hash, start/end times and build/run exit0. `diagnostic-prerequisite.json` records actual artifact/source/archive hashes without changing the public fork-exchange window. Linked actual source-v13 production WarehouseForkliftYard and WarehouseForklift in libscraperx_sim.a and final libJolt-finite-clean.a. Masses: chassis3040.96kg, mast220kg, carriage40kg, fork72kg. Native update90Hz/four collision substeps, ordinary static floor atgrade0, actual authored production stand2447 top0.20. Native contact listener and the exact production ray predicate were observed independently.

Before the first update only, all four owner-created truck bodies received one common yawPI/translation; joint local frames stayed intact. No Avatar or surrounding fullworld; this is causal fixture evidence, not public-route, runtime-Godot, or device acceptance. Root owns public-driver integration and the next authorized public attempt. Repository branchChatGPT HEAD83b84b74b3dfc90c79a473f3160ae7952507f5ad; all dirty/concurrent work preserved. Only this new evidence file is written in the repository.

Exact fine-lowering start: qlift0.121157m, mastrelative-0.00790247rad, world bladeaxisY0.000433482. Initial leveling used tilt `clamp(-20*axisY,-0.3,+0.3)` for900ticks with45neutral ticks after each180; then90neutral. The desired early stop `abs(axisY)<0.0002 && i>90` did not trigger in the successful trace, so the900tick bound ended leveling. Fine lowering used1800maximum control ticks and stopped on the first None surviving45neutral ticks; another45neutral ticks followed before detach. Every90fine ticks also paused45ticks. The final successful predicate uses the lower blade center gap2.98018mm; the other blade center was3.08021mm. Production requires any qualifying part, not both.
