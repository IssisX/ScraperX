# Parkour feel delivery — 2026-09-26

## Exact source and artifact

- Gameplay source: `3034691f8a2e62cb3a9849d9cc0001dd05d37e30`, branch `ChatGPT`.
- [Workflow run 36271706982](https://github.com/IssisX/ScraperX/actions/runs/36271706982), job `108486786199`, completed successfully at **2026-09-26 21:29:48 UTC**. Every required step passed.
- [Artifact 10915474571](https://github.com/IssisX/ScraperX/actions/runs/36271706982/artifacts/10915474571), `ScraperX-build-3034691f8a2e62cb3a9849d9cc0001dd05d37e30`, expires 2026-12-25.
- Downloaded archive SHA-256 matches GitHub's artifact digest: `65af140021f7c6cfee670d784ff9338daa42c3748b495fe33f449c9249c10bf6`.
- APK: `ScraperX-3034691f8a2e-arm64.apk`, **32,822,007 bytes**. APK SHA-256 matches its packaged checksum: `03097a19400d8012292deb4a62bd4491eb2a15b751ab8c1a92fa6ba3ff51b927`. ZIP integrity passes; packaged `lib/arm64-v8a/libscraperx_native.so` is present. The artifact's `CHECKPOINT.txt` names the exact gameplay commit.

## What changed

- Native vault travel uses a monotone cubic path whose entry and exit slopes carry the measured approach speed along the real landing path. Collision sweeps, target support and exit momentum remain native-owned.
- The visual camera interpolates between the two most recent 90 Hz native player poses. Death/respawn, crouch-body changes and large discontinuities snap the visual history so the view never sweeps across a teleport.
- Head bob and speed FOV ease with frame-rate-independent response; a maximum 1.5° strafe lean follows actual horizontal velocity. The landing dip now has zero slope at its start and end. Motion-comfort toggles cut bob/lean and speed FOV immediately.
- The GDD's conditional backflip remains a later task; this build does not claim that move.

## Verification

| Gate | Observed result |
|---|---|
| Native full suite | All nine CTest entries passed. New vault proof measured 5.5 m/s approach, 5.60 m/s first vault tick, 5.64 m/s last vault tick and 5.5 m/s exit; it crossed the actual rail without abort. Previous straight path entered at 6.69 m/s. |
| Render interpolation | Native test proved the visual pose is halfway between fixed poses at half-step remainder, while the physical snapshot remains unchanged. Godot main-scene test proved the camera uses that pose. |
| Camera feel | Godot main-scene test proved gradual FOV, bounded lean, bob fade after takeoff, exact stationary baseline and immediate motion-comfort toggle response. |
| Existing traversal | Native athletic suite and all retained viewport input scenarios passed in CI. Local ARM64 Godot touch vault passed at 30, 60 and 120 FPS; climb, double-tap vault, crouch and active pipe bridge also passed. |
| World and delivery | Rendered foundation, active pipe bridge crossing/tower arrival and regression scene captures passed. Audio, both solid-table checks, Android ARM64 compile, APK export and publication passed. |

I visually inspected the published foundation and bridge-arrival screenshots for a live first-person world and working tower arrival. Local ARM64 software-rendered capture crashed in the graphics driver; the x86 CI rendered gates passed. An APK installation, subjective movement feel on the Fold, sustained device frame rate, and the deferred backflip are not established by these checks.

The later delivery-record commit changes documentation only; it does not change this APK's gameplay source.
