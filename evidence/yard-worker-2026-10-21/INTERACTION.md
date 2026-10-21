# Opening-yard worker — 2026-10-21

A welder now works beside normal FreightYardArrival. Walking close makes him stop welding, face the player and point toward the existing crane yard. The player retains ordinary touch control, can walk around the cargo beam and can physically bump him. After the player passes, he returns to his bench and resumes work. No interaction button or movement cutscene is added.

The real85kg capsule uses the existing PhysicalStoryActor finite walking law:220N,300W,1.4m/s; this is a reduced-order upright body, not articulated human physics. Jolt contacts supply footing and reciprocal collisions. A fixed two-slot contact buffer retains independent final-substep receipts for this worker and the earlier story actor. Checkpoints save his body position, momentum, activity and intent; restore invalidates support until the next actual solve.

The real static steel workbench is Kit entity63202; worker63201 starts at(-89,.9,-50)m. Native activity drives the imported clothed body, enclosed welding mask, arm gestures, electrode and ten bounded sparks. The small cached broadband welding loop is procedural environmental sound; boots reuse the existing recorded footstep assets. Pause, absent support and the stopped electrode silence the relevant cues. Native welding activity is environmental work, without a metallurgy/structural-weld simulation.

## Evidence

- `NATIVE.json`: coherent ARM64 native/bridge producer identity and actual command/output. Existing physical/story actor checks exit0. New public normal arrival, notice, pointing, onward path, return, exact checkpoint and collision checks exit0. Actual shove moves worker-.171883m in X and reaches1.79387m/s while its drive force remains bounded. The original unchanged600kg beam-ramp route also exits0 with the relocated station.
- `GODOT_MCP.json`: actual shipping Main, real touch events, no position/contact/activity injection;12 checks, no failures,1407 native ticks, complete return to work. Reproduce with the shipping `yard_worker_input_test.gd`; this source is now a candidate-CI gate.
- `PRESENTATION.json`: implementer's isolated fixture,28/28, actual garment/hood/gesture render inspected. This is distinct from the integrated native/touch test.

Full-world screenshots on the Mesa softpipe host timed out; the completed screenshot contained HUD only after rendering was disabled and is not visual acceptance. The actual native/gesture/sound-state path is proven, but whole-yard appearance, audible output and Fold6 frame time still need matching device observation. No production rendering quality was lowered. Separate pre-existing full-release rail/west route failures remain outside this feature's acceptance; a green APK candidate is not full-release certification.

## Placement correction

Candidate39642854365/source167aa106 exposed an introduced regression: the first bench location blocked the existing loaded forklift lane. The station is now east of that lane, with bench and seam derived from the same native home. The original beam route and physical laws are unchanged. New worker fixtures follow the clear approach and retrace it before passing the existing cargo beam and crate; longer travel budgets cover the longer walk. Actual native and Godot MCP paths pass with the new station. Matching candidate acceptance remains a separate publication result.

## Render repair in the combined delivery batch

Sourcef2bf0b8 candidate39647297796 passed the worker, body and original loaded-beam gates, then exposed1011draws in the crown-to-ground render stress view against the existing<1000limit. That limit and shipping shadows/whole-tower visibility remain unchanged.

Root batches all ten identical sparks into one MultiMesh and joins only worker garment surfaces with identical material, Skin, skeleton, transform, format and shadow/layer settings. Every original vertex, triangle and bone weight remains. The first-person body owner/assets and all native laws are unchanged. Actual Godot MCP paired rendering: sparks22→4 near/10→1 at1590m; near spark images match exactly. Garment working30→24draws, pointing/moving24→18; maximum image error1/255 in the working pose, exact pixels in the other two. Garment data/render41checks and affected worker behavior31checks pass. Exact calls/results/source hashes and limits live in `RENDER_BATCH.json`. Whole-world budget and matching APK acceptance remain separate pending CI observations.

The same candidate workflow now caches compiled C++ work and saves the cache before later presentation gates. This is configured build reuse, not a measured build-time claim. Focused checks cover partial edits; the complete connected feature/repair is one APK batch.

## Tuning

`src/sim/yard_worker.cpp` owns the2.2m notice radius, .8s attention before pointing,2s pointing interval,5m rearm distance, .9m/s return and1.2m/s yield targets, all below the existing actuator speed bound. `yard_worker.hpp` owns the authored home, work seam and crane guide points. `yard_worker_view.gd` owns garment/gesture appearance; `yard_worker_audio.gd` owns only source gain, spatial falloff and presentation sound. These cannot redefine contact, body position or actuator limits.
