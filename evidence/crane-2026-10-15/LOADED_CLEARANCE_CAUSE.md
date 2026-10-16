# Loaded front-sling mast contact — supported cause, 2026-10-15

Checkout `IssisX/ScraperX`, branch `ChatGPT`, HEAD `470cc214035da1143927e38fe538baddc63f7944`. Extensive existing/concurrent changes preserved. Root owns geometry/world/bridge/Godot. This assignment changes the test observer only; temporary owner logging was removed after the capture. No force, pose, stock, COM, clearance, failure gate, collision or mechanics correction was made.

## First wrong edge

Actual load transfer pitches the rig until the front sling's **25mm radius collision envelope touches the real forklift mast top crossbar**. The existing owner then correctly removes that obstructed leg. This is a physical load-path/rig balance defect in authored crane hardware, not a query translation defect or overlength failure.

The persistent [assembly window](assembly-window.json) retains trials1–10 and was explicitly reassessed from trial10 to permit one observer-only discrimination. [Trial11](assembly-window.trial-11.log), exit1, reproduces tick93 exactly: chassis y1.09173m versus supported pickup0.608765m, rise0.482965m. Rig mask15→14; leg0 actual3.35746m, stock3.356m, failure threshold3.38956m. All3 wheels are airborne at failure. Trial10 already established a valid pickup: actual fork clearance0.152178m, finite carriage0.352146m, forward tilt−0.138481rad, actual wheel forces4768.57/4776.38/23539.2N. No tine support or pickup gate failure explains this transition.

## Query and geometry discriminator

Actual mast BodyID8388610 has these shape-origin-local boxes:

| Part | Center m | Half extents m |
| --- | --- | --- |
| Uprights | (±.55,1.5,0) | (.065,1.5,.075) |
| Top crossbar | (0,2.92,0) | (.55,.08,.08) |
| Bottom crossbar | (0,.1,0) | (.57,.1,.1) |

Leg0 actual mast-local top `(-.512161,3.54215,-.00967407)`, bottom `(-.519974,.235666,-.592430)`. Hit fraction `.161703`; actual mast surface point `(-.513428,3,-.0800018)` lies exactly on the top/rear edge of the top crossbar. Probe center `(-.513428,3.00748,-.103912)` is approximately25mm from that edge. Subshape4294967294 is recorded without assuming it directly indexes the authored part vector; the surface coordinate itself identifies the top crossbar. Neither endpoints nor a fabricated mast frame were substituted.

The exact same world cast with output base zero hits the same BodyID/subshape/contact at fraction`.161698`. Their fraction difference5e−6 and contact float error are negligible relative to the geometry. Pinned Jolt `TransformedShape.cpp:97` subtracts the supplied base from BOTH cast and target transforms; `NarrowPhaseQuery.cpp:312` passes the original world cast into that owner. The base is not a second body translation. `PoweredCable::CableRow` converts shape-local eyes to COM-local once and reconstructs them through the COM transform; the actual endpoints/casts coincide with the test's shape-world transform calculation. A frame correction in YardCrane/PoweredCable is unsupported.

A conservative rear-route condition for this crossbar is cable center mast-local z≤−.105m while y lies2.815..3.025m (box plus25mm radius). At y3.025 current leg0 center z=−.100820m, violating that conservative envelope by4.18mm. The exact round-corner cast establishes the first contact. Front leg1 is barely still clear; all other legs clear. Ignoring the mast, reducing radius or shortening the probe would hide real steel contact.

## Mass and load path

The trace records shape origins, world COMs, shape-local COMs, quaternions, masses, velocities and angular velocities for the actual chassis, mast, carriage, fork, spreader, trolley and boom. Derived forward-axis pitch: chassis18.7948°, spreader19.3664°, articulated mast10.8494°. Chassis/spreader continue rotating at approximately+.1844/+.1949rad/s. The actual front tilt stop therefore does not protect the mast against the loaded common trim.

Actual masses: chassis3040.96kg, mast220kg, carriage40kg, fork72kg, spreader228.645kg. Articulated truck aggregate COM transformed into the actual chassis frame is approximately`(.000275,.216501,-.419814)m`; the empty spreader COM/upper hook share local z−.275599m. Thus the empty rig is balanced for empty suspension but the loaded truck's gravitational resultant lies roughly144mm farther aft.

At a level, vertically rigged static truck, front/rear eye z are±.75m. Truck mass3372.96kg requires front share(.75−.419814)/1.5=.220124 and rear share.779876: approximately7.28kN front and25.81kN rear. That downward resultant at z≈−.4198m is aft of the existing upper hook by≈.1442m and supplies about4.77kN·m initial pitch moment on the spreader. This is the mechanically supported reason to correct loaded balance. The simplified ratio atan(.1442/.365)≈21.6° only illustrates the short upper/lower-eye lever; it is NOT an equilibrium prediction, because actual cable skew, articulated pitch and changing force distribution matter.

At first failure the surviving leg forces are2733.4/11819.7/13781.3N; hoist35259.7N. Failed leg0's state is cleared by current `YardCrane::post_step`. The temporary environment-gated pre-destruction logger was not enabled inside the login environment, so its exact final impulse/force is **unavailable**, not inferred from the remaining forces. This does not leave the collision cause ambiguous. Trial11 captured the actual collision itself and all poses; no additional experiment was spent merely to fill that missing telemetry.

## Smallest owner-correct repair boundary

Root's `src/sim/yard_crane_geometry.cpp`: make the upper lifting point and honestly derived empty-spreader COM align with the loaded sling-resultant region near z−.42m, using connected material and recalculated mass/inertia. Preserve existing truck, sling eyes and stock, force/work caps, mast collision and all checks. Moving only the upper hook would pitch the EMPTY rig and destroy valid pickup alignment; moving/adding real ballast and its support is the coupled design correction.

A bounded material estimate: existing56kg rear steel block plus29.952kg baseplate at z−.65m total85.952kg. Before added supporting steel, moving those two parts aft≈.384m moves the existing228.645kg spreader centroid from−.275599 to−.419814m. The resulting plate extends beyond the old rail end, so extend real rails/crossmember/support first, then derive the exact final ballast position from the finished material centroid. Do not assign a magic COM, place ballast in empty air or trust this estimate as final design acceptance. The actual query must remain the routing authority.

Native build PASS with one compiler, private coherent `source-v2/src` include overlay first and archive`18af7c94...` reused; no working `simulation.hpp/world.hpp` ABI was pulled in. Capture binary SHA256`d1ae9491fb596238faf1bdc7c024c7fb9907407cad653f66183143b8bbb1a93f`. Receipt `/data/data/com.termux/files/usr/tmp/scraperx-crane-mechanics-20261015/build-receipt.json`; sources/archive hashes retained by trial11. Temporary owner diagnostic cleanup changes current owner bytes relative to that capture receipt; no claim of a new current-source build is made. Retained test observer does not change simulation. Existing cable/articulated prerequisites were reused, not rerun. Full loaded lift/hold/land/release/drive/retry and world11m transfer remain unaccepted until root's physical correction passes their existing path.
