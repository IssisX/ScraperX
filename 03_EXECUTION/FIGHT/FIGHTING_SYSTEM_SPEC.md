# ScraperX Fighting System — Design Spec (DRAFT, read-only investigation)

Base: trunk `ScraperX-Claude @ 66287f33` (§8–§9 base: `e8f92b2e`). Godot 4.7 + native C++17/Jolt at 90 Hz.
Status: SPEC ONLY. No code changed, no branch, no PR, no post. Every number below
is traced to a file unless it appears in §7 (unverified).

> CORRECTION BANNER — READ FIRST: §9 (owner decisions 2026-11-01)
> SUPERSEDES the never-dead assumption in the header and in §1–§3:
> Matron CAN be killed (damage meter + threshold, §9.1; death state,
> §9.3), she must VISIBLY look damaged (§9.2), and the free punch is
> now the 1-2 jab-cross (§9.4). The full corrections list is §9.5.
> Everything else in §1–§8 stands unless §9.5 says otherwise.
> CHECKPOINTS, 11-05 and 11-07 (rule 34; `J044A_SPEC.md`, the checkpoint contract change): a checkpoint
> SAVES her injuries, and a restore or reload brings her back as saved, dead included. Every "undamaged"
> or "`dead_ = false`" restore below is superseded.

Owner rules that bind this design (not re-litigated here):
- ~~"Knock her down, never dead" (owner 2026-10-19). There is NO health/damage
 number on either side, and this spec adds none. Winning buys space, time, and
 interrupts — never a kill.~~ — SUPERSEDED 2026-11-01 by owner decision:
 Matron CAN be killed; see §9 for the damage meter, death state, and the
 full corrections list. (Original text retained for history; do not build
 from it.)
- No velocity writes, no gravity-factor writes, gravity on, one shared
 force/power budget (hers), moving support authoritative, Matron affects
 grip/capture only via physical contact. General skills only: no
 per-enemy/per-ladder hacks. A strike must work through the same general
 balance model `take_blow` already applies to crates, drums, and toolboxes.
- Failing-test-first, 300-line diff cap per slice, evidence package per slice.
- The punch button is always present (owner 2026-10-28). Nothing here removes it.

---

## 1. The fight loop as it stands (audited, not assumed)

### 1.1 The player's one move

Source: `src/sim/physics_world_strike.cpp`, `src/sim/player_strike_limits.hpp`,
`godot/presentation/main.gd` (`&"punch"` → `_native.request_strike()`),
`godot/presentation/main_context.gd` (`punch_button`), `first_person_arms.gd`,
`life/strike_feel.gd`.

- Phases (90 Hz ticks): Windup 11 (~0.12 s) → Reach 6 (~0.07 s) → Recoil 16
 (~0.18 s). Total ~0.37 s per punch. A punch under way owns his hands: no
 climb, vault, hang, pick-up, rig, machine, hook, or jump starts until
 recoil ends (`strike_owns_hands`). If the world takes his hands mid-punch
 (her fingers, a fall, a catch) the punch ends undrawn and lands nothing.
- Offer (when Action/Punch will actually throw): on his feet on support,
 grounded, not in flight/lunge/wall-run, not climbing (`traversal_state_ ==
 None`), not carrying, not rolling, hands free of machines/hook/load, a fist
 available (right else left; lost or broken arm removes that side), and —
 for a free punch — she is within 1.6 m, within 60° of his facing, within
 1.5 m vertically, and the fist's swept path is clear of steel
 (`fist_path_clear`: a 0.06 m sphere swept shoulder→fist; a rail or wall
 takes the punch first). Fist reach 0.75 m, struck from chest + 0.25 m up.
- Landing: a force on her body at the fist, held over 3 contact ticks, with
 the equal force back on him. Limits: 1500 N / 3000 W. Force actually
 applied is `min(1500, 3000 / closing_mps)` (closing floored at 0.5 m/s), so
 at closing ≥ 2 m/s the force falls below 1500 N. Nothing here writes a
 velocity or a position. The landing is read by her balance as a blow
 (`matron.cpp take_blow`) via `struck_by_player` (impulse accumulates in
 `player_blow_ns_` over the contact ticks).
- Maximum punch blow: 1500 N × 3/90 s = 50 N·s → 50 / 220 kg = **0.227 m/s**
 of velocity change on her. This is the single most important number in
 this audit (see §1.3).

### 1.2 Her balance model (the only "damage" in the game)

Source: `Matron::take_blow` (`src/sim/matron.cpp:4006`, called once per tick
from `post_step`), constants in `src/sim/matron.hpp`.

- A blow is judged per strike-window, not per tick: blows accumulate in
 `blow_change_` / `blow_impulse_` until 6 ticks pass with no blow
 (`kBlowQuietTicks = 6`), then the accumulators reset. A punch's 3 contact
 ticks accumulate into one judged blow. Two punches 33 ticks apart are
 judged separately — there is NO combo accumulation in the current model.
- A blow only counts if: she is sensed, in the world, not dead/riding/down/
 emerging, not held by a blow-proof holder; the striker closed on her at
 ≥ 0.5 m/s of its own motion (`kStrikerClosingMps`); and the striker was
 actually in contact with her body that step (passing close by is no blow).
 On the pipe the same read applies in all three directions, and a blow
 ≥ 0.8 m/s (`kPipeTearMps`) tears her hands open (`pipe_struck`).
- Balance math (gravity 9.81, from `matron_hold.cpp:123`'s `9.81F` load
 moment; see §7 for the caveat). `w = sqrt(g / height)`; `takes = w × base`
 (stagger bar); `fells = w × (base + reach)` (knockdown bar):

 | stance | w | takes (stagger) | fells by stage (Pest / Dangerous / Changed) |
 |---|---|---|---|
 | Upright (h 1.05, base 0.08) | 3.06 | **0.245 m/s** | 0.306 / 1.162 / 2.537 |
 | Crouched (h 0.55, base 0.45) | 4.22 | **1.900 m/s** | 1.985 / 3.167 / 5.068 |

 Stage step-reach: Pest 0.02 m, Dangerous 0.30 m, Changed 0.75 m.
 Reach modifiers: ×2 during her lunge burst, ×2 while she is set/moving
 (`set_s_ > 0`), ×0.5 while she holds something, ×2 when she faces the
 blow at Dangerous stage. Reach is ZEROED (so `fells == takes`) during her
 Turn windup, for low blows (striker middle < 0.5 m above her feet), and
 for blows from above (≥ 40 kg falling ≥ 4 m/s onto her).
- Outcomes, in order:
 - `blow < takes`, or already Staggering with `blow < fells`: taken, no
 visible state change. (This is what a punch does today — see §1.3.)
 - `takes ≤ blow < fells`, OR braced: **Staggering**. Her feet catch her
 up (deceleration at her stage's `recover_accel`), her current action
 is cancelled (`clear_interrupted_actions`: journey, routecraft, pipe,
 climb, line, travel, turn — and a Grab whose grip has not closed
 counts as a miss, starts the 1.5 s grab cooldown, and ends the hold).
 Stagger time = 0.4 + 0.8 × clamp((blow − takes)/(fells − takes)) s
 (0.4–1.2 s). `staggers_ += 1`, rage +0.02. She resumes her saved state.
 - `blow ≥ fells` (and not braced): **Down** via `knock_down()`. She lies
 on the steel along the blow direction (or kneels where lying does not
 fit), no hands, no eyes, sliding to a stop on friction, never off an
 edge; if pinned where she would stand, she crawls 0.6 m to room first.
 Down time by stage: Pest 4.0 s, Dangerous 2.5 s, Changed 1.2 s.
 `knockdowns_ += 1`, rage +0.08. A stagger that becomes a knockdown is
 counted once (the stagger count is refunded).
 - **Braced**: a blow she faces (within 50°, `kBraceCos = 0.643`) at
 Changed stage, outside her windup, can NEVER knock her down — it only
 staggers, however large. Low blows and blows from above cannot be
 braced (`blow_faced_` requires neither).
 - **Immunities**: while Down, take_blow early-returns — blows during
 Down are ignored entirely, and the hold-escape tag is cleared. While
 Down the punch offer is also off (her capsule stands over where she
 lies: nothing there to hit). There is NO ground-and-pound in this
 engine, by construction. Emerging, dead, riding, and blow-proof-holder
 states are likewise immune.
- Rage feeds stage (hysteresis, `step_rage`): Pest → Dangerous at rage
 ≥ 0.45, Dangerous → Changed at ≥ 0.85; back down below 0.30 / 0.65.
 Every stagger (+0.02) and knockdown (+0.08) pushes her toward a harder
 stage. Fighting her escalates her: Changed gets up in 1.2 s, has a
 0.75 m step-reach (fells 2.54 upright), and cannot be downed from the
 front. `staggers_`, `knockdowns_`, `blow_mps_`, `rage`, and state are
 already exported in her view (`refresh_view`) and already drive sounds
 (`audio/matron_sound.gd` reacts to stagger/knockdown count edges) — the
 economy has instrumentation, but no on-screen fight read today.

### 1.3 The headline finding: the punch cannot stagger her

Maximum punch blow 0.227 m/s < upright stagger bar 0.245 m/s — by 0.018 m/s.
That gap holds in EVERY stance the audit could construct: crouched bars are
far higher (1.90); lunge/set/faced modifiers only raise `fells`, never
lower `takes`; low/windup zeroing sets `fells == takes == 0.245`, still
above 0.227. A lone punch therefore NEVER staggers or downs her on her
feet, and at 0.227 it cannot tear her off a pipe either (bar 0.8).
Its only real job today is the hold escape (§1.5), which is
size-independent by design. This is why the fight feels dead: the player
punches, the engine absorbs it, and she shows no state change — only the
player's own hit-stop (0.07 s, view-only), camera kick (~1.5° for a full
50 N·s landing), and FOV pinch say anything happened at all.

### 1.4 Her counter kit and when each triggers

- **Grab / hold**: when her hand reaches the player (`caught()`, ankle
 reach, finite grip — a ladder offers no immunity) and `grab_ready()`
 (no grab cooldown), she enters Grab. Miss costs her a 1.5 s cooldown
 (`kGrabCooldownSeconds`); a torn grip costs a regrab cooldown
 (`kRegrabAfterTearS`). While she cannot grab (cooldown), she shoves
 instead (below). In the hold, her pull ramps (finite, capped
 `kRipMaxN`); a braced player (hands on a rail) makes it grow.
