# Loaded stair handoff: actual high-footing loss

Exact `ChatGPT` source `6b8dc272af29576ba847e9761f91b155f83db11e`, [run38066022945](https://github.com/IssisX/ScraperX/actions/runs/38066022945), [job114253718878](https://github.com/IssisX/ScraperX/actions/runs/38066022945/job/114253718878), failed athletic **first**, after38.57s. Stop-on-first-failure left the other53 native gates unexecuted; Godot/Android did not run and no APK was exported. The [prior d1af237 result](../ci53-followup/README.md) remains53/54 passing, including the earlier cage and stairmode2 actual x86 passes. Do not attribute those53 passes to this latest run.

The new actual endpoints discriminate the failed +24m handoff assertion:

| Observation | Player Y | Support | Contact Y | Held block | Deaths |
| --- | ---: | ---: | ---: | ---: | ---: |
| Final climb5, tick19048 |25.0708|44|24.1872|56|0|
| Handoff walk returns `waypoint_reached=1`, tick19688 |9.14783|1002|8.25|56|0|

High footing was lost during the handoff walk; cargo remained intact. The exact first support departure/contact is not observed. Native top-stair footprint X[-9,-7], Z[-121.8,-114.2] overlaps receiver X[-10,-2], Z[-117.5,-107.7] by2m×3.3m at the same24.1872m top. An ideal straight line is geometrically continuous. These endpoints do not establish a geometry gap, corner-cut or precise contact cause.

Super_Agent owns the **in-progress, unverified** `tests/simulation_tests.cpp` correction: contact-aware centre entry(-8,-113.5) followed by existing(-9,-109), sharing the original12s budget, retaining actual receiver support/held-block proof and logging first support loss. Root owns integration and delivery. No correction pass or next source SHA is claimed. Early rejection now avoids the long remaining gate wait; the same54 gates remain required once on full success.

[ci-failure.json](ci-failure.json) records exact observations. Full private log: `/data/data/com.termux/files/usr/tmp/scraperx-ci-green-oct10/ci-6b8dc27-failed.log`; SHA256 `b6a0f54e561e2fe540fef6e13d3798dbfbdc5c6571416b7d8626eb6833cca75a`. The giant log is not copied into the repository.

New gameplay remains behind full green exact-source Actions and a verified matching APK package/version above213671847/signing/provenance, followed by its download link. Owner waived Android Download copying; no install/device observation is claimed. Preserve the full227-line canonical scope, phase weights and overall61%; this driver/CI work adds no gameplay-completion credit.

Implemented and reviewed: ordinary PD transfer now centres through (-8,-113.5), proves actual settled receiver45/contactY24.1872/held56, then moves to the original(-9,-109). One12-second/1080-tick budget is shared. All754 require calls/messages, earlier flights, native physics and geometry remain unchanged. Final affected ARM build/full athletic execution passes27 receipts; transfer uses636ticks/7.07s, real receiver contact/block retained/deaths0, later40m carry and checkpoint restoration also pass. [Exact implementation/limits](IMPLEMENTATION.md) and [manifest](manifest.json) retain proof. The small ordinary approach variation showed brief upward support loss but still landed high; it did not reproduce the exact x86 lower-deck fall. Full exact-source x86/Godot/Android/APK delivery remains required.
