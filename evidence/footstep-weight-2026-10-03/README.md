# Footsteps: weight, material texture and honest cadence

Owner report: the footsteps sounded thin and repetitively metallic. This source follow-up changes the three existing step recipes and connects footstep cadence to native support-point velocity. It is separate from delivered movement source `0ecf341`. Exact-source Actions/APK is delivered at `543d13e04c615af6869d46826214878446513200`, run `37158452295`; the immutable receipt below records the verification. GDD §24.1–24.2 records the paid-product quality and living-soundscape direction.

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

## Exact-source delivery closure

[Run 37158452295](https://github.com/IssisX/ScraperX/actions/runs/37158452295) succeeded at source `543d13e04c615af6869d46826214878446513200`, job `111306655572`. [Artifact 11287534104](https://github.com/IssisX/ScraperX/actions/runs/37158452295/artifacts/11287534104) contains `apk/ScraperX-ChatGPT.apk`, package `com.cory.scraperx.chatgpt`, label `ScraperX-ChatGPT`. Independently calculated APK SHA-256 is `699a05d551a3db21449b5a553d127f03bf1fbd15becd2c46ae4bbb6aca3c5824`, matching `SHA256SUMS`; ZIP SHA-256 `28adc9e7b14447b7c94a692a2541a11c4441dd1cf2ea048ef7da669bc8af2849` matches GitHub metadata. Actual CI signing logs verify v2/v3 and one stable signer `9a06889e7614d140f6cd1fc45634bb1e2391968a9fcc60f1608c9e50e15a1ba4`.

CI passes 31/31 native gates and 5/5 footstep-contact cases. Shipping audio mix passes at bed −22.7 dBFS, steps −14.6 dBFS, peak −0.8 dBFS and ten steps. Uninterrupted ordinary shipping headless grade→cargo net→upper route→loaded AS-026→+121.9 m finishes on Tower support 11 with zero deaths and zero launcher work. Three actual CI images were opened: +99 m supported ledge, +104.9 m frame hang and launcher supported +352.9 m roof. These rendered fixture/launcher captures retain their own scope; they do not establish later hand integration or flexible-ladder gameplay. [CI receipt](ci-543d13e/receipt.json) and exact retained logs/images record the proof. Device listening and Android execution remain open.
