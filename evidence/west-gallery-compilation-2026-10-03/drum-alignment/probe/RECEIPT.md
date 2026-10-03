# Loaded drum/tongue contact probe — rejected geometry

Two bounded private native runs completed; **neither establishes a usable withdrawal**. Native process exit 0 verifies finite states, Update success and proxy force/work/contact bounds, not encounter acceptance. The earliest supported blocker is an **introduced collision-layout defect**: fixed bearing rails intersect the tongue uprights by 42 mm before player input. Preserve these results; do not infer a drum-face friction requirement from them.

## Recovery and scope

IssisX/ScraperX `ChatGPT`, clean head `543d13e04c615af6869d46826214878446513200` inspected before work. Parent `../FEASIBILITY_SCREEN.md` was read; all new writes are under this probe directory. No repo, shared build/runtime, prior encounter/rope/audio evidence, commit, push or agent changes were made. Pinned Jolt is `e77f175595e64cb44218cc9d9d56fc365ad0e36a`, using the existing static library in the isolated Ubuntu compiler/runtime.

`sh build.sh` exited 0. Exactly two 12-second runs at 90 Hz with one `Update(float(h),4)` per tick: `contact-probe noinput` and `contact-probe withdraw`; both exited 0. Global settings 10 velocity / 2 position, 0.02 m slop, local slider/grip 40/8, actual internal dt 0.00277777784504 s, 4320 observer callbacks per run. No settings tuning, geometry revision or third run was performed.

## Actual assembly

Local rack coordinates are X lateral, N normal, S downhill. Rack rotation is asin(0.18/2) = 5.163607 degrees: a 2.0 m × 1.5 m, 0.16 m-thick static rack drops 0.18 m across its complete length S∈[−1,+1]. Surface origin is world (0,1.5,0). Static 0.12 m-thick side walls have their inner faces at X=±0.75 and tops at N=1.45.

The loose drum is an actual dynamic solid cylinder: mass 360 kg (stored mass 359.999991283 kg), radius 0.60 m, full axial width 1.10 m, 10 mm convex radius; provided axial inertia 64.8 and transverse inertia 68.7 kg m². Its initial COM is local (0,0.605,−0.65), zero velocity, with its cylinder axis along +X. Gravity and real rack/stop/wall contact determine its subsequent state. The drum's initial available path is not a full 2 m center-of-mass path; the prior 635.688 J ideal full-path screen cannot be claimed as this fixture's measured arrival energy.

The 60 kg tongue is one moving native compound U-frame, opening width 1.5 m, initial X origin +0.35 m. Each vertical stop leg is 0.18 m wide, N=.06..1.46. Its cross-section has an integral 45° relieving front: the full 0.18 m width permits 0.18 m progressive downhill clearance, with 8 mm rounded convex hull edges. The bridge spans X=±0.93, N=1.46..1.58. A declared integral handle/bracket lies above the left wall. Those compound shapes represent a hollow structural frame by mass override; the collision volume is not a measured solid-steel mass distribution. Native shape-derived inertia is used for the 60 kg body.

A native SliderConstraint attaches the tongue to the static rack, world X axis, limits ±0.4 m, friction 35 N; actual native guide reactions are observed. Two fixed full-width rail boxes were placed at N=1.418..1.458, S=−.095..−.035 and +.055..+.115. Their friction is zero; drum/rack/tongue/wall friction is CHOSEN 0.90, explicitly testing the prior screen's difficult friction corner. Restitution, generic damping and sleeping are off; CCD is enabled. Release could only arise from the physical translated opening, not a flag, pose, weld or velocity write.

**Fatal intersection:** the rails run through both tongue leg tops. Their N overlap is `min(1.458,1.46) − max(1.418,.06) = .042 m`, with intersecting X/S ranges. This is not a clearance fit or hollow sleeve. Native maximum penetration is 0.042000096 m, corroborating the authored overlap. Guide contact counts persist through both runs, with 19.98 mm guide drift and large startup reactions. The joint and collision solver are opposing incompatible geometry. Do not hide this by disabling real collisions or calling it a high friction corner.