- **Shove**: 5.5 m/s along the floor + 2.0 m/s up (`kShoveMps`,
 `kShoveUpMps`), 1.5 s cooldown, 3.0 m edge reach at Dangerous (she
 aims at the shaft rim). Pest shoves away from herself. NOTE (law
 tension, §7): `shove_player` DOES write the player's velocity
 directly (`SetLinearVelocity`) plus a 1.0 s stumble with landing loss.
 Her shove is the one existing velocity write in the fight; this spec
 keeps all NEW player strikes force-based so it adds no new tension.
- **Pipe swat**: on the pipe, her arm pushes a piece along a fixed
 direction from her shared budget; it ends when the piece has the swat's
 target velocity. A player-blow of ≥ 0.8 m/s on the pipe tears her off it.
- **Rip / fatality**: the second catch, when he has one arm left, is the
 fatality: carry to an edge (or the floor version), hang window, then
 the rip. Once the rip has begun, NOTHING frees him — the escape window
 is strictly before the rip begins. After a fatality the run is hers
 (checkpoint restore path, as with other deaths).
- **Rage/stage as counter-pressure**: every stagger/knockdown feeds the
 stage thresholds above. She gets harder the more you hit her, and
 Changed's 1.2 s down-time plus front-brace means frontal brawling stops
 working. Her turn (0.9 s windup, 0.5 s cycle, 0.2 s burst) and her
 lunge remain her own offensive windows.

### 1.5 What the player can and cannot do, by situation

- **On the ground, free**: punch (if offered), move, jump, climb, use
 machines, pick up / throw / haul. Punching owns his hands for ~0.37 s.
- **While climbing** (`traversal_state_ != None`): NO punch. He is
 punchless on a ladder/rung route — her hand can still reach his ankle
 there. This is the single biggest vulnerability window in the fight,
 and any new strike must decide explicitly whether it is climb-legal
 (this spec: no — strikes stay grounded, §3).
- **In flight / lunge / wall-run / rolling / on a machine / on the hook
 / carrying**: NO free punch. (His flight/lunge presses are their own
 moves, not strikes.)
- **While held, before the rip**: punch is offered unconditionally
 (BREAK FREE on the button; in the fatality hang window the button
 shows seconds left) and frees him HOWEVER SMALL the blow
 (`his_blow()`: set down on her deck, never over open air; during Grab
 pre-grip it breaks the grab as a miss and staggers her at minimum
 0.4 s). He has no locomotion of his own while carried — the punch is
 his only input that matters there. After the rip begins, or during the
 reel, the punch frees nothing.
- **While she is Down**: he cannot hit her (offer off, blows ignored).
 The Down window is for running, climbing, repositioning, or grabbing
 an objective — not for more damage. Any design that lets him "finish"
 her contradicts both the engine and the never-dead rule.
- **While she is Staggering**: the punch offer is ON (only Down turns
 it off). A further blow ≥ `fells` during Staggering downs her; below
 that, she stays staggering. This is the only legal "juggle" in the
 engine, and it is a one-step juggle: stagger → down.
- **While she is on the pipe**: blows count in 3D; ≥ 0.8 m/s tears her
 off. The punch (0.227) cannot do it alone.

### 1.6 What the player sees today per landed blow

- Landed punch, no stagger: hit-stop 0.07 s (view-only; the sim keeps
 stepping), camera kick ≤ ~1.5°, FOV pinch 2.2°, windup FOV 0.6°,
 first-person arm animation (drawn back, driven, recoil; kick scaled
 by impulse/50), thrown/landed/missed counters in `strike_feel`. No
 Matron reaction: no flinch, no sound edge (sound edges fire on
 stagger/knockdown count changes only), no HUD element. A landed punch
 that does not stagger is visually indistinguishable from a punch she
 "ignored" — because, in sim terms, she did.
- Stagger / knockdown: she stops / lies down (rig draws her state),
 sound edges fire, `blow_mps` is exported. There is no HUD or prompt
 telling the player what he earned or how long the window is.

---

## 2. The proposed fighting system

Design thesis: **make the existing balance model the game**. No HP, no
new currencies, no death. Every move is a different way to feed
`take_blow`'s judged blow; every reward is an interrupt, a window, or
space. The fun is the loop: land → she reacts → she escalates →
frontal play stops working → flank or go low → down her → use the
window → she gets up harder. A player should want to play FOR this
loop, the way a player plays a boss fight for its pattern.

### 2.1 Moveset (player)

All new strikes follow the punch's lawful pattern: windup → reach →
recoil phases at 90 Hz, force held over contact ticks from per-move
limits (new constants beside `player_strike_limits.hpp`, same shape:
force N, power W, contact ticks), equal force back on him, fist/foot
path swept clear of steel, no velocity/position writes, owns his hands
(and, for the kick, his feet — no jump/climb starts) until recoil ends.
All are grounded-only (no strike from a climb, flight, machine, or
carry), all use the general `take_blow` path — no Matron-specific
constants in strike code. Numbers are proposals computed against §1.2's
verified bars, not measurements; each slice's failing test pins them.

| Move | Input (proposal) | Reach / speed | Contact | Max blow | What it does |
|---|---|---|---|---|---|
| **Punch** (existing, unchanged) | Punch button | 0.75 m fist; 11/6/16 ticks (~0.37 s) | 3 ticks @ 1500 N | 50 N·s → **0.227 m/s** | Hold escape (size-independent, unchanged). On her feet: absorbed, no stagger. Kept exactly so nothing regresses; its job is escape + fastest interrupt attempt. |
| **Jab** (new, fast) | Punch button, tap (same button, distinct timing: thrown when no other strike is queued) | 0.75 m; 7/4/10 ticks (~0.23 s) | 4 ticks @ 1500 N | 66.7 N·s → **0.303 m/s** | The reliable stagger tool vs an UPRIGHT Matron: 0.303 clears takes (0.245) but not Pest fells (0.306) — stagger only, never a knockdown, by 0.003 m/s of design margin. Vs Dangerous/Changed upright: stagger. Vs crouched: nothing (bar 1.90). Fast enough to punish her windup and to break a pre-grip Grab (stagger cancels it). |
| **Low sweep** (new, the anti-brace tech) | Punch while Crouch held | 0.60 m ankle sweep; 16/6/22 ticks (~0.49 s) | 4 ticks @ 1350 N | 60 N·s → **0.273 m/s** | Strikes at ankle height, so `low = true`: reach is zeroed (`fells == takes`) AND the blow cannot be braced. Vs ANY upright Matron, any stage, faced or not: 0.273 ≥ 0.245 → **knockdown**. Vs crouched: nothing. Price: slowest windup, shortest reach, longest recovery — whiffing in front of her is a grab/shove invitation. This is the move that keeps Changed beatable. |
| **Heavy kick** (new, the space-maker) | Punch held ≥ 0.25 s (or Punch while moving toward her — owner to pick, §7) | 1.10 m foot; 18/8/26 ticks (~0.58 s) | 5 ticks @ 3000 N | 166.7 N·s → **0.758 m/s** | Vs upright Pest: knockdown (fells 0.306). Vs upright Dangerous/Changed: stagger (fells 1.162 / 2.537). Vs crouched: nothing. Longest reach and biggest blow, slowest of all, biggest self-pushback (equal force back on him — a missed kick stumbles his own footing). The move for buying the longest window vs Pest and for staggering a Dangerous Matron out of a lunge windup. |
| **Slip** (new, the defensive option) | Double-tap move direction (mapping proposal, §7) | ~0.4 m lateral, ~0.22 s, grounded only, 1.2 s cooldown | — (no strike) | — | A short lateral force burst from his legs (force over ticks, like a jump impulse but sideways — NOT a velocity write). Breaks her facing cone and her grab/shove line; does NOT grant immunity — if her hand is already on his ankle, Slip does nothing (he is held, not free). While slipping he cannot punch, climb, or jump. Use: dodge her shove, exit her facing to set up an unfaced blow (defeats brace), reposition for the sweep's short reach. |

Why this set is fun as its own reason to play: each move has a distinct
job and a distinct failure mode (jab: fast but never downs; sweep:
downs anyone upright but is slow and short; kick: long and hard but
never downs Dangerous+; slip: the only defense, and it earns the flank
that beats brace). The player reads her stage and stance and picks —
Pest: kick her down, take the 4.0 s window; Dangerous: jab/sweep, watch
for the 2.5 s window; Changed: never punch her face (braced), slip to
her side or sweep her legs, take the 1.2 s window and RUN, because she
is back up almost at once. Crouching Matron (lunge / hunting fast):
no strike works — that is the "back off" tell, and the correct play is
Slip out, not a bigger punch.

### 2.2 Visible stagger / knockdown economy (what the player sees and earns)

Presentation-only additions, reusing existing channels (no sim change):
- **Every landed blow reads**: a `blow_mps`-scaled flinch in her rig
 (head/torso recoil proportional to the judged blow, even when it does
 not stagger) plus a grunt/contact sound edge scaled the same way.
 Today a 50 N·s punch reads as nothing; after this, no landed blow
 ever reads as ignored. (Sim effect: none. The flinch must never move
 her body or change a bar.)
- **Stagger read**: she visibly catches her feet (existing Staggering
 pose), a stagger sound edge (exists), and a short on-screen cue with
 the window: "STAGGER — her action broke" plus the remaining stagger
 seconds (0.4–1.2 s, from the exported state). Earned: her current
 action is cancelled; a pre-grip grab breaks; her turn goes on cooldown.
- **Knockdown read**: thud + dust/impact cue at the fall point, "DOWN"
 cue with the down seconds by stage (4.0 / 2.5 / 1.2), and the rule
 made legible: no prompt to hit her while down (offer stays off), so
 the player learns the window is for movement, not damage.
- **Rage / stage read**: her stage is already exported; surface it as a
 fight tell — posture/audio shift at Dangerous, a further shift at
 Changed — so the player can SEE that hitting her escalated her, and
 can predict the shrinking windows (4.0 → 2.5 → 1.2 s) and the brace.
 The economy's lesson must be visible: damage feeds rage feeds danger.
- **Counters**: her shove already stumbles him (1.0 s recovery +
 landing loss); her grab already has a reach/cool-down rhythm. The
 fight HUD shows none of this today; the cues above plus the existing
 BREAK FREE button (with hang-window seconds) are the whole HUD.

### 2.3 Matron fight reactions and counters (keeping her dangerous)

No new Matron moves are proposed. The existing kit already counters
every player move, and the spec relies on that rather than inventing
more:
- Punching in range is grab-risky: her hand reaches his ankle at
 contact distance, and a punch owns his hands for ~0.37 s — he cannot
 climb away mid-punch. The jab's speed is its safety; the sweep's and
 kick's recoveries are their danger.
