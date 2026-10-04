# Recorded-footstep integration receipt

PASS: resource check (3 surfaces, 12 distinct recorded 16-bit variants), all 5 contact cases, and shipping-scene audio integration.

UiTestDriver audio_mix: bed −23.2 dBFS, steps −13.0 dBFS, prominence 10.2 dB, peak −0.8 dBFS, 10 steps; exit 0. All original gates remain unchanged. 160 frames at 30 FPS completed in 32 wall seconds.

Runtime boundary: unchanged production native/input/controller/pool/buses/Master and default audio 0.8/1.0/0.8/0.7, unmuted. Private LOW graphics adapter used existing settings APIs. The successful audio-only follow-up disabled viewport 3D drawing after its first real Xvfb Compatibility softpipe frame; 3D listener/current-camera stayed true, native ticks 2→95, bank ready, mixer not silent, 6 Effects voices, wind/drone/positional hum playing. This proves scene audio integration, not complete 3D rendering. Standard 3D mix remains a separate CI gate.

The initial fully drawn LOW run timed out with exit 124 at 300 seconds after 142 steadily rendered frames (4.73 game seconds), without a UiTestDriver verdict. Continuous AVI progress and CPU-running state support environmental software-rendering cost. This is not an audio gate failure.

Source snapshot: ChatGPT, HEAD ac45fa090667e6d4763bc35be0a47794cdb73c1c plus parent uncommitted integration. Canonical 81-file source manifest SHA256: 93038babdc469703f819eb65b94a53d8eca4a734c5f5e3efa1048a96edbe0362. Production input/audio owners and all 12 WAVs matched current repo; subsequent SoundBank difference was header comments only. Private project viewport 192x164/autoload and Linux extension feature keys x86_64→arm64 were the only adapted copied files.

Native SHA256: 9ff20444d0733bdcf6ff2353ee4d80aa2025f8c39d9b3e324d1d08a75f615198.
LOW adapter SHA256: 5ad31ed5941e7fa057464083285b44b9d92c18d696e41df74acef8f52db16fd4.
Render-omission adapter SHA256: f6f558974c4b9bb52b0afef3bdd876b48b13d2b6059fa49c08aa048b7d2fa1d0.

Attached logs are copied directly from the actual runs. Detailed manifests, exact commands, both adapter sources and AVI files remain in the parent proof directory. No Android/APK or notice-packaging verdict is claimed.
