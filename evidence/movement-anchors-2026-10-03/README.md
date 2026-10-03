# Planted-hand interpolation and normal campaign continuity — 2026-10-03

Repository `IssisX/ScraperX`, branch `ChatGPT`, base `2942fab`. [The receipt](receipt.json) hashes the actual changed source and local extension. This is local verification; a new exact-source Actions/APK receipt is pending.

## Changes and supported causes

- Native render anchors identify each planted hand's actual Jolt body and grip generation. Rigid locals convert from COM to visible shape origin once, then use the same Kit pose interpolation as the rendered member. New acquisitions and changed grips snap the local coordinate through that rendered pose. Hand ownership is evaluated before the principal-support fallback. The cargo net samples its existing vertex histories at the same render timestamp, without allocating a mesh or writing physical state.
- Landing recovery expires from the recorded impact. Blocked walking previously renewed it every tick, indefinitely bypassing step-up. The real-cabinet regression reproduced that renewal, then passed after its removal; native contact forces and impact detection remain authoritative.
- The shipping touch driver now walks from ordinary grade through the cargo net and retained upper route into AS-026 without restart or fixture selection. A +99 grip approach and AS-026 takeoff stance were corrected using actual reach/catch failures. Native grip reach, forces, hinge and receiving geometry remain authoritative. Actions now runs this uninterrupted scenario explicitly.

## Evidence actually obtained

- [Native tests](native-tests.log): final affected ladder, cargo-net, landing and parkour tests pass **4/4**, 24.51 s. The ladder acquisition falsifier failed before the owner's render-clock repair. Retained net samples reject independent hand interpolation.
- [Uninterrupted normal campaign](normal-campaign.log): shipping headless touch exits 0 at **Y=121.9**, support **11**, **zero deaths**, **zero launcher work**. Normal-world identity checks pass; no debug relocation follows ordinary spawn. Loaded swing span is 0.074953 rad in this run.
- [Rendered AS-026 slice](rendered-ladder.log): shipping touch with explicit supported +110 m staging exits 0 on the same extension; moving catch, loaded climb (0.175422 rad span), timed departure, offset receiver and +121.9 m Tower walking complete. All [six captures](captures/) were explicitly opened and inspected. The yellow moving rail and glove contact are visible in the catch; rail rotation and ascent are visible at departure height; receiver and Tower exit show actual arrival. The release-height crop only partly shows the glove; snapshots do not quantify continuous hand smoothness. This is the existing rigid AS-026 ladder, not the new flexible rope ladder.
- Same-extension Godot bridge cargo walk-up/hold probe observed `HAND_BRIDGE PASS`, with two render getter samples at half-tick and no native tick advancement. Its final raw transcript was not separately retained; receipt fields record the actual observed output.
- Super_Agent separately reviewed the supplied native correction and new campaign/CI diff. No remaining supported Important/Critical introduced defect was reported. Its advice was checked against actual source; it had no local repository or runtime access.

## Verification limits and failure classification

The captures use a private 432×371 LOW-quality softpipe harness, shadows/MSAA off, with rendering enabled around explicit pose captures. Native stepping and shipping touch continue. They prove bounded visible/collision integration, not continuous-render performance or console-quality acceptance. The original llvmpipe attempt terminated with a local SIGILL; softpipe completed. The harness is not production source. Dummy audio supplies no sound proof.

Uninterrupted grade→121 is headless proof, while rendered proof is the staged upper slice. Exact-source Actions, APK identity and checksum remain pending for this candidate. Existing launch delivery `f23a68e` remains the latest verified APK until a successor is closed. Android play, comfort and sustained performance remain unverified.

Non-Kit fixture acquisition still uses the current physics hand endpoint when its owner has no prior pose history. Runtime mixed-owner transfers and new net grip coordinates are not separately established by these checks. Broad contact-aware parkour and the breeze-driven flexible ladder remain further work; this repair implements neither by implication.