- Her shove (5.5 m/s + stumble + 1.0 s recovery) punishes whiffs and
 breaks his footing; at Dangerous she aims at the rim. Slip is the
 answer to shove, not a bigger strike.
- Her grab cooldown (1.5 s) is the punish window after a broken grab:
 stagger her pre-grip and she cannot immediately re-grab — the player
 earns ~1.5 s plus the stagger time.
- Her windup (0.9 s) is the counter window: any blow ≥ takes during
 windup downs an upright Matron (reach zeroed) — but the punch's
 0.227 cannot reach it, which is exactly why the jab (0.303) and sweep
 (0.273) exist. Telegraph: her windup state is already in the sim;
 surfacing it (posture/audio tell) is presentation work in slice 5.
- Her stage escalation is the long counter: each stagger +0.02 rage,
 each knockdown +0.08. A player who farms Pest knockdowns will face a
 Changed Matron within ~6 knockdowns (0.08 × 6 ≈ 0.48, crossing 0.45)
 and ~11 within reach of Changed (0.85) — at which point frontal
 play is braced and windows are 1.2 s. The fight gets harder BECAUSE
 the player is winning, which is what makes it replayable rather than
 farmable.
- Never-dead enforcement stays structural, not promised: no HP exists,
 Down is blow-immune, the punch offer is off while Down, and she
 always stands back up (or crawls to room). No slice may add a
 finishing move, a damage number, or a death state for her.

### 2.4 What winning an exchange buys (never death)

- Stagger: her action cancelled, grab broken if pre-grip, turn on
 cooldown, 0.4–1.2 s of her catching her feet. Buy: an interrupt and
 a breath — enough to climb, slip out, or line up a sweep.
- Knockdown: 4.0 / 2.5 / 1.2 s of her on the steel, no hands, no eyes.
 Buy: distance, an escape route, an objective grab, a climb start.
 NOT more damage (immune while down) and NOT a kill (she stands up).
- Flank (unfaced blow vs Changed): the only way to down a Changed
 Matron with a high blow — buy: the knockdown itself, at the price of
 the Slip and positioning that earned the flank.
- Hold escape (existing): the punch frees him pre-rip, set down on her
 deck. Buy: his life, not her defeat.
- What winning NEVER buys: her death, a permanent disable, a
 ground-and-pound, or a lower stage (rage only decays by the existing
 stage-down thresholds over time, not by being hit).

---

## 3. Build slices (dependency order, each ≤ 300 lines, failing test first)

Each slice: (a) a failing test written first that fails on current trunk
and passes after the slice, (b) the change, (c) evidence: the affected
native group(s) + the relevant Godot act run where presentation is
touched. No slice lands without its evidence package. Slices are
sequential: each assumes the previous landed.

- **S1 — Every landed blow reads (presentation only, no balance change).**
 Failing test first: a scripted act run asserting that a landed punch
 (50 N·s, no stagger) produces a rig flinch + a sound edge scaled by
 `blow_mps`. Change: `blow_mps`-driven flinch in her rig + grunt edge
 in `matron_sound`, both presentation-only, zero sim effect. Evidence:
 act run (flinch + sound edges present on a non-staggering landing;
 absent on a whiff) + existing strike/act groups green. This fixes
 "punches feel like nothing" without touching a single bar.
- **S2 — Low sweep (the anti-brace tech).** Failing tests first (native,
 seeded `World` scenarios in the matron balance exam style of
 `tests/sim_tests_matron.cpp:378+`): sweep vs upright Pest →
 `State::Down`; sweep vs upright Changed faced → `Down` (brace
 bypassed); sweep vs crouched → no state change; sweep out of reach →
 whiff, no blow, full recovery. Change: sweep strike (phases, ankle
 strike point, `low = true`, limits 1350 N / 4 contact ticks, 0.60 m
 reach, owns hands till recoil ends) + punch-button mapping (Punch
 while Crouch held) + arm/foot animation + whiff read. Evidence:
 native balance group + act run (sweep lands/downs/whiffs as tested).
- **S3 — Heavy kick (the space-maker).** Failing tests first: kick vs
 upright Pest → `Down`; vs upright Dangerous → `Staggering` only; vs
 upright Changed faced → `Staggering` only (braced); vs crouched →
 nothing; kick whiff → full recovery, self-pushback visible. Change:
 kick strike (1.10 m, 3000 N / 5 contact ticks, longest phases) +
 mapping (hold-to-kick or move-toward-punch, per §7 owner pick) +
 animation + self-pushback read. Evidence: native balance group +
 act run. Depends on S2's strike-plumbing pattern, not its numbers.
- **S4 — Jab (the fast stagger tool).** Failing tests first: jab vs
 upright Pest → `Staggering`, never `Down` (0.303 vs fells 0.306);
 jab vs upright Dangerous/Changed → `Staggering`; jab vs crouched →
 nothing; jab during her Grab pre-grip → grab broken + 1.5 s
 cooldown. Change: jab strike (7/4/10 ticks, 1500 N / 4 contact ticks)
 + tap-vs-hold input disambiguation with S3's kick + animation.
 Evidence: native balance + grab groups + act run. (Ordered after
 S2/S3 so the input mapping is designed once, not three times.)
- **S5 — Slip (the defensive option).** Failing tests first: slip from
 her facing cone → outside grab/shove line, no grab follows within
 the slip window; slip while held → refused (no effect); slip while
 climbing/in flight → refused; slip cooldown 1.2 s enforced (second
 slip inside cooldown refused). Change: lateral force burst over
 ticks (grounded only, force-capped like a jump impulse but sideways;
 explicitly NOT a velocity write), owns feet till recovery ends,
 cooldown state, animation. Evidence: native movement/grab groups +
 act run. Depends on S2–S4 only in that the input map is final by now.
- **S6 — Fight tells: windup telegraph + stage HUD (presentation only).**
 Failing tests first: act runs asserting (a) her Turn windup produces
 a visible/audio tell before the burst, (b) stage changes
 Pest→Dangerous→Changed produce distinct tells, (c) the Down cue
 shows the correct stage seconds. Change: presentation reads of
 already-exported state (turn, stage, rage, down_left_s) — no sim
 change. Evidence: act runs + existing presentation groups green.
 This is last because it displays everything the earlier slices built.

Explicitly OUT of scope for these slices: combos/chains beyond
tap-vs-hold disambiguation (no input buffering system), weapons,
kicks from a climb, strikes while held other than the existing escape
punch, any HP/damage number, any finishing move, any change to her
shove/grab/rip numbers, any change to the punch's existing numbers
(it stays the escape tool), and the parachute (PR #14, separate).

---

## 4. Risks and open design questions (for the main agent / owner)

1. **Input mapping is the scarcest resource.** Punch, sweep, kick, and
 slip need distinct mobile inputs, and the punch button must stay
 always present. The proposals (Punch while Crouch = sweep; hold =
 kick; double-tap move = slip) are guesses — the move input scheme
 was not verified (§7). If the owner hates hold-to-kick, the
 fallback is Punch while moving toward her = kick. This needs an
 owner pick before S3, not after.
2. **The jab's 0.003 m/s margin is a razor.** 0.303 vs Pest fells
 0.306 staggers-but-never-downs by design — but any future tweak to
 punch/kick limits, mass, or step-reach flips it. S4's test must pin
 both sides (stagger yes, down no) so a drift fails loudly.
3. **Sweep-vs-Changed could feel mandatory.** If the sweep is the only
 reliable Changed answer, players will spam it; its 0.49 s cycle and
 0.60 m reach are the intended tax, and her shove/grab punish whiffs.
 If playtesting shows sweep spam, the lever is recovery time, not
 the low-blow rule (which is load-bearing for the whole design).
4. **Rage farming.** Knockdowns feed rage; a player who downs Pest
 repeatedly faces Changed quickly. That is intended (escalation),
 but the rage decay rate (stage-down thresholds) was not tuned here
 — if Changed arrives too fast, tune decay, not the fight rewards.
5. **Shove velocity-write precedent.** Her shove writes velocity
 today. If a future slice ever wants player knockback, that needs an
 explicit owner carve-out; this spec avoids the question by staying
 force-based everywhere new.

---

## 5. Files touched (for the main agent's planning; no edits made here)

- `src/sim/player_strike_limits.hpp` (per-move limits), a new
 `src/sim/physics_world_strike2.cpp` or extension of
 `physics_world_strike.cpp` (sweep/kick/jab/slip state machines),
 `src/sim/matron.hpp` (only if a new strike needs a view field —
 avoid), `godot/presentation/main.gd` (input routing),
 `main_context.gd` (button labels: PUNCH / SWEEP / KICK / SLIP states),
 `first_person_arms.gd` (strike animations), `life/strike_feel.gd`
 (hit-stop/kick per move), `life/matron/*.gd` (flinch, tells),
 `audio/matron_sound.gd` (grunt/stagger/down edges).
 Deliberately untouched: `take_blow`'s bars (no balance-constant
 changes), her shove/grab/rip numbers, the punch's numbers, the
 never-dead rule, manifests, governing-laws doc.

## 6. Evidence standard per slice (reminder for builders)

Spec + failing test first; 300-line diff cap; one worktree/branch per
slice off current trunk; native group runs on the slice head; Godot act
runs for presentation slices (Godot-runtime MCP, actually watch it);
rule_check 0 BLOCK / 0 REVIEW; branch CI green (all proof jobs, only
publish-checkpoint failing off-mainline) before main lands. No slice
lands on prose-only "tests pass".

---

## 7. What I could NOT verify (honest gaps — do not treat as fact)

1. **Gravity constant**: the threshold arithmetic uses g = 9.81, sourced
 from `matron_hold.cpp:123` (`player_mass_kg * 9.81F` in a load
 moment). I did NOT find the PhysicsSystem gravity-init line, so if
 the sim's gravity differs, every `takes`/`fells` number in §1.2
 shifts by `sqrt(g/9.81)`. The formulas and constants are verified;
 the gravity value itself is not traced to init.
2. **Movement suppression while held**: I verified he is carried
 (`carry_pose` in `step_hold`) and that the punch is his only
 effective input there, but I did NOT trace exactly which movement
 commands are dropped in `step_hold` vs merely ineffective.
3. **The "brace" Action during her reach**: `strike_step` comments
 that "during her reach, before her fingers close, the brace has
 Action" — I did NOT trace what the context Action button does in
 that window or what it costs.
