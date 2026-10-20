# Mounted finite winch — first Milestone 4 interaction

Normal freight-yard approach → RIG → clip marked winch eye → clip beam eye → hold UP/DOWN → release for brake → stow/leave/return. Bay is beside the counterweight at (-108,0,-36). The ordinary 240kg beam can tip on grade; it has all six physical degrees of freedom. No path motion, frozen load rotation, invisible guide or new cable solver.

The existing PoweredCable owns reciprocal load, 6000N pull/brake, .25m/s maximum command, 2000W source, .8 efficiency and 250000J finite device capacity. These are declared gameplay tuning values in mounted_winch_bay.cpp. Installation/restoration preserve source receipts; reclip does not refill the device. Cable twist/torsion, bending/remeshing and detailed cable research remain deferred by owner instruction.

## Executed evidence before publication

Workspace /data/data/com.termux/files/usr/tmp/scraperx-portable-line-20261020 (P); source write owner P/repo, ChatGPT, following accepted ad10799. Actual command arrays and outputs, not model replies, are retained:

- `python P/winch/owner/check.py rigging_powered rigging_system` → strict builds and actual owner fixtures exit0. Real100kg lift .499571m, payout .246378m, source3749.08J; neutral hold, stow/range/context/selection stop, cold restore, retained reclip ledger, dynamic support reaction, weak mount, empty source and passive rigging regression. Receipt P/winch/owner/rigging_system-commands.json. One representational float ULP is admitted at the exact powered overload rating; real weak-eye overload remains required.
- `python P/winch/build.py` → coherent33-translation-unit native world closure exit0. `python P/winch/build_bridge.py` → three matching bridge units and final Linux ARM64 extension link exit0. Commands in winch/contact-closure-build.json and bridge-commands.json.
- `python P/winch/check.py mounted_winch_public` → exit0, `PASS MOUNTED_WINCH_PUBLIC arrival=1 paid_lift=1 brake=1 lower=1 loaded_unclip_rejected=1 stow_persists=1 range_stop=1 return=1 onward=1 deaths=0 resets=0`. Loaded beam COM .600367m versus .179729m on grade; brake tension1392.38N; source5624.11J. Receipt P/winch/mounted_winch_public-commands.json.
- Actual native Godot MCP `run_project` on P/winch/runtime, then `run_script` loading shipping Main and mounted_winch_input_case.gd → checks22, failures[], tick845. Touch install/lift/lower/release/cancel/stow, native source/work readback and spatial gearbox-loop scheduling. Receipt P/winch/godot-mcp-winch.json. Source is the actual Main/native graph; one staged starting position only. No heard-audio or device-feel claim.

MCP rendered screenshot commands timed out at10s and60s on this host’s2160x1856 Mesa softpipe renderer. Debug readback remained running with no game/script errors; ALSA fell back to Dummy audio. No rendering success claimed. Stopped the MCP game cleanly. Use the1280x900 CI capture for this candidate; no further local screenshot campaign.

The full suspended crossing/reconfiguration part of M4 and revised M5–7 are pending; this bounded winch interaction does not complete those milestones. Publication and matching green APK receipt will be recorded in the live checkpoint after actual CI acceptance.
