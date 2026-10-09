# Beam viewport-touch evidence

The focused actual-beam correction check **passes**. The separate connected staged77→88 touch attempt **fails before reaching the inclined girder**; no connected touch pass is claimed.

Both use bridge `5432f105cb3401f86f6f66f744be6a63c3521e990edebd30b52b03d99815c0c4`, shipping Main/input routing and maintained `touch_braced_bay` world selection: **PipeBridge fixture**, not the full normal campaign. Each private harness permits one initial supported stage only. Native full-route acceptance belongs to the parent evidence, separately.

## Focused changed behavior

[Focused log](touch-focused-beam.log), [receipt](focused-receipt.txt) and [private harness](focused-harness.gdpart) record actual native-supported girder staging at `(28.12,80.27272727,-137)`, then ordinary viewport-touch inputs. Godot4.7 ARM64 headless/Dummy/fixed60 run exits0. Four27-native-tick holds retain grounded balancing entity1900, gravity1, finite position/velocity and zero deaths at every checked viewport frame.

| Input reaching touch router | Actual lateral result | Lateral velocity after hold |
| --- | --- | --- |
| +.25 | +.036983490m | +.125m/s |
| neutral | +.000507355m; no inward recenter | 0 |
| −.25 | −.036983490m | −.125m/s |
| final neutral | returns X28.12 | 0 |

Natural down-slope motion remains `Vz+.049545/Vy−.022521m/s`; lateral braking does not establish whole-body static equilibrium. Logged recovery work reaches1.12324478J. This proves the changed input-driven balance correction through the actual touch router, without later relocation or velocity writes. It does not prove connected ascent, human phone feel or rendered visual quality.

## Connected route attempt retained

[Route failure log](touch-braced-bay.log), [receipt](receipt.txt) and [private harness](touch-harness.gdpart) retain initial real Tower11 support at `(25.1,77.9,-132)`. The preceding `_touch_teeter` prerequisite alone was bypassed. Existing flat entry `_walk_to(28,-131,.14)` declared success, then the maintained pose was `(29.00566,77.9,-130.6556)`. The added ordinary supported approach to the correction probe point stopped at `(28.00002,77.89872,-137.2413)`, with entity1900 grounded on the77m recovery deck and `balancing=false`, rather than inclined footing. Deaths remained0. Gentle/neutral/opposite, gap, crouch, return mantle and88m arrival were never reached. The entry helper's residual-velocity overshoot is a supported hypothesis; this run does not isolate a native beam defect. No route rerun or helper adjustment followed.

An earlier [startup guard rejection](campaign-guard-startup.log) occurred before staging because the private harness incorrectly applied the normal-campaign identity guard to this fixture scenario. After parent agreement, only that added guard was removed. It is separate from the actual route failure. [Import log](import.log) records successful full-current-source private import.

[Manifest](manifest.json) hashes the logs/harnesses/receipts. Both run-specific source receipts match start/end. No maintained source or geometry changed in this proof.