4. **Grip-tear exact trigger**: `arm_tears_` / `hold_tears_at_` exist
 in hold code and the fatality takes his second arm, but I did NOT
 fully trace the first tear's trigger conditions or its timing
 relative to the rip.
5. **Mobile move input scheme**: I verified the punch button and the
 Action context button, but NOT whether movement is a joystick,
 buttons, or both — so the Slip mapping (double-tap move) and the
 kick mapping (hold vs move-toward) are proposals, not fits to a
 verified scheme.
6. **Rig poses for Staggering/Down**: I verified the states and their
 sim behavior, but did NOT read `matron_rig.gd`'s per-state poses —
 so "she visibly catches her feet / lies down" is the state's
 documented meaning, not a confirmed animation.
7. **Per-tick `take_blow` timing**: confirmed it runs in `post_step`
 once per step; I did NOT verify the step order relative to the
 strike's contact ticks beyond the code's own comments.
8. **Shove vs the no-velocity-writes law**: `shove_player` DOES call
 `SetLinearVelocity` on the player. I report this as an existing
 tension, not as permission: whether the law has a carve-out for
 her kit, or the shove predates the law, is an owner question.
 This spec adds no new velocity writes either way.
9. **Existing balance-exam tests**: I cited
 `tests/sim_tests_matron.cpp:378+` (toolbox/drum stagger tests) as
 the pattern for slice tests but did NOT run any test — this was a
 read-only investigation, so no number in §1.2 was confirmed by
 execution, only by reading the code that computes it.

---

## 8. The experience layer (Cory's bar: mind-blowing, nothing skimped)

Base note: this section was written against trunk `ScraperX-Claude @ e8f92b2e`
(FreeDrop #68/#69 landed after §1–§7's base of 66287f33; that merge touched
traversal files, the descent test, and CTest registration only, so the
strike/balance audit in §1 stands unchanged). Every channel below was read
in code on this base unless it appears in §8.7. Design rule for the whole
section: **every effect scales with the judged blow** (`blow_mps`, the
exported judged-blow velocity change) or with the move's force-time —
never a fixed cosmetic. A jab that barely staggers must never feel like a
kick that downs her, and a whiff must never feel like a landing.

### 8.1 Haptics — the API exists, the fight does not use it yet

Verified path (no invention): `godot/presentation/main_feel.gd` owns
haptics. `HAPTICS` maps a kind to `[base_duration_ms, base_amplitude]`;
`_haptic(kind, strength)` sends `duration = base × lerp(0.6, 1.0, strength)`
and `amplitude = clamp(base × strength, 0.05, 1.0)`. On the phone
(`InputRouter.Device.TOUCH` + `OS.has_feature("mobile")`) that is
`Input.vibrate_handheld(duration_ms, amplitude)` — the Fold 6 path.
On gamepad it is `Input.start_joy_vibration`, with a hardcoded heavy list
(`land`, `death`, `warn`) getting full low-motor amplitude. Pulses closer
than `HAPTIC_GAP_MS = 50` are coalesced (dropped) unless the kind is in
`HAPTIC_PRIORITY`. In CI mode (`_ci_mode`) nothing vibrates; pulses are
only counted in `haptic_counts` — and `godot/tests/matron_hold_haptics_test.gd`
is the passing test that asserts exact count dicts per scripted state
sequence. The whole fight-haptics spec below is testable the same way:
scripted act runs assert `haptic_counts`, with `_settings.vibration =
false` asserting zero counts. Existing scale for calibration: `press`
12 ms / 0.30 (lightest), `matron_grab` 45 / 0.80, `matron_tear` 80 / 1.0,
`death` 150 / 1.0 (heaviest), `slip` 70 / 0.85, `stumble` 40 / 0.70.

New kinds to add to `HAPTICS` (presentation constants, not sim numbers),
following existing naming. Each strike sends TWO pulses: the strike's own
contact pulse on landing, then the outcome pulse. Outcome kinds join
`HAPTIC_PRIORITY` so a pair inside the 50 ms window still reads as two
beats, not one smear.

| Pulse | Kind (new) | [base ms, base amp] | strength formula (input) |
|---|---|---|---|
| Punch contact | `strike_punch` | [14, 0.40] | `clamp(blow_mps / 0.227, 0.3, 1.0)` |
| Jab contact | `strike_jab` | [16, 0.45] | `clamp(blow_mps / 0.303, 0.3, 1.0)` |
| Sweep contact | `strike_sweep` | [30, 0.60] | `clamp(blow_mps / 0.273, 0.3, 1.0)` |
| Kick contact | `strike_kick` | [55, 0.80] | `clamp(blow_mps / 0.758, 0.3, 1.0)` |
| Whiff (any move) | `strike_whiff` | [12, 0.25] | fixed 0.5 (soft air, always felt, never confused with a land) |
| Landed, no stagger | `strike_land` | [24, 0.50] | `clamp(blow_mps / 0.303, 0.4, 1.0)` |
| Stagger (priority) | `strike_stagger` | [70, 0.90] | fixed 1.0 |
| Knockdown (priority) | `strike_down` | [120, 1.00] | fixed 1.0 (below `death` 150 ms, above everything else) |

Reused existing kinds, no new ones needed: shoved by her → `stumble`
(40 / 0.70, strength from his landing loss as today); grabbed →
`matron_grab` (45 / 0.80); torn → `matron_tear`; Slip's own pulse →
existing `slip` (70 / 0.85). For gamepad parity, `strike_stagger` and
`strike_down` join the hardcoded heavy list alongside `land`/`death`/
`warn` (a one-line `main_feel.gd` change in S1). All fight haptics
obey the existing `_settings.vibration` toggle (`settings_store.gd`)
and nothing else — they are NOT gated on reduced motion, matching
existing haptic behavior (see §8.7).

Why two pulses: the contact pulse tells him WHICH move connected (jab
short, kick long), the outcome pulse tells him WHAT IT EARNED. A
punch that lands but does not stagger reads as tap + soft thud; a
sweep that downs her reads as medium sweep + long heavy double-beat.
No landed blow ever reads as nothing, and no whiff ever reads as a hit.

### 8.2 Animation detail per move (90 Hz ticks, viewer only)

The pattern to follow is verified in `first_person_arms.gd::_strike_target`:
key wrist targets relative to the camera — guard (right 0.17 × side,
up −0.19, back −0.28), cocked (right 0.24 × side, up −0.24, back +0.02),
out (right 0.05 × side, up −0.08, back −0.62 view reach) — with
windup as a smoothstep `t²(3−2t)` over windup ticks, reach as an
ease-out cubic, and recoil as a spring `1 − exp(−6t)·cos(9t)·(1 + 0.25·kick)`
where `kick = clamp(impulse / 50, 0, 1.5)`. The fist closes hard on
reach; on landing the out-target becomes the hit point + normal × 0.05
(from the strike dict's `hit_point` / `hit_normal`). Every new move
reuses this function shape with its own phase ticks (from §2.1) and
pose deltas; nothing here moves a sim body.

| Move | Windup (ticks) | Contact (ticks) | Recoil (ticks) | Key poses (first-person) |
|---|---|---|---|---|
| Punch (existing) | 11 | 6 (reach) + 3 contact | 16 | Guard → cocked (elbow out, fist drawn back) → snap out to full reach / hit point → spring back to guard, overshoot scaled by `kick`. |
| Jab | 7 | 4 + 4 contact | 10 | Same pattern, shorter draw-back (cocked right 0.20, not 0.24), snappier reach, minimal recoil overshoot. Reads as FAST, not hard. |
| Sweep | 16 | 6 + 4 contact | 22 | Camera dips toward the sweep side and lowers ~0.15 m; the striking arm sweeps LOW across the bottom of frame (the visible proxy — see §8.7, no first-person foot mesh exists). Longest windup telegraphs the move; longest recoil punishes a whiff. |
| Kick | 18 | 8 + 5 contact | 26 | Camera thrusts forward and pitches down slightly at contact, then springs back with the largest overshoot (`kick` up to 1.5 from 166.7 N·s). No drawn foot exists (see §8.7) — the kick reads through camera thrust + her reaction, not a leg mesh. |
| Slip | — (~0.22 s) | — | — | Camera shifts laterally ~0.4 m to the slip side with a slight roll, then settles; no arm pose (hands stay free — Slip owns feet, not hands, per §2.1). The existing `slip` haptic fires with it. |

Matron's reaction animation (in `life/matron_view.gd::_pose`, driven
from `matron_view` pivots; sim states already exist, this is pose only):
- **Flinch (NEW, S1, presentation-only):** on any landed blow, her
 head/torso take an additive recoil along the blow direction, amplitude
 proportional to `blow_mps` (capped at the knockdown-scale blow), decaying
 over ~0.3 s. It must never move her body, change a bar, or feed a
 state — it is a rig offset, applied in `_pose` and gone in a third of
 a second. This is what makes a 50 N·s punch visible at last.
- **Stagger (exists, verify in act runs):** `STATE_STAGGERING` pose —
 trunk taken the way the blow went, arms catching, `hip_y = 0.95`
 (from `_pose`). The stagger read is this pose + the §8.4 cue + the
 §8.1 stagger pulse, all firing on the `staggers` count edge.
- **Knockdown (exists, verify in act runs):** `STATE_DOWN` pose — she
 lies along `fall_x` / `fall_z` (both exported), or kneels (`kneeling`
 exported, `hip_y = 0.42`) where lying does not fit. Dust bursts at
 the fall point (§8.3) on the `knockdowns` count edge.
- **Recovery rise (NEW cue, S6, presentation-only):** during the last
 ~0.5 s of the down window (`down_left_s` exported, verified), her
 pose eases toward standing via the existing `_ease_pose` path — she
 visibly stirs before she stands, so the window's end is never a
 surprise. Same treatment at the end of Staggering once
 `stagger_left_s` exists (§8.5).

### 8.3 Effects detail (all view-only, all blow-scaled)

Base system verified in `life/strike_feel.gd`: hit-stop `HIT_STOP_S =
0.07` s scaled by `clamp(blow / 50, 0.85, 1.15)` where `blow` is the
strike's force-time in N·s (frozen: drawn arms, camera kick, and her
drawn body hold still; the sim keeps stepping — never a time scale);
camera kick `0.03°/N·s` capped at `KICK_MAX_DEG = 3.0°`, decaying at
14/s with a 26 rad/s shudder; FOV pinch `2.2°` decaying at 10/s; windup
FOV `0.6°`. The existing caps flatten every new move to the punch's
ceiling (jab 66.7, sweep 60, kick 166.7 N·s all clamp to 1.15× hit-stop
and 3.0° kick) — so S1–S3 extend `strike_feel.gd` with PER-MOVE caps.
These are view-only constant changes, not sim changes:

