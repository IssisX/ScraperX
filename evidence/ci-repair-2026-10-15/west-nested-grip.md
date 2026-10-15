# West junction nested grip rejection — 2026-10-15

Read-only production diagnosis at checkout `ChatGPT`, HEAD `c2b2517d67d19e8ae4843db6177048db546277ad`; existing dirty/untracked work preserved. Only this evidence file is authored in the checkout. Private sources, object/archive, executable and receipts live at `/data/data/com.termux/files/usr/tmp/scraperx-ci-west-nested-grip-20261015/`.

## Exact cause

The north folded end bracket physically occludes both the horizontal bar and the rising rail at the first failed lateral query. It is not a coincident leaf seam or missing search collision. Strict hold classification and exact first-hit visibility are correct here.

The first failed query remains relevant lateral sample34. Centre `(-23.657510757,384.157409668,-137.194503784)`, aim `(-24.057510376,384.607421875,-136.844497681)`, facing `(-1,0,0)`. `CollideShape` returns three actual leaves of body8391191/entity1934:

| Candidate leaf | Geometry | First rejecting predicate |
| --- | --- | --- |
| 4294967260 | Rising rail, centre(-24.30,387.049988,-137.600006), half(.055,.055,2.577301) | Visibility exact leaf identity |
| 4294967255 | Horizontal bar, centre(-24.30,384.600006,-139), half(.0600004,.0599976,2.199997) | Visibility exact leaf identity |
| 4294967266 | North end bracket, centre(-24.30,384.599976,-136.179993), half(.1400003,.1399994,.6700058) | Hold section .28m exceeds .18m |

Both rail candidates pass rigid/nonsensor, box/hold and front tests. Front distance is `.642488480m`, minimum `.174999997m`. Rising selected centreline point is `(-24.299999237,384.619842529,-136.806488037)`; horizontal point is `(-24.299999237,384.600006104,-136.844497681)`.

Their approach origin is the actual COM-anchored `(-23.657510757,384.607421875,-136.851165771)`. Both rays hit bracket leaf4294967266 first on the SAME correct body: rising ray fraction `.758544326`, hit `(-24.159997940,384.617126465,-136.816223145)`; bar ray fraction `.758489668`, hit `(-24.159999847,384.601623535,-136.845947266)`. Thus same-body succeeds and exact leaf fails for a genuine intervening structural surface. Backend and capsule clearance cannot repair this; prior matched trace proves capsule clearance true at this query.

The authoritative obstruction is `src/sim/west_brace_bay_route.cpp:171`: north bracket span x[-24.44,-24.16], y[384.46,384.74], z[-136.85,north_post_z+.14]. Its south face begins50mm before the rail's z=-136.80 junction, and its front face x=-24.16 is140mm in front of both rail centrelines. The authored comment says these brackets sit outside the active traverse span; the live solids contradict that intent at the north junction.

## Smallest owner-correct repair boundary

Repair the authored folded NORTH end bracket/load path in `west_brace_bay_route.cpp:167–176`. Mount its longitudinal leg behind the exposed bar/diagonal hand plane, retaining real overlap with both hand members and the existing post crossmember. This exposes the intended approach without removing the frame load path. Derive the offset from the actual rail half sections; place the bracket front face behind the rail centreline and exposed first-hit surfaces while retaining positive backside member overlap. The existing20mm endpoint extension remains unchanged. Keep its structural section larger than the hold threshold. An offset/back-mounted bracket is a physical design correction, not a tolerance adjustment.

Do not accept arbitrary same-body leaves, enlarge reach, classify the thick bracket as a hold, or weaken the junction observer. The visible obstruction is real. Geometry repair is recommended but NOT implemented or runtime-proven by this assignment. The exact downstream rising-rail transfer remains unverified after such a repair; run the same route once after implementation.

## Replay and limits

Command executed: `proot-distro login ubuntu -- /usr/bin/python3 /data/data/com.termux/files/usr/tmp/scraperx-ci-west-nested-grip-20261015/nested_run.py`.

Private climb compilation and archive replacement/link PASS. Completed replay FAIL exit1 in45.3675s, reproducing the unchanged junction failure at global tick1322: player(-23.7338,383.962,-137.157), two real hands, Climbing, zero deaths. `nested-run.log` lines42–58 capture every candidate's first rejection; final line retains the original route assertion. A redundant launch was accidentally started because the first async session receipt was not retained; that second process was canceled, and no second completed replay is claimed. Both launches used identical private source. This has no production/runtime mutation beyond their separate simulations.

The private runner preserves frozen archive18cc2d81b7628927b2acbbf1997a417e670e1851a738d4f2270d00673b12bb0d and matching frozen Snapshot header57f165f39595c6100f3e8a094f7df7646ca6248c54a29d9be7c974c2b8285e1f; corrected Jolt8711a784f394fdf91af3366ab0b6695e80322cb73b46a8d1659e775b20aced1b. No live changed Snapshot header was linked against18cc. `nested-receipt.json` records commands, identities and verdict; `source-receipt.json` binds private source/driver/executable/log. Instrumentation is gated to only this actual sample's original queries; no alternate aim/input/geometry sweep. Native host evidence only, no Android/device/full route pass claim.

Root accepted this cause and selected a160mm rearward shift for both existing folded end brackets: x[-24.44,-24.16] to[-24.60,-24.32], retaining section/y/z and35–40mm backside overlap with bar/diagonal plus the post crossmember connection. Root owns that implementation and its same-route acceptance; this note does not claim it was applied or passed.
