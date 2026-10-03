# Footsteps: weight, material texture and honest cadence

Owner report: the footsteps sounded thin and repetitively metallic. This source follow-up changes the three existing step recipes and connects footstep cadence to native support-point velocity. It is separate from delivered movement source `0ecf341`. Exact-source Actions/APK remains pending. GDD §24.1–24.2 records the paid-product quality and living-soundscape direction.

## Source-supported repair

The old steel recipe has a bright click and sustained inharmonic ring; its lower weight sits mostly at 80–96 Hz. Actual exports show a spectral centroid of 1936 Hz and 12.51% energy remaining after 70 ms. Revised steel has a broad boot-body knock, short damped plate response and small sole-texture tail. Concrete combines a substantial knock with grit; earth combines packed-ground weight with granular crunch. Cue names, lengths, four variants and seeded draw counts are retained. Every one of the 38 non-footstep WAVs remains byte-identical.

Footstep distance now uses player velocity minus native velocity at the support point. That readback includes rotation. A carried stationary rider therefore supplies zero walking distance; walking relative to the support still supplies distance. World velocity continues to drive airborne/environmental feedback.

## Actual evidence

- Godot 4.7 parsed and exported all 50 old/new WAVs. All 12 steps changed, peaks remain safe and endpoints silent. `analysis.json` retains exact hashes and per-variant spectra. Steel centroid changes 1936→500 Hz; its >70 ms energy tail changes 12.51%→0.108%, while 300–6000 Hz RMS changes −0.92 dB. These are measured timbre/decay changes, not a subjective quality verdict.
- The real AudioDirector regression fails against the old API/cadence in three moving-support cases. After integration all five cases pass: passive translation, relative walking, rotating contact, fixed floor and airborne. Baseline probe supplies the support argument when the API offers it; the shipped test calls the new contract directly.
- Actual shipping `audio_mix` input scenario passes with Movie Maker mixing: ambience −22.7 dBFS, footstep window −15.3 dBFS, peak −0.8 dBFS, ten footsteps. The retained scenario rejects inaudible ambience/footsteps and clipping. Local software rendering/3D suppression bounds cost; this is audio evidence, not graphics/performance proof.
- Independent scoped Super_Agent review found no introduced Important/Critical in the supplied diff. No external execution or listening is attributed to that review.

`receipt.json` owns source hashes, checks and boundaries. `before/` and `after/` contain exact exported step variants. The six `<phase>-<material>-matched-rms.wav` files concatenate all four variants at equal RMS for gain-neutral comparison. They are review artifacts, not gameplay assets. `analyse.py` requires its original full before/after bank to recheck all 38 unchanged cues; both complete banks are retained here so its 38-cue preservation check can be rerun. The unchanged cues share identical Git blobs.

## Remaining sound work

Listen through actual target-device speakers/headphones and ordinary repeated traversal. The component regression is not a native moving-platform playtest. Existing surface mapping remains concrete/earth/steel by support identity; a richer native material query, sustained contact-slip scuffs, load/strain/rolling/crush ambience and spatial refinement remain tracked work. The new short scratch is authored into a step, not a measured dynamic slip cue.