| Move | Force-time | Hit-stop (proposed) | Camera kick (proposed) | FOV pinch |
|---|---|---|---|---|
| Punch | 50 N·s | 0.07 s (unchanged) | ≤ 1.5° (unchanged) | 2.2° (unchanged) |
| Jab | 66.7 N·s | 0.08 s | ≤ 2.0° | 2.2° |
| Sweep | 60 N·s | 0.08 s | ≤ 2.0° | 2.2° |
| Kick | 166.7 N·s | 0.11 s | ≤ 5.0° | 3.0° |
| Whiff (any) | 0 | none (no freeze on air) | none | windup FOV only, then release |

Windup FOV scales with the move's commitment: punch 0.6° (existing),
jab 0.6°, sweep 0.9°, kick 1.2° — the eyes narrow further the longer
the windup, so a kick's 18-tick windup reads before it lands.
Reduced-motion handling follows existing behavior exactly: kick and FOV
pinch zero out under `reduced_motion`, hit-stop is kept (the existing
code keeps it — see §8.7).

Particles follow the verified pool pattern in
`life/gore/blood_fx.gd` (pooled `GPUParticles3D` emitters, `amount` /
`lifetime`, `emitting` fired on burst, never allocated per hit):
- **Flesh impact** (any landed blow on her body): burst at
 `hit_point` along `hit_normal`, amount scaled by `blow_mps`
 (proposal: 12 + 40 × clamp(blow_mps / 0.758, 0, 1) particles,
 0.3 s lifetime). A punch flickers; a kick erupts.
- **Steel sparks** (a strike stopped by steel via `fist_path_clear`):
 burst at the steel contact point, short bright spray (proposal:
 24 particles, 0.4 s). This is the ONLY spark source — whiffs in open
 air spark nothing.
- **Knockdown dust** (on the `knockdowns` count edge): burst at her
 fall point, amount scaled by `blow_mps` (proposal: 20 + 60 ×
 clamp(blow_mps / 0.758, 0, 1), 0.8 s lifetime), settling outward
 along `fall_x` / `fall_z`. The dust is the knockdown's signature:
 it marks WHERE she fell, so the player reads the space he just won.
Particle counts/materials are proposals to tune in act runs (§8.7) —
the pool pattern and the firing points are verified, the numbers are not.

### 8.4 UI detail (Fold-usable, signal-dense, in `ui/hud.gd`)

Owner: `godot/presentation/ui/hud.gd` (`update_hud`, `_draw_prompts`,
`_draw_toasts`, `_draw_her_marker`, reticle, gauge — all drawn with
`UiStyle.text` and `u`-scaling, which is what keeps them Fold-usable).
New element: `_draw_hit_marker`, driven by `strike_feel`'s edges
(`thrown` / `landed` / `missed` counters, verified) plus the matron
count edges (`staggers`, `knockdowns`, verified exports). Center-screen,
never in the bottom button cluster, sized in `u` units:

| Marker | Trigger | Draw | Hold |
|---|---|---|---|
| Whiff | `missed` edge, no state change | Small hollow grey X | 0.25 s fade |
| Landed | `landed` edge, no count change | Small white X | 0.25 s fade |
| Stagger | `staggers` count edge | Larger yellow X + `STAGGER` + remaining seconds | 0.6 s, then fade (countdown continues in the cue below while she staggers) |
| Knockdown | `knockdowns` count edge | Large orange X + `DOWN` + remaining seconds | 1.0 s, then fade (countdown continues below while she is down) |

Countdown cues (text beside the marker, same draw pass):
- Down: `DOWN <down_left_s>s` — reads the verified `down_left_s`
 export (0 while pinned/crawling: show `DOWN — PINNED` instead of a
 number when `down_left_s == 0` and state is still Down).
- Stagger: `STAGGER <s>s` — reads `stagger_left_s`, which does NOT
 exist yet (see §8.5: S6 adds it mirroring `down_left_s`).
Stage tells: her stage is NOT exported as a name; `rage` IS exported.
The HUD therefore shows a rage heat bar (fill = `rage`, with the
Dangerous/Changed thresholds marked once confirmed in code — §8.7)
plus the posture/audio shifts that already exist per stage, rather
than a hardcoded stage label. The windup telegraph (S6) reads the
verified `turn_s` view field (seconds into the Turn): while `turn_s`
is in windup, the HUD shows her windup tell (lean marker + audio cue
— audio per §8.6) before the burst.
The rule made legible: while she is Down, the punch button
(`main_context.gd::punch_button`, which already supports a greyed
`BREAK FREE` state) shows greyed with label `SHE'S DOWN — MOVE`.
The button stays present (owner rule 2026-10-28 — never removed), but
a press does nothing, matching the sim (offer off, blows ignored while
Down). The player learns in one fight that the Down window is for
movement, not damage — no tutorial text needed.

### 8.5 Causal chain, end to end (builder checklist)

Each row: the step, what implements it, and the EXACT exported value
the next step reads. No new sim numbers are invented for presentation;
the single required addition is marked.

