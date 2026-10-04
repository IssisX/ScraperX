# Contact movement: rigid-hand checkpoint

**Scope:** the begun rigid-hand integration and passive departure repair, preserved at the owner's requested pause. The larger GDD §7.2 movement objective remains unfinished. Base source is `543d13e04c615af6869d46826214878446513200` on `ChatGPT`; production code and tests are committed with this evidence. Current-source hashes and the ordinary replay result belong to `traversal/hand-runtime-source.json` after that replay closes.

## Implemented

`PhysicalHandClimb` owns two independent finite SixDOF hand constraints between the actual translation-only 85 kg player and eligible rigid holds. `PhysicsWorld` owns acquisition, actual reachable geometry, regrip, commands and lifecycle; Kit retains the support bodies. Rigid `Climbing` keeps gravity on, skips the old root-velocity driver and separate weight force, and retains actual foot contacts and landing consequences. Constraints clear before reset/restore/body teardown. Public traversal identifiers and existing Godot presentation ownership remain intact.

Hand parameters are CHOSEN: 5,000 N/m stiffness, 180 N·s/m damping, 1,500 N vector force limit, conservative shared 3,000 W target-command bound; local joint solver 40/8 retains global 10/2. Native motor impulses give reciprocal reactions. The command debit is a bound, not measured positive muscle work or a closed energy ledger. Production force readback samples the last collision substep; the module fixture observes all collision-step boundaries.

Passive hang/climb release and stepping off remove attachment without replacing actual player velocity. Rotating/deforming support point velocity remains an airborne steering reference. The selected principal hold establishes soft/rigid hand consistency before either hand acquires; cross-backend regrips and carrying candidates await the later material adapter. The [read-only source review](source-review.md) found that boundary defect, then confirmed its source repair.

## Actual verification

- Standalone hand module: actual red 0/5, final green 5/5. Gravity-on sag/load, one-hand redistribution, compliant oblique catch to the 1,500 N vector cap, reciprocal 25 kg support recoil, shared command bound, unchanged release momentum and invalid/soft acquisition cleanup are exercised. All component fixtures assert zero external contacts. Exact frozen source, commands, logs and limitations are in `module/`.
- Passive departure regression: the original rigid vent loses 0.900364 m/s and the net gains 2.46207 m/s relative to gravity-only departure. The repair passes rigid, cargo-net and rotating AS-026 cases with approximately 3.24e-8 m/s error. These native cases use explicitly labelled staging where needed; the net approach is ordinary grade input. Original red and green logs are retained.
- Reviewed integration: strict native/bridge build exits 0; five affected CTest gates pass: physical hand module, physical traversal, suspended ladder route, cargo-net route and parkour flow, 53.80 s total. The new traversal assertions inspect actual gravity, hand-constraint count, command debit and cleared release state.
- The integration's first ordinary headless campaign passed grade→121.9 m/support 11/deaths 0/launcher work 0. It predates the final soft/rigid guard correction and is labelled `*-before-review`. The final reviewed-source replay then exits 0 on the unchanged shipping driver: ordinary grade→cargo net→upper machinery→loaded AS-026→+121.9 m/support 11/deaths 0/launcher work 0. Final pose is `(9.00225,121.9,-174.5659)` at reported elapsed 236332 ms. Its exact source/extension/driver and log hashes are in `traversal/hand-runtime-source.json`; this is headless runtime evidence.

## Remaining boundary and restart

Cargo-net climbing, `Hanging`, receiving `Mantling` and powered jump still use their existing controllers. Flexible material coupling, independent mixed-support transfer, real foot balance, powered reciprocal departure and full controller energy closure remain work. Existing reach/mass/Box-leaf restrictions remain; low-mass cylindrical ladder rungs need their registered geometry query. A one-hand vertical motor has only about 32.18 N upward margin beyond player weight; the fixture's equilibrium required 16 s, and a real oblique catch drops roughly 1.70 m. These are measured design limitations, not a device smoothness verdict.

The new native checkpoint has no new rendered-hand inspection, Android APK or device acceptance yet. The immutable green audio/movement APK at `543d13e` has its separate ledger. Follow this checkpoint's exact-source Actions after publication; do not transfer an older APK's green result to it. On explicit continuation, use `CONTACT_MOVEMENT_IMPLEMENTATION.md` for the next coupled contact slice and `CONTINUE_HERE_CHATGPT_CODEX.md` §7 for the running to-do.

The completed rope-ladder and freight-gallery investigations are saved separately in `evidence/west-gallery-compilation-2026-10-03/`. Their accepted component and rejected assembly receipts retain their own scope. The owner requested a pause before another stage.

## Publication and pause

Native source `61361c6b5db417da8ec5003f4e671297cff4fe27` was pushed to `ChatGPT`. [Exact-source Actions run 37162561352](https://github.com/IssisX/ScraperX/actions/runs/37162561352) is in progress when paused; `actions-at-pause.json` is the actual API observation. All workers and local check processes have finished. Android/full-regression/rendered delivery for this new candidate remains unverified. Check this run first after the owner's continuation instruction. Documentation-only successors leave this executable source unchanged.


## Delivery observation, 2026-10-04

The run pending at the historical pause has now completed with failure. [The failure checkpoint](ci-61361c6/README.md) records the five failed/not-run native gates, fresh three-case local reproduction, supported source observations and unresolved velocity/impact discriminator. Android/rendered delivery did not run. No gameplay or workflow repair is included in this documentation-only checkpoint; all workers and checks are finished and the owner-requested pause remains in effect.
