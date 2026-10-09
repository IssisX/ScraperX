# SCRAPERX — RELEASE PLAN (proposal, 2026-10-09)

The owner (2026-10-08): *"This game will be public-facing and real money will be exchanged for it.
It must look, feel, and play like it."* This plan merges two read-only audits of the tree at
`ea58a2b`. The first audit covered the route, look, feel, technical risk, shortcuts and world
pressure, and three competing plans were critiqued against the owner's rules. The second covered
what a paid Google Play release needs. Every serious finding of the second audit was re-checked
against the source by a separate agent, and none was refuted. Nothing below has been built.
Items marked **OWNER** wait on a decision in §3.

## 1. Where it stands

- **The route.** It exists from grade to 802.64 m, but no single run crosses the 750 deck.
  `touch_stack` ends there and `touch_high` starts from a fresh spawn. The canonical branch (the
  220 ring, swing, C6, mast, pitman, helix, TP-340) is proven only in four separate pieces. Death
  mid-ride is proven only on the cascade mast.
- **The player can't keep progress.** Only settings persist (`settings_store.gd`). An Android
  background kill loses the climb.
- **The opening and the ending.**
  - The game opens on Godot's stock splash and icon, with no title.
  - Nothing happens at the top.
  - The altimeter is scaled to 1,600 m, so the top reads as halfway (`simulation.hpp:681`).
- **Every build is a debug build.** There is no app bundle (AAB), no release key and no
  crash symbols. The APK ships no licence notices.
- **Nothing has been measured on a device.** That covers frame time, heat, battery and load time.
  The world is about 2,000 unbatched meshes. Determinism is proven only on x86, not on the ARM
  phone.
- **The machines don't feel carried.**
  - Machines are drawn unsmoothed while the camera is smoothed, so every ride judders.
  - The rider's camera bobs as if walking.
  - Height stops being heard at 150 m.
- **The look reads as greybox.** Surfaces are flat colour with no texture, wear or grime. Sky
  reflections are off, so metal reads dark. Ropes are 3.5 cm lines, and machines have no lamps
  or markings to show cause and effect.
- **Already clean.** No third-party art, fonts or sounds ship (everything except the voice is
  generated in code), and there are no ads or analytics.

## 2. The plan

Each item is one complete, verifiable unit, proven through the touch path on the route as built
(rule 2.5).

**A. Truth first (no owner decision needed)**

1. **The whole climb in one run** (L). `touch_summit` plays from grade to 802.64 m with no spawn
   in between, plus a matching native chain. It becomes a CI gate. A second run takes the canonical
   branch through TP-340 and on into AS-008.
2. **Nothing strands** (M).
   - A mid-ride death-and-restore falsifier for every machine.
   - The incline, wheel and tram exits joined to the next stage on touch.
   - The stone wheel's rides per hopper counted.
3. **The route proven on ARM** (M). The native suite runs on an arm64 CI runner, plus a
   cross-architecture world hash (rule 10).
4. **Rides feel carried** (M).
   - Kit bodies drawn interpolated at the camera's blend.
   - Bob and lean taken from motion relative to the support.
   - A ride readout (name, height, speed) instead of checkpoint toasts.
   - Hold-to-reverse on touch.

**B. A product someone can buy**

5. **Continue and title** (L).
   - Save the exact last checkpoint, machines included. Proof: save, load and advance N ticks is
     bit-identical to the uninterrupted run.
   - A title screen over the live tower, with CONTINUE and NEW CLIMB.
   - The dev START list gated to development builds.
   - A confirmation before restart.
6. **The top** (M). A native predicate on the route's top, the altimeter scaled to it, and an
   arrival beat. What stands there is **OWNER** (D5).
7. **Release pipeline** (L).
   - A release preset under the final id (**OWNER** D1), built as an AAB in release mode with the
     test driver excluded.
   - An upload key in a protected environment, plus Play App Signing.
   - Native symbols kept for crash reports.
   - A Licences and Credits screen.
   - The icon and splash.