1. **Input** — punch button (`main_context.gd::punch_button`, labels
 PUNCH / BREAK FREE / SHE'S DOWN — MOVE) → `_native.request_strike()`.
 New moves reuse the button with the §2.1 mappings (tap = jab, hold
 = kick, Punch + Crouch = sweep, double-tap move = slip). The strike
 dict the sim returns each frame carries: `strikes`, `hits`,
 `phase`, `phase_ticks`, `arm`, `landed`, `hit_point`,
 `hit_normal`, `impulse` (all verified in `strike_feel.gd` /
 `first_person_arms.gd`). ⚠️ The current dict has NO move field —
 new strikes must add one (`move`: punch/jab/sweep/kick/slip) so
 feel code can tell moves apart (§8.7).
2. **Strike phases** — `physics_world_strike.cpp` state machines
 (windup → reach → recoil per §8.2 ticks), force held over contact
 ticks from per-move limits, equal force back on him, fist/foot
 path swept clear of steel. Whiff = reach ends with no `hits` edge →
 §8.1 whiff pulse + §8.4 whiff marker, full recovery.
3. **Contact / impulse** — force-time accumulates into `player_blow_ns_`
 (punch pattern) → the judged blow in `take_blow` once the window
 quiets (6 ticks). Exported: `blow_mps` (the judged blow in m/s).
 EVERY effect in §8.1–§8.4 scales from `blow_mps` or from the
 strike's force-time (`impulse` in the strike dict).
4. **State change** — `take_blow` judges against `takes` / `fells`
 (§1.2): below bar → taken, no state change (but flinch + land
 pulse + land marker still fire — S1); stagger bar → `State::
 Staggering`, `staggers += 1`, rage +0.02, her action cancelled,
 pre-grip grab broken + 1.5 s cooldown; fell bar → `State::Down`
 via `knock_down()`, `knockdowns += 1`, rage +0.08, fall along the
 blow (`fall_x` / `fall_z` exported, `kneeling` exported).
 Exported: `state`, `staggers`, `knockdowns`, `blow_mps`, `rage`,
 `down_s`, `down_left_s`, `fall_x`, `fall_z`, `kneeling`
 (all verified in `refresh_view`).
5. **Her counter** — from the new state, her existing kit answers:
 stagger → turn cooldown, grab cooldown if broken; down → she lies
 for `down_s` (4.0 / 2.5 / 1.2 by stage) then stands via
 `_ease_pose`; rage crossing 0.45 / 0.85 → harder stage (see §8.7
 on threshold confirmation); shove (5.5 m/s + stumble) and grab
 punish whiffs and recoveries. No new counter is added.
6. **World consequence** — the player spends the window: the Down
 countdown (§8.4) ticks on `down_left_s`; the punch button greys;
 dust marks the fall point; on standing, the recovery-rise cue
 (§8.2) warns him. Then the loop restarts one stage harder.
REQUIRED ADDITION (S6, one view field, same pattern as `down_left_s`):
`stagger_left_s` — seconds left in Staggering — so the stagger
countdown in §8.4 reads a real number instead of a nominal timer.
Everything else in this section reads fields that exist today.

### 8.6 Sound — SPEC ONLY (Claude's lane, do not reassign)

Cues specified here, implemented by Claude when he returns (Mon Nov 2
~11am CST). No sound work is assigned to any other agent in any slice.
Follow the verified edge pattern in `audio/matron_sound.gd` (count
edges on `staggers` / `knockdowns` firing `matron_hit` /
`hit_heavy` cues at her head/center):
- Whiff: short air-cut cue on the `missed` edge, quiet, distinct from
 any impact.
- Impact, per move: flesh-contact cue scaled by `blow_mps` (jab crisp,
 sweep low thud, kick heavy boom) on the `landed` edge — including
 non-staggering landings (S1's grunt edge: today only stagger/
 knockdown edges make sound, which is why a landed punch sounds like
 nothing).
- Stagger: existing pattern (feet stamping/slewing + `matron_hit`),
 kept, with intensity scaled by `blow_mps`.
- Knockdown: existing pattern (`hit_heavy` at center + `matron_hit`
 at head), kept, with the fall-point dust (§8.3) as its visual twin.
- Shoved / grabbed / torn / hang: existing `matron_grab` /
 `matron_tear` / `matron_hang` cue pattern, unchanged.
- Slip: soft footwork/air cue, quiet by design (defense should not
 shout).
- Windup telegraph (S6): rising tension cue while `turn_s` is in
 windup, resolving on the burst or on stagger-interrupt.

### 8.7 Honest gaps (everything not verified — do not treat as fact)

1. **Punch haptics today**: no strike-land `_haptic` call was found
 in the `main_feel.gd` sections audited (calls found: press, land,
 stumble, roll, teeter, balance, slip, reach, strain, grab, vault,
 mantle, break, hold events). S1 must confirm whether the punch
 vibrates at all today before adding the §8.1 kinds.
2. **No first-person foot mesh**: `first_person_arms.gd` draws arms,
 hands, and the remote — no foot or leg mesh was found. Kick and
 sweep first-person animation is therefore specified camera-led
 (§8.2); adding a drawn foot is an asset decision for the owner,
 not assumed here.
3. **Particle counts/materials**: the `blood_fx.gd` pool pattern
 (pooled `GPUParticles3D`, amount/lifetime, burst firing) is
 verified, but no strike-specific spark/dust counts or materials
 were read — §8.3's numbers are tuning proposals for act runs.
4. **`stagger_left_s` does not exist**: `down_left_s` is verified;
 no stagger-remaining export was found. §8.4's stagger countdown
 depends on S6 adding it (mirroring `down_left_s`).
5. **Rage thresholds**: the 0.45 / 0.85 stage thresholds and the
 0.30 / 0.65 stage-down thresholds come from the §1 audit, not
 from a re-read of `step_rage` in this pass. The §8.4 rage bar
 must confirm them in code before marking threshold ticks.
6. **HUD button-cluster positions**: the punch/Action cluster's exact
 screen position was not verified, so §8.4's "never in the bottom
 button cluster" is a placement rule for S6 act runs to check, not
 a confirmed layout.
7. **Strike dict move identity**: the verified strike dict fields
 (`strikes`, `hits`, `phase`, `phase_ticks`, `arm`, `landed`,
 `hit_point`, `hit_normal`, `impulse`) include no move field.
 §8.5's per-move feel requires adding one; the slice that adds a
 move must add the field with it.
8. **Matron Down lying pose detail**: `STATE_DOWN` + `kneeling`
 (`hip_y = 0.42`) and `STATE_STAGGERING` (trunk along blow,
 `hip_y = 0.95`) are verified in `_pose`; the lying (non-kneeling)
 Down pose's exact hip/pitch values were not quoted here and must
 be confirmed in S1/S2 act runs.
9. **Hit-stop under reduced motion**: existing `strike_feel.gd`
 zeroes kick/FOV under `reduced_motion` but KEEPS hit-stop. §8.3
 follows that behavior; if the owner wants hit-stop off too, that
 is a separate decision, not smuggled into a fight slice.
10. **Carried caveats from §7**: gravity constant (9.81 unverified
 at init — scales every `takes`/`fells` number and hence every
 `blow_mps` scaling reference in §8.1/§8.3); new-move phase ticks
 are §2.1 proposals, not measurements; gamepad heavy-list
 extension is a required `main_feel.gd` edit in S1.

---

## 9. Owner decisions 2026-11-01: she CAN be killed, she must LOOK damaged, real two-handed boxing

This section OVERRIDES the spec's old assumptions where it says so. Base:
trunk `ScraperX-Claude @ e8f92b2e` (FreeDrop landed; strike/balance code
untouched, so the §1 audit stands). Three owner decisions, all 2026-11-01:

1. **Matron CAN be killed.** This SUPERSEDES the 2026-10-19 "knock her
 down, never dead" rule that §1–§3 were built on. She needs to be hit
 A LOT, but she can be killed.
2. **She must VISIBLY look damaged** to signal how far along she is.
 Damage progression reads on HER body/pose/movement first; a HUD
 element is secondary confirmation only, never the primary read.
3. **Real two-handed boxing, looks matter most.** A genuine 1-2
 (jab-cross, alternating fists, boxer's weight transfer, shoulder
 rotation, guard return). The current punch is one fist only; the 1-2
 must look like a boxer. "Two-handed" means boxing WITH both hands
 in alternation, one fist at a time — it NEVER means both hands
 striking together as one punch. A two-hands-one-punch is not a
 boxing move and is explicitly out of the design.

### 9.1 Damage accumulation model (primary: persistent judged-blow impulse)

**Primary meter: `damage_ns`, a persistent total of judged-blow impulse
in N·s from ALL valid physical sources accepted by the real `take_blow`
path — player strikes, crates, drums, falling steel, machinery,
dropped objects, anything that judges. It is accumulated in a new sim
field that is live-only: it lasts while the run goes on and does not
persist through checkpoints (rule 34; checkpoint rule below). Player-only attribution is tracked separately as
`player_damage_ns` (see below); it never replaces or narrows the
real meter.**

Why impulse, not stagger/knockdown counts: `take_blow`'s own
accumulators (`blow_change_` / `blow_impulse_`, `src/sim/matron.hpp`)
reset after 6 ticks with no blow — they judge ONE blow, they cannot
remember a fight. Counts would be worse: they would let a hundred
absorbed punches (0.227 m/s, never staggering) kill her without her
ever reacting, which reads as nothing happening and then a sudden
death. Impulse totals reuse the game's existing unit of "how hard she
was hit" (the judged blow, already exported as `blow_mps`): every
judged blow adds `blow_mps × 220 kg` N·s to `damage_ns` at
the moment its window is judged, whatever its source. Absorbed blows
count too — a punch that does not stagger still bruises (50 N·s) —
but heavy blows count more, so the jab/sweep/kick economy from §2.1
matters for the kill, not just for the stagger. Environmental blows
feed the SAME meter at their real impulse: the physical world is a
legitimate way to hurt and ultimately kill her — a crate, drum, or
falling beam that judges is real damage, not a stagger-only side
effect. Source attribution is separate, not a filter:
`player_damage_ns` tracks the player-attributed share via the
existing `player_blow_ns_` path (§1.1), so the general `take_blow`
code gains no Matron-specific damage constant — the meter is fed by
the same judged blow every striker feeds, and attribution rides
alongside it. Mixed-source rule (tested in S7): if a player strike
and an environmental hit land inside the same judged-blow window,
each source is attributed only its real respective contribution —
never double-count the combined result, never assign the whole
window to the player.

**Kill threshold: `damage_ns ≥ 6000 N·s` (owner-set; it stands as
set. 4000–8000 is a tuning range only, not permission to retune;
pinned by the kill slice's failing tests).**
In landed blows, at 6000:

| Strike | Per land (N·s) | Lands to kill alone |
|---|---|---|
| Punch (escape-era free punch) | 50 | 120 |
| Jab | 66.7 | 90 |
| Sweep | 60 | 100 |
| Kick | 166.7 | 36 |
| Full 1-2 (jab + cross, §9.4) | 150 | 40 combinations |

A realistic mixed fight lands ~50–70 strikes. That is "a lot" without
being a grind: it spans several encounters across a run, not one
exchange, and it rewards the heavy moves (a kick is worth 3.3 punches).
Environmental impulse counts toward the same 6000 alongside player
strikes — the world can contribute to, and finish, a kill.

**Rage vs damage are separate tracks — this is the design's core
tension.** Rage (§1.2) is short-term heat: +0.02 per stagger, +0.08
per knockdown, decayed by quiet, and it drives her stage (Pest →
Dangerous → Changed) and her windows. Damage is permanent wear: it
never decays, never resets on stagger/knockdown/recovery, and it is
the only thing that kills her. Consequence, stated plainly: as damage
climbs, rage usually climbs with it (the same blows feed both), so
she gets HARDER as she gets closer to death — the endgame is a race
to finish a Changed Matron before she finishes you. The mastery loop
this creates is deliberate: burst her, retreat while rage cools, come
back — damage stays, rage does not.

**Down windows and accumulation, exactly.** While she is Down,
`take_blow` early-returns and the punch offer is off (§1.5) — no
judged blow exists, so NOTHING accumulates during Down. `damage_ns`
persists unchanged through every Down window; the window is for
movement and for READING her damage stage (§9.2), not for damage.
Same rule for Emerging, riding, and blow-proof-holder states: no
judged blow, no accumulation. During Staggering the offer is ON, so
blows land, judge, accumulate — and a blow ≥ `fells` during
Staggering that downs her counts in full, including the downing blow.

**Checkpoint damage semantics (authoritative, rule 34).** Damage is
live-only and does not persist through checkpoints: a checkpoint
commit does not store `damage_ns` or `player_damage_ns`, and a
checkpoint rewind or a reload brings her back undamaged and alive
(both meters at 0). Damage persists through ordinary combat, stagger,
and knockdown within the live run only. S7 tests pin that a restore
or reload returns both meters to 0.

### 9.2 Visible damage stages (body first, HUD second)

Five stages on the 6000 N·s meter. The body carries the read; the HUD
(§8.4) shows only a small stage pip as confirmation. Stage is a
presentation read of an exported `damage_stage` field (0–4) plus
`damage_ns` for the pip; the stage thresholds are presentation
constants derived from the same meter, not second sim numbers.

| Stage | Meter | Body / pose / movement tells (rig, §8.2 `_pose` path) | Paired sound + haptic |
|---|---|---|---|
| 0 Fresh | 0–1199 | As today. Guard up, upright hunting posture, full stride. | As today. |
| 1 Bloodied | 1200–2699 | Trunk lean forward ~5° (additive pitch in `_pose`); first blood-burst residue on her torso (blood pool intensity up, §8.3 pattern); breathing cue quickens. | Stage-change edge: low grunt + new haptic kind `damage_stage` [60, 0.70], priority (one beat, not a fight pulse). |
| 2 Battered | 2700–4199 | Lean ~12°, hip lower (~0.05 m), guard arms drop halfway (elbows out, fists below chin line); recovery visibly slower — rise ease stretches (see recovery scaling below); blood bursts larger/longer. | Same stage-change pair, heavier breathing bed under it. |
| 3 Broken | 4200–5399 | Lean ~20°, pronounced slump; guard DOWN (arms hang at sides between actions — she no longer presents a guard); gait limps (asymmetric step: short step on one side, foot drag on the other, pose-level on her walk cycle); one eye swells shut in the rig (lid pose, if the rig exposes it — §9.7). | Same pair + a wet cough/breath cue on stage entry (Claude lane, spec'd). |
| 4 Dying | 5400–5999 | Barely standing: lean ~28°, knees bent, arms hanging, steps shuffle and stumble (occasional catch-feet micro-staggers with NO sim state change — pure pose noise); blood heavy; she tracks him slower (head turn lags — pose only, never a sim turn-rate change). | Same pair + heartbeat-bed cue under the fight (Claude lane, spec'd). |

**≥6000 N·s is NOT a stage.** There is no stage 5: at 6000 the
meter ends and she dies (S8). Stage boundaries are 1200 / 2700 /
4200 / 5400 only.

Recovery scaling (sim, part of the kill slice, pinned by tests):
stagger catch-up time and Down time scale by `×(1 + 0.15 × stage)`
(stage 0–4, so up to ×1.6 at Dying). This is the one place damage
changes a sim duration, and it is deliberate: a dying Matron is
slower to get up, which is both the tell and the reward. Her
OFFENSIVE numbers do NOT scale with damage — shove, grab, and rip
keep their existing force/cooldown values at every stage. Her danger
at high damage comes from rage stage (Changed brace, 1.2 s windows),
not from a secretly weakened kit, and her vulnerability comes from
recovery scaling. Both tracks stay independently testable.

What the stages must never do: move her body, change a `takes`/`fells`
bar, or feed a state. Stage tells are pose/particle/sound/haptic
only, except the recovery scaling above, which is the single declared
sim effect. (The flinch rule from §8.2 applies unchanged: a landed
blow always shows, at every stage.)

### 9.3 Death state (what happens when the meter fills)

When `damage_ns ≥ 6000` on a judged blow: her current action is
cancelled, any hold ends via the existing `end_hold(Dead)` pattern
(`matron.cpp:4243`: `end_hold(dead_ ? State::Dead : State::Searching)`),
and she transitions to the ALREADY EXISTING `State::Dead` (sim enum
`matron.hpp:135`, view `STATE_DEAD := 6` in `matron_view.gd`) —
`dead_ = true`, `drop_stock()`, following the lethal-fall death
mechanics (`matron.cpp:3332`) — EXCEPT the body removal, which is
 deliberately separated (see below). **Native death and visual
disappearance are SEPARATE.** The sim transition may happen
immediately on the killing judged blow, but her body must NOT
visually disappear on that tick: the killing blow gets a readable
physical death/fall reaction — she falls, settles into a final
pose, dust bursts — and her presentation lifetime runs past the sim
transition until that beat has read. `remove_body()` (or its
equivalent) runs only after the presentation beat, on a
presentation-lifetime constant pinned by S8's act run, never as an
instant vanish. What that means in play: sim-side she stops
pursuing, grabbing, shoving, and sounding at once; visually she
falls and lies there, readable, and only then leaves. "Hit her ->
she vanishes" is a FAIL of S8. Death cue: fall + dust burst at her position (existing
knockdown-dust pattern, largest scale), the `strike_kill` haptic
[150, 1.0] priority (equal weight to the existing `death` kind, a
distinct kind name — her death is not his death), and a death sound
edge (Claude lane, spec'd). If the killing blow lands while she holds
him, he is set down on her deck by the existing hold-escape set-down
rule — never over open air, never dropped into a fall.

**DEFAULT (owner-confirmable, NOT blocking any slice): death ends her
pursuit for that run, and the player keeps climbing freely.**
Checkpoint restore keeps its existing semantics, already in code:
"A restore puts her back as the checkpoint had her, dead or not"
(`matron.cpp` restore path resets `dead_ = false`) — so dying and
then restoring an earlier checkpoint brings her back, exactly as a
lethal fall does today. Damage is live-only (rule 34, §9.1): a
restore or reload brings her back undamaged, `damage_ns` /
`player_damage_ns` at 0. No slice needs to re-litigate this.
**The one open design point (does NOT block any slice):** what, if
anything, replaces her after death — a victory screen, a locker
respawn (Dormant → Emerging reset), or nothing (the tower is simply
hers no longer). If the owner picks respawn or a victory screen
later, it is its own slice; every kill slice below is complete
without it.

### 9.4 The 1-2 combination (jab-cross, looks matter most)

**Input: punch-button rhythm. The free attack (Action button) IS
this 1-2.** First press throws the lead fist;
a second press during the first strike's recoil plus a 6-tick grace
window queues the rear fist, which throws the tick after recoil ends
— zero dead time between fists. A press during windup/reach is
ignored (no mashing); a third press during the cross queues nothing
(a combination is exactly two fists; after the cross's recoil the
exchange is fresh). The combination is INPUT-RHYTHM, not
hit-confirmed: a whiffed jab still queues the cross (boxers throw
combinations that miss); only a hands-taken event (`strike_cancel`:
her fingers, a fall, a catch) cancels a queued cross, as it cancels
any punch today.

**Fists and mapping (this replaces the free-punch row of §2.1).**
Single press with no follow-up = left jab (the §2.1 jab: 7/4/10
ticks, 1500 N, 4 contact ticks, 66.7 N·s, 0.303 m/s). Queued second
press = right cross. The legacy right punch survives UNCHANGED only
as the hold-escape (`his_blow` path, size-independent, §1.5) — it is
no longer the free-punch default. If only one fist is available
(`strike_arm()`: right else left; a lost/broken arm removes that
side), the second press throws the available fist again as a plain
jab: no cross, no combo bonus, no broken animation. Arm selection
for the combo overrides `strike_arm()`'s right-first default for
exactly two throws (lead left, rear right), then resets.

**Per-fist animation (first-person, camera-led, §8.2 pattern).**
- Left jab: left arm from guard (mirror of the verified right-arm
 targets: side −0.17 guard, −0.20 cocked, −0.05 out), short
 draw-back, snap out, minimal recoil overshoot. Shoulder rotates
 slightly into the punch; hips stay square. Reads as FAST.
- Guard return between fists: BOTH fists return to guard during the
 handoff tick — the cross does not throw from the jab's extended
 arm. This single frame of guard return is what sells "boxer"
 instead of "flailing."
- Right cross: full shoulder rotation (torso twist toward the target,
 camera rolls ~2° with the twist), weight transfer (camera pushes
 forward ~0.10 m and down ~0.03 m at contact, then springs back with
 the kick-scaled recoil from §8.2), rear fist travels farther
 (out-target back −0.70, vs jab −0.62) and returns to guard on the
 same spring. Reads as HEAVY.
- Weight transfer is camera-led: the player has no drawn body mesh
 (§8.7), so the transfer reads through camera push/roll plus her
 reaction, not through a visible torso. The act run for the 1-2
 must show the camera track (push on cross, roll on twist, guard
 return between fists) — a 1-2 with no guard return FAILS the slice
 even if both blows land.

**Timing (90 Hz ticks).** Jab 7 windup / 4 reach / 4 contact /
10 recoil (21 ticks, §2.1). Cross 10 windup / 6 reach / 5 contact /
16 recoil (32 ticks). Full 1-2: 53 ticks (~0.59 s) with zero gap,
vs 66 ticks for two separate legacy punches plus the inter-punch gap.
Cross windup (10 ticks) exceeds the 6-tick quiet window, which is
what makes the judging below work.

**Judging: TWO judged blows, not one combined window (decided).**
Each fist's contact ticks accumulate into its own window and are
judged separately by the existing `take_blow` path, because the
cross's 10-tick windup guarantees > 6 quiet ticks between the jab's
last contact tick and the cross's first. No judging change, no
combined-window constant, no Matron-specific code in strike code —
the combination is an input/animation layer over two ordinary blows.
Each blow feeds `damage_ns` separately (jab 66.7 + cross 83.3).

**What the 1-2 earns over two separate punches (decided: both,
modestly).** (a) Cadence: zero gap between fists (saves the full
second windup a fresh punch would pay — 11 ticks — plus the
inter-punch gap). (b) A slightly heavier cross: 5 contact ticks at
the lawful 1500 N cap = 83.3 N·s → 0.379 m/s, vs the jab's 0.303.
That 0.379 clears upright Pest `fells` (0.306) — the cross can DOWN
an upright Pest — while still only staggering upright Dangerous
(1.162) and Changed (2.537), and it never downs a crouched Matron
(bar 1.90). The cross stays inside the lawful pattern (same 1500 N
force cap, longer contact hold, exactly like the kick's 5-tick
pattern) with no velocity writes and no new Matron-specific strike
constant. The combination's reward is therefore real but bounded:
rhythm earns you a down on Pest and a faster two-blow exchange, not
a free kill.

### 9.5 Corrections list (every §1–§3 assumption this overrides)

1. **Header owner-rules bullet ("knock her down, never dead,"
 2026-10-19; "this spec adds no [damage] number")** — SUPERSEDED
 2026-11-01. New rule: she can be killed (§9.1–§9.3). A damage
 number now exists (`damage_ns`, §9.1). The rest of the header
 (no velocity/gravity writes, one budget, general skills,
 failing-test-first, punch button always present) stands unchanged.
2. **§1.2 ("the only 'damage' in the game" = balance)** — REFINED.
 Balance remains the per-blow judge; damage is the persistent
 layer above it. Rage and damage are separate tracks (§9.1).
3. **§1.3 (punch cannot stagger; "its only real job is the hold
 escape")** — NUMBERS STAND, consequence changes. The punch still
 cannot stagger, but every landed punch now feeds `damage_ns`
 (50 N·s) even when absorbed. The "feels dead" fix is S1 flinch +
 §9.2 stages, not stagger alone.
4. **§1.5 ("While she is Down: ... NOT for more damage")** — PER-BLOW
 RULE STANDS, purpose expands. No accumulation during Down
 (§9.1), but the Down window now also displays her damage stage
 (§9.2) — the player reads how close she is to death while she
 lies there.
5. **§1.5 ("no ground-and-pound ... by construction")** — STANDS.
 Offer stays off while Down; no slice adds a Down attack.
6. **§1.6 / §2.2 (what the player sees)** — EXTENDED by §9.2
 stages and §8.4's stage pip (secondary only). Body first, HUD
 second — a HUD-only damage bar would violate the owner decision.
7. **§2.1 moveset, Punch row ("existing, unchanged ... kept exactly
 so nothing regresses")** — CHANGED for free punches (§9.4):
 single press = left jab; the legacy right punch survives only as
 the hold-escape. Jab row: now also the 1-2 lead. NEW row: Cross
 (queued second press; 10/6/16 ticks + 5 contact; 83.3 N·s;
 0.379 m/s). Sweep, kick, slip rows unchanged.
8. **§2.3 ("Never-dead enforcement stays structural, not promised")**
 — REPLACED. Death enforcement is now the §9.1 threshold plus
 the §9.3 `State::Dead` transition. Down immunity stays per-blow
 only (§9.1); it is no longer a promise she always gets up.
9. **§2.4 ("What winning an exchange buys (never death)" + "What
 winning NEVER buys")** — REWRITTEN in effect. Stagger/knockdown
 buys stay as written. NEW top entry: winning the long fight buys
 her DEATH (§9.3) and free climbing for the run (default). The
 never-list shrinks to: no permanent disable other than death
 itself, no ground-and-pound, no damage during Down.
10. **§3 slices S1–S6** — REVISED ORDER AND CONTENT (see §9.6).
 S1–S3, S5 unchanged in content. S4 becomes jab + 1-2. S6 moves
 after S7 and gains stage tells. New slices S7 (damage + stages)
 and S8 (death state).
11. **§4 risks** — ADD: (a) threshold risk (6000 is owner-set
 and stands; 4000–8000 is a tuning range only, not permission to
 retune; if playtesting shows the kill unreachable in a normal run,
 that goes to the owner, never to the bars); (b) stage-read risk (if
 act runs show the body tells do not read at combat distance,
 strengthen pose deltas before adding HUD weight — body first is
 the owner's rule, not a preference); (c) 1-2 feel risk (if the
 guard-return frame reads as a hitch rather than boxing, tune
 the handoff tick, not the combination window).
12. **§5 files touched** — ADD: `src/sim/matron.hpp` (new
 `damage_ns_` / `damage_stage` fields + view export),
 `src/sim/matron.cpp` (accumulation in the judged-blow path,
 threshold check, `State::Dead` transition),
 `godot/presentation/first_person_arms.gd` (left-fist targets,
 cross weight transfer, guard return), `life/matron_view.gd`
 (`_pose` stage offsets), `main_feel.gd` (`damage_stage`,
 `strike_kill` kinds), `ui/hud.gd` (stage pip, death cue).

### 9.6 Revised slice order (S1–S8, each ≤ 300 lines, failing test first)

S1 (every landed blow reads) and S2 (sweep) and S3 (kick) unchanged
from §3. Then:

- **S4 — Jab + 1-2 (revised).** Failing tests first (native,
 seeded `World` scenarios + strike-phase tests): single press =
 left jab (7/4/10, 0.303, stagger-but-never-down vs upright Pest);
 second press in recoil+6-grace = right cross (10/6/16 + 5 contact,
 0.379, downs upright Pest, staggers Dangerous/Changed faced);
 press during windup/reach queues nothing; third press queues
 nothing; one-fist-available falls back to jab-only; cross windup
 guarantees two separately judged windows (assert via two
 `blow_mps` edges, not one). Change: jab + cross strikes, combo
 window + alternation state, left-fist targets, guard return,
 weight-transfer camera, strike-dict `move` field (§8.5). Evidence:
 native balance + phase groups + Godot act run (camera track shows
 guard return; a 1-2 with no guard return FAILS). **S4 visual
pass criterion (owner words): the sequence must convincingly read
as guard -> left jab -> guard return -> right shoulder/weight
transfer -> cross -> guard.**
- **S5 — Slip.** Unchanged from §3.
- **S7 — Damage accumulation + stages (NEW, sim).** Failing tests
 first (native): judged blows from ALL valid sources sum into `damage_ns`
 (player: punch 50, jab 66.7, cross 83.3, sweep 60, kick 166.7,
 asserted per move; environmental: real judged impulse);
 `player_damage_ns` tracks the player share separately;
 mixed-source window test (player strike + environmental hit in one
 judged window attributes each source's real contribution only, no
 double-count, no whole-window-to-player); accumulation pauses
 during Down and resumes after (total unchanged through Down);
 checkpoint restore and reload return `damage_ns` /
 `player_damage_ns` to 0 (live-only, rule 34); stage flips at 1200/2700/4200/5400 only
 (0–4) — ≥6000 is death (S8), NOT a stage; recovery scaling
 ×(1 + 0.15 × stage) on stagger/down times; offensive numbers
 unscaled at every stage.
 Change: `damage_ns_` field, judged-blow hook, stage computation +
 view export, recovery scaling. Evidence: native balance + view
 groups. No presentation in this slice beyond the export.
- **S6 — Fight tells, revised (presentation, moves after S7).**
 Original S6 content (windup telegraph + stage HUD) unchanged,
 PLUS: stage pose offsets (§9.2 lean/hip/guard/limp per stage),
 stage-change sound/haptic pairs (§9.2), stage pip in HUD
 (secondary only), Down-window stage display. Failing tests first:
 act runs asserting each stage's pose delta reads at combat
 distance (if a stage does not read, the slice fails — strengthen
 pose, do not add HUD weight). Evidence: act runs + existing
 presentation groups green. Depends on S7's export.
- **S8 — Death state (NEW, sim + presentation).** Failing tests
 first (native + act): `damage_ns` crossing 6000 on a judged blow
 → `State::Dead`, `dead_ = true`, body removed, pursuit ends
 (no Hunting/Searching/Grab/Shove after); hold ends via
 `end_hold(Dead)` with set-down on deck (never open air);
 checkpoint restore resets `dead_ = false` per existing semantics;
 death cue + dust + `strike_kill` haptic fire once (not per tick).
 **No-vanish rule: sim transitions to `State::Dead` immediately,
 but the act run must show a readable fall + settled final pose —
 she does not disappear on the killing tick; body removal happens
 only after the presentation beat.** Change: threshold check +
 transition + hold-end wiring + death cue/haptic + separate
 presentation lifetime for the final pose. Evidence: native state +
 hold groups + act run (she falls, lies dead visibly through the
 beat, player climbs freely, no further Matron actions).
 The respawn/victory-screen question (§9.3) is explicitly OUT of
 this slice.

### 9.7 What §9 could NOT verify (honest gaps — do not treat as fact)

1. **Rig bone-level pose API**: `matron_view.gd` `_pose` and the
 STATE_STAGGERING/DOWN poses are verified (§8.2), but
 `matron_rig.gd`'s bone-level controls were NOT re-read in this
 pass. Lean angles (5°/12°/20°/28°), hip drops, guard-drop arm
 angles, limp asymmetry, and the swollen-eye lid pose are
 PROPOSALS for S6 act runs to confirm — if the rig cannot express
 one, S6 substitutes the nearest readable tell and records it.
2. **Threshold reachability in a real run**: 6000 N·s is derived
 from verified bars, not from measured fight encounters — no data
 was found on how many player strikes a typical run contains. S7's
 tests pin the meter; the number itself is owner-set (6000) and
 stands.
3. **Recovery-scaling factors**: ×(1 + 0.15 × stage) is a proposal,
 not a measurement. S7's tests must pin both the scaling and the
 unscaled offensive numbers.
4. **Cross contact-tick count**: 5 contact ticks at 1500 N is
 chosen to land the cross at 0.379 m/s (above Pest `fells`,
 below Dangerous `fells`) — lawful pattern, but the tick count
 itself is a proposal for S4's tests, not a verified constant.
5. **Left-fist guard/cocked/out targets**: mirrored from the
 verified right-arm targets (§8.2); the mirror was NOT confirmed
 in `first_person_arms.gd` in this pass — S4's act run must show
 the left fist drawing correctly.
6. **Carried caveats from §7/§8.7**: gravity constant (scales every
 bar and hence every N·s figure here); `stagger_left_s` still to
 be added by S6; sound implementation stays Claude's lane;
 `strike_kill` / `damage_stage` haptic kinds are new constants
 following the §8.1 pattern, not existing kinds.

### 9.8 Owner corrections applied 2026-11-01 evening (in-place edits — no parallel rules)

Cory's four corrections, applied by editing §9.1 / §9.2 / §9.3 / §9.6
above (plus §9.4 / S4 wording), so no contradictory rule stays active
anywhere in this spec:

1. Environmental damage counts (§9.1, S7): `damage_ns` += judged
 impulse from ALL valid `take_blow` sources; `player_damage_ns`
 is attribution only. Replaces the earlier player-only meter and
 the S7 "environmental blows do NOT feed it" line.
2. Stage boundaries (§9.2, S7): 0–1199 / 1200–2699 / 2700–4199 /
 4200–5399 / 5400–5999 = stages 0–4; ≥6000 = DEAD, not a stage
 transition. Replaces S7's "boundaries at .../6000 flip
 `damage_stage` 0–4" wording.
3. Checkpoint semantics (§9.1, §9.3, S7), superseded by rule 34:
 damage is live-only and does not persist through checkpoints; a
 checkpoint rewind or a reload brings her back undamaged and alive;
 damage persists through combat / stagger / knockdown within the
 live run only.
4. Death presentation (§9.3, S8): sim `State::Dead` may be
 immediate; visual removal is separate and later — readable fall
 + settled pose first, never instant vanish.
5. Two-handed clarification (2026-11-02, Cory): "two-handed boxing"
 means alternating fists only (left jab, right cross, guard return
 between). Both hands as one punch is explicitly out, stated in
 §9 item 3 and §9.4.

Reaffirmed unchanged: she CAN be killed; damage visibly accumulates
on her body; real two-handed boxing; free attack = left-jab /
right-cross 1-2; body readability over HUD; S4 visual fail sequence
guard -> left jab -> guard return -> right shoulder/weight transfer
-> cross -> guard; no ground-and-pound while Down; rage and
persistent damage stay separate systems.

### 9.9 Matron hearing + Turn redesign (separate slice — PR #75, ChatGPT-owned, DO NOT duplicate)

Owner decisions 2026-11-01 (email thread + issue #3 comment 6566474026),
folded here ONLY so combat/tells work does not encode old assumptions.
This spec does not implement the redesign; PR #75 does. S1/S6
presentation must be built against this model, never the old one:

1. She hears ALL real gameplay sounds that lawfully reach her ear —
 footsteps/run/climb, world impacts/crashes, machinery — under the
 existing acoustic propagation/evidence model (source loudness +
 carried/occluded path to her ear). Do NOT special-case hard
 landings as the only meaningful sound.
2. Microphone is optional input, never required. When enabled, room
 loudness is another player sound source at the player's position
 through the same hearing path; microphone-off must not disable
 ordinary in-game hearing.
3. The Turn must NOT remain player-trigger-only. The old rule (clear
 sight within 13 m, or effectively maximal player noise against
 `kTurnNoiseLoudness` 0.95 while sprint tops at 0.8) let a
 distant/quiet player suppress the Turn indefinitely. That is gone.
4. Turn is ESCALATION, not permission to pursue/traverse. A far
 player never needs to "cause a Turn" for her to use a lawful
 physical route (see #73, which already corrects that).
5. Desired Turn model (PR #75): keep a minimum anti-spam cooldown,
 but add a deterministic maximum wait while actively Hunting.
 Close sustained sight or any sufficiently loud sound AT HER EAR
 may trigger the Turn once eligible; if neither happens before
 the hunt deadline, she Turns anyway. Ear-level evidence
 (`heard_level_`), not source-type checks ("was this a hard
 landing?"). Deadline presentation is a dramatic ramp near expiry
 (body/head twitch, breath/tension/pose escalation) — NO big
 arcade HUD countdown unless separately approved. Existing 0.9 s
 Turn windup remains the final warning.
6. S6 fight tells must NOT encode hard-landing-only or
 player-trigger-only Turn assumptions. If a tell references a
 Turn trigger, it references ear-level sound, close sustained
 sight, or the hunt deadline — the PR #75 model.

PR #75 status (tracked, not FB Bot's): DRAFT PROOF, head 8d1eeb8f,
files matron.cpp / matron.hpp / sim_tests_matron_moves.cpp only,
test-only run 41370181142 + implementation run 41370220308 active at
time of writing. Review/landing follows the usual rule (independent
review + green proof) when it leaves draft; FB Bot does not start a
parallel Turn/hearing implementation.
