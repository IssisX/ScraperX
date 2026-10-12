# Shared passive coupling on joint-supported lightweight grips

Bounded integration verified; source and proof are saved with this evidence.
The preceding committed source9a1b2c8 retained a pre-existing deformable-plank passive hang failure. The unchanged
maintained test880d5194 acquires real hands586 and fails required270-tick
sustained hanging at619. Old790c and new9a1b native logs match byte for byte;
this failure was not introduced by lightweight grip discovery/occlusion.

The logging-only private variant preserves the exact commands/assertions and
records actual public snapshot fields570–625. Compilation succeeds; execution
still exits1. Radial mode starts592, rest span0.750896454m. Actual span reaches
1.767821908m at618; both individual radii exceed1.75m. Hand generations stay1,1,
neutral work stays0, no regrip precedes release. At619 hands clear and velocity
changes by the ordinary gravity step. The native reach guard causes detachment;
why tension fails to arrest the inherited fall is the remaining diagnosis.

Prior Cartesian upward force1732N drops to305N on radial entry, later settles
near the834N player weight while downward speed persists~4.28m/s. Actual support
is joint-constrained and nearly stationary. A contact-free static versus light fixed-joint probe confirms the coupling
defect: identical85kg actor/initial downward3.87m/s/rest0.749483m produces
maxspan1.06877m on static support, but8.20736m on a real0.722kg support fixed
to world, whose actual motion is exactly0. The latter crosses reach at34ticks
and remains below the force cap. A free0.722kg control recoils and falls with
the actor, as expected. Native radial impulses run once before the structural
solve and use free-body inverse point mass; they do not iterate with the actual
joint load path. The maintained rejecting acceptance and generic repair remain
pending; the private probe is diagnostic proof, not completion. Preserve actual90Hz/substeps,1500N per hand,1.75m reach, reciprocal loads,
finite paid work and actual body/anchor continuity. Full movement/APK unfinished.

## First native candidate and newly exposed oracle mismatch

Super_Agent MCP session01a12d7c-2125-7bf1-9b35-a5578ba94112 implemented private
native User2 scalar rows on the actual joint/contact island. Native backend
50492408/headerb8f86fdd and root Simulation comment3bf90d5f compiled with original
warnings and the pinned Jolt RTTI policy. Both dependency-sensitive objects were
rebuilt; no mixed header ABI. Independent native review found no gameplay blocker.
Settings can recreate the finite law; archive registration/topology/work restore
remain separate owners. Save→COM-changing shape change→StateRecorder restore can
leave dormant anchor readback inconsistent; no production caller was found and
full archive/shape restoration is not credited by this slice.

The maintained controlled catch is GREEN: static/fixed maximum span1.06857m,
under1µm difference, fixed support motion~60nm, real free-body recoil and force
receipts within0.01N·s. Actual impulse cap, force direction and zero neutral work
pass. The real unchanged plank keeps two hands all270 ticks but then fails its
static endpoint sag limit:45.150757mm against45mm, with1252.5589N hand load,
player vy+1.68058m/s and board centre vy+0.24245m/s. Tuning physics to this
unsettled static assumption is unsupported.

A private logging-only continuation explicitly skips only that static assertion:
all scheduled Drop/momentum, real1936 recovery and unload checks pass, deaths0;
this diagnostic receives no completion credit. Over the same270 ticks mean sag
35.6913mm and mean public hand Fy1080.760N vary through the real passive arc.
Public force is last-substep readback, and real capsule contacts remain possible;
no contact-free shipping momentum or exact force quadrature is claimed.

The maintained oracle uses independent discrete beam influence coefficients
and measured hand load over the same270 ticks, retaining20–45mm on85kg-normalized
mean sag. Immediate Drop/recovery/unload and all hand/force checks remain. No
extra settling, endpoint selection, raised range or native force/damping/reach
change. Final finite-force and intact-reference checks reject invalid inputs.

## Final rejecting verification

Final native Simulation SHA256
`3bf90d5f879dc056236348c4376bf8f64098a2052a8f688204ac58151ed9b407`,
backend `504924083db46ba4e0f6373c8ec0bcdc97e157e70278e536d5e2b02c41ee5f27`,
header `b8f86fdd58ec6f06d32a0db09b8cc0159a4ff023c9527b2ff2cfaaad180b20b2`.
Maintained hand test
`1dc69671c5817aa90df2c35f7ec94bfefe7da84fc3dd95127e4e1c1346b20a9b`
passes14/14, including actual post-solver player/support impulse receipts,
integrated angular conservation, paid lengths, late arc and handoffs.
Final plank test
`399a907f4c4553fd1bdb62d0d93941b6bea90991c0cd09d955f83e79e3e482ef`
passes actual270-tick intact hang, finite hands, normalized mean sag28.2666mm,
ordinary Drop/momentum, real1936 recovery and unload return. The coupled solver
repair did not change the270-tick deadline or physical inputs.

The explicitly private fault mutation restores an extra85kg gravity load to the
already hand-loaded support. Its real wood fractures and the intact-beam guard
rejects it. This rejects duplicate weight through the complete physical oracle;
it does not independently falsify the final normalization threshold. The mutant
archive/bridge is not a delivery artifact and was never staged to the project.

Physical traversal, dynamic grip, static/moving ledge, thin/rising catches and
maintained simulation source7c0a908 all pass. Full simulation output matches the
preceding passing run byte for byte. `final-simulation-receipt.json`,
`affected-native-receipt.json` and `final-plank-and-mutant-receipt.json` retain
separate compiled-source and last test-guard boundaries; earlier affected tests
were not unnecessarily repeated for later assertion-only strengthening.

The full headless touch encounter passes on exact bridge
`79afec4711455b313ec681b2aa59b56dffd09afe82e56b8501786b22ed62c479`
and final driver
`99e45603c727dc6b7ee957a9e1857151fdde5cabbb22b7b3679c6bf8ba532944`.
A newly exposed first-contact standing assertion incorrectly rejected the
existing native brace response: diagnostic tick1009 shows response1, recovery
0.622738s, both UI toggles clear and zero hands. The driver now requires clear
input, real1931 footing and zero actual hands through native recovery (declared
maximum2s, bounded check2.5s), then ordinary standing. No input/pose/physics reset.
Manual release windows, receiver, miss/recovery/retry and onward checks remain.
`touch-final-receipt.json` records exit0 and the exact source/log binding.

This is local ARM native and headless input evidence. Material-net visibility/
coupling, generalized one-hand control, universal shape eligibility, articulated
limbs, phone feel and fullGREEN/APK remain outside completion credit.
