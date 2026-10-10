# Bounded viewport-touch delivery preflight — 2026-10-10

Baseline source: `ChatGPT` / `9280e2eef2fe208bf775917143b4b897060e9f24`. Native54 passed actual x86 Actions at this source. These separate local Godot4.7 ARM64 preflights do not establish full green Actions, a new APK, or installed-phone behavior. No native rerun or complete fixture rerun was added.

## Frozen input fixtures

The original23 fixtures each ran once: **21 PASS / 2 FAIL**, preserved in [fixtures-SUMMARY.md](fixtures-SUMMARY.md), [fixtures-results.json](fixtures-results.json) and [fixtures-source-manifest.json](fixtures-source-manifest.json). These are baseline results, not final repair status.

- `touch_move_look` failed its old sprint timing. Separate finite-acceleration/braking diagnosis and corrected exact-driver execution now pass; see [sprint receipt](../touch-sprint/README.md).
- `touch_carry` failed expected block56 pickup. [Actual diagnosis](carry-diagnosis.json) shows Action consumed, grounded support1 retained, and bar55 held throughout: the approach selected the wrong real carryable. The ordinary target-specific correction is a separate candidate; final carry pass is pending here.

The suspended-ladder shelf correction has its own [complete local ladder receipt](../touch-shelf/README.md). Super's shelf/sprint/carry/cart changes are separate from these frozen baselines; root owns integration and updating final carry/cart status.

## Four independent normal-world routes

Each route executed once after the runtime could load. Exact commands, exit codes, terminal messages and log hashes are in [normalworld-results.json](normalworld-results.json).

| Route | Result / seconds | Actual observed outcome |
| --- | --- | --- |
| [Ballast mantle](normalworld-touch_ballast_mantle_feedback.log) | PASS /68.594 | Stable roof3s, real hand/foot transfer, return beam and Tower, deaths0. |
| [Slab-haul cart](normalworld-touch_slab_haul_cart.log) | FAIL31 /372.621 | Actual cart walk-off, but recovery settled on1954 near350m rather than required341 apron1956. |
| [Taper inspection](normalworld-touch_taper_inspection_route.log) | PASS /159.544 | Ordinary2m gap Jump, middle miss apron, blocked standing header/crouched bypass,374.25 floor and onward3.518m; support51, deaths0, gravity1. |
| [West brace](normalworld-touch_west_brace_bay.log) | PASS /147.702 | Bar catch/traverse, reverse shelf, final396.25 transfer,270 hold ticks and onward3.23m; support51, deaths0, gravity1. |

Cart before miss: `(-29.1384,352.9182,-141.585)`, support2320, grounded. Departure: `(-27.29347,352.9091,-141.5405)`, support0, airborne. Recovery: `(-26.89983,350.0447,-141.7217)`, support1954, grounded, deaths0; supported-arrival returned true, and the strict1956 requirement rejected it. The cart correction/final pass remains separate and pending.

[Source manifest](normalworld-source-manifest.json) and [receipt](normalworld-receipt.json) bind driver SHA `d1c2c36a9ff56817de2ced0dd05e21d696e8bfa29beb6e5ede7f29cf11bd50a4`, bridge SHA `8c5f73ee23e77f24ab09d3842e3a95da7d1942b5425b2931649475b47671d20c`, and74 matching native-source hashes. At completion all four route functions matched maintained source; concurrent sprint/ladder changes were separate. Initial launches exited20 before gameplay because tracked Linux x86 selectors replaced the private ARM selectors. Only the private architecture mapping was restored; this environmental startup failure is recorded in the manifest and is not a route failure or gameplay rerun.

## Connected campaign remains incomplete

`connected-touch-campaign-zb2kb9b8` ran once for600.0287 host seconds, then exited124 at the environmental wall-time limit. Last observation: tick14979, `(5.413736,61.77796,-122.9987)`. There was no gameplay FAIL line, no supported121/143 observation and no campaign PASS. [connected-receipt.json](connected-receipt.json) and [connected-campaign.log](connected-campaign.log) retain the exact frozen driver/bridge boundary and shutdown output. This incomplete run cannot verify the later ladder/lift connection.

## Final bounded repair status

All four affected functions now pass localARM replay: settled ladder approach, finite sprint/release waits, actualblock56 offer/pickup/carry/apron, and actual341cartmiss/landing/paidrecall/retry/fullreset/Towerexit. Root carrycandidate10.57 clears the realpedestal; first10.68candidate failure is retained in touch-carry-cart. Exact unchangedfunction hashes reuse the21fixture/3normalworld passes. No repeatednative or fullfixture campaign. Native54 already passedx86; fullGREEN exactsource Actions andAPK remain pending.