## Honest proxy control

The actual 85 kg upright dynamic capsule is gravity-on, friction 0, straight half-height .55 m, radius .35 m (total half-height .90 m). Its fixed inspection deck is a real horizontal collision box. Both runs start with identical assembly and capsule initial state. No-input adds no grip or control force. Withdrawal at t=3 s acquires a finite native SixDOF grip only if the actual proxy/handle points are within .15 m; the observed initial gap is .02050 m. Rotational motors are off, axes free; translational springs are 5000 N/m, damping 180 Ns/m. Axis force limits are X=240 N, Y/Z=40 N, so the fixed per-axis envelope has vector upper bound √(240²+40²+40²)=246.57656 N.

The **explicit proxy** controller requests .25 m capsule withdrawal over 3 s, then holds that input target. It applies native AddForce only while actual deck foot-contact normal is upward. PD coefficients are 2000 N/m and 180 Ns/m; traction is bounded at 250 N, with 80 J positive command-work budget. Constant force during each outer update allows direct `F·(COM_after−COM_before)` work observation, accumulated separately positive/negative. A conservative horizontal-displacement bound limits force when approaching the remaining work budget. No force was applied during unsupported observed internal steps. The proxy does not establish ordinary input, shipping handle acquisition, anatomical hand contact or a complete muscle energy ledger.

## Results

| Measurement | No input | Bounded withdrawal |
|---|---:|---:|
| Final tongue origin X, m | 0.35010880 | 0.35010880 |
| Largest negative tongue travel, m | −0.00140525 | −0.00140525 |
| Peak drum yaw | 0.001821 degrees | 0.001821 degrees |
| Maximum drum lateral shift | 0.19083 mm | 0.19083 mm |
| Final drum S, m | −0.65078765 | −0.65078765 |
| Drum exited | no | no |
| Peak guide transverse force | 2457.854 N | 2457.854 N |
| Peak guide torque | 2749.574 Nm | 2749.574 Nm |
| Peak guide friction force | 35.000 N | 35.000 N |
| Maximum guide drift | 19.982 mm | 19.982 mm |
| Peak control force | 0 | 250 N |
| Positive / negative control work | 0 / 0 J | 36.1901 / 10.4659 J |
| Peak finite grip force | 0 | 243.3176 N |
| Maximum grip point error | n/a | 0.148763 m |
| Maximum grip storage proxy | n/a | 55.326 J |
| Unsupported force samples | 0 | 0 |
| Finite observed states / Update errors | true / 0 | true / 0 |

Native contact event counts (Added+Persisted, not unique contact points or impulses) for each case are identical:

- drum/rack 4320; drum/tongue 4320;
- drum/left wall 0; drum/right wall 0;
- tongue/guide rails 21599; tongue/side wall 4320;
- capsule/inspection deck 4320;
- drum/receiver 0; drum/background floor 0; other pairs 0.

The capsule maintains real support with friction zero; commands and finite grip load a physically stuck tongue. Control force/work bounds pass. **Encounter acceptance fails**: withdrawal does not achieve the .15 m minimum translated-opening release travel, much less the requested .25 m alignment. No-input held, but because an unintended obstruction participates, it does not validate the intended stop topology.

## Energy and first missing boundary

Initial mechanical KE+gravity PE is 11196.8464341 J; final is 11167.793397 J in both cases. That difference is **not a conservation residual**. Contact projection, friction/guide loss, motor work and compliance storage have not been separated. The .5*k*point_error² grip energy is explicitly a proxy diagnostic, not proven exact SixDOF motor storage. No positive muscle-power, full energy closure, drum arrival/pan transfer, receiver/recovery, rendered gameplay, APK or device claim is made.

First missing assembly boundary: compile a non-intersecting real bearing/guide interface for the U-frame and its complete ±.4 m sweep (including upright/bridge/handle clearances). The rounded drum contact and 0.90 friction corner cannot be evaluated as playable until that is resolved. This assignment stops with the two preserved cases and the supported geometric rejection, per the parent's instruction; no production adoption is recommended from these results.