8. **Device baseline** (M, needs the owner's Fold).
   - The release build gets an in-game performance log the owner can share from the phone.
   - It covers the inner and cover screens, a 60-minute soak, folding mid-ride and a background
     kill.
9. **Performance from those numbers** (L). First-launch quality tiers, a 45 FPS cap, batched
   static dressing, visibility ranges, and the dead SSAO setting removed.

**C. Irresistible at first sight (the "I must play this now!" reaction)**

10. **Height felt (#90)** (L).
    - Layered wind from 0 to 802 m.
    - Exposure: wind swell, breath and heartbeat near edges.
    - A haze layer climbed into and above.
    - Haptics for balance, rides and impacts.
    - A trauma-based camera shake.
11. **Look pass (#93)**, first the yard and the Stack, then every stage (XL).
    - One industrial surface shader: variation, grime under decks, rust streaks, edge wear.
    - Sky reflections back on, with surface roughness re-tuned.
    - A non-colliding finish layer, so detail never changes physics.
    - A hero light.
    - Machines made readable: thick sagging ropes, marked drive masses, RUNNING and BRAKE lamps.
12. **Sound identity** (L). Each machine's motor, rope and brake sounds driven by its own native
    state. Music is **OWNER** (D11).
13. **Onboarding, captions, comfort** (M).
    - In-world cues for the first moves.
    - Captions that print the line actually spoken, uncensored.
    - A voice volume slider.
    - Reduced motion.
    - Danger shown by more than colour.
14. **Shortcuts (#89)** (L). Every bypass the physics allows is probed natively and timed against
    the canonical route; none is blocked with a fake wall. Known bypasses: AS-007's ladders,
    slingshot overdraw, walking the helix flight and the tram track. The signature shortcut is
    slingshot overdraw onto a visible target. The rule 7 ruling is **OWNER**.
15. **World pressure on one band (#91)** (L), after a checkpoint guard so a hazard can't loop a
    restore. Band and form are **OWNER**.
16. **The last two supplied machines** (L each).
    - The rack climber at 750.1 to 778.1 m: its 28 m stroke matches that rise exactly, giving a
      second way to the derrick.
    - The gravity balance at 462.25 to 487.25 m, the only ladder-only rise above 340 m.
    - Both are **OWNER**: site, and whether each gets its own action.

**D. Store**

17. **Compliance** (M plus calendar time).
    - Privacy policy, Data safety ("none collected" is true today), and the IARC rating with
      strong language declared.
    - A 16+ audience, the listing, screenshots and a trailer.
    - Google's closed test. Its tester count and length are unverified; to check against current
      policy.
18. **Clearances** (S, external). Trademark searches for the title and "Kellerworks", the voice
    model's licence and provenance, and the sky noise hash replaced.

## 3. Decisions only the owner can make

| # | Decision | Recommendation |
|---|---|---|
| D1 | Store name and app id | Trademark search first, then an id with no Claude/Gemini mark. A separate release preset keeps `com.cory.scraperx.claude` as the dev identity; never upload it to Play. |
| D3 | Price model | Paid upfront: no billing code. The final id is never published free. |
| D4 | Rating | Accept 16+ with the unfiltered voice, declared truthfully. |
| D5 | What stands at the top | A beacon visible from grade, an arrival beat, stats and credits. |
| D8 | Public repo and the hourly public APK | Make the repo private (or add a proprietary licence), stop `apk-latest`, and move playtests to Play internal testing. I won't change the repo's visibility myself. |
| R9 | All eight vertical machines share one action (stand, press RAISE) | Give each its own control (the route layer, not the machine), or accept the repetition. |
| R7 | Rule 7 against #89 | A physically valid route is never removed with a fake wall. A designed shortcut may skip a climb using a machine's stored energy, but never lets a climb skip a machine. |
| R8 | AS-007's ladders from 220 to 340 m | Name them rule 8's backup for that band; leave them standing. |
| R12 | Start and stop ramps on the supplied machines' motors | Allow it: a drive-profile change, deterministic, not a redesign. |
| D10/D11 | Art fidelity and music | Whichever the budget executes to a high bar; don't ship a paid game without music. |
| D6/D7 | Device floor and renderer | Decide after item 8's numbers. |
