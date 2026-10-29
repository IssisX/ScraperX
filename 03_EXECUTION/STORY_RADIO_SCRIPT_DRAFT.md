# Story radio — script draft for Cory's approval

**Status:** Story proposal only, not a cue manifest, voice approval, event implementation, or request to change native state. **Base:** `ScraperX-Claude` (2026-10-29). **37 lines; 2 speakers.**

## Pitch

Jack Miller climbs into a derelict but very much working industrial tower on a Saturday, and foreman Walt Brenner wants him gone before the owner notices. An unfamiliar operator, Iona, slips onto the work channel and speaks in precise directions tied to the machinery Jack actually moves. Neither will tell him much about the locked room on the 154-metre deck. After something gets out, the foreman's warnings turn into pleas, Iona's certainty starts sounding like fear, and the climb stops being about how high Jack can get and becomes a question of whether he can keep climbing while the tower's steel carries someone else behind him. The machinery always does its real work; the radio only reacts to what actually happened.

## Voices and continuity

- **Walt Brenner (BRENNER):** the established foreman on the native `ForemanRadio`; impatient, dry, increasingly protective. The four existing native beats are kept verbatim here (`foreman_s1_cage`, `foreman_stack`, `foreman_locker`, `foreman_ring220`); they would remain single-owner beats, **not** play twice through a second radio player.
- **Iona (IONA):** a *proposed* second live person on the work-radio frequency, giving concise observations. Her physical location, employer and history are deliberately unspecified; there is no Iona scene actor, handover record or terminal. Cory must approve this radio-only role. The three previously accepted **manifest lines** are preserved exactly and identified by their current keys.
- **Jack Miller:** the player, addressed but not a third radio speaker. Trunk's existing Brenner radio and the owner's 2026-10-21 naming decision identify him as Jack. **Existing inconsistency to resolve separately:** `godot/presentation/identity/work_id_page.gd` currently prints `RO VALE / ACCESS & RIGGING`, `USE DEFAULT RO`, and `DEFAULT RO`. This draft does **not** legitimize those as the climber's name or edit the Work ID UI.

**Trigger contract:** All trigger sources below are either physical zones read from the native player position/support, existing one-shot world-state predicates or transitions observable in trunk's public Godot/native readback. `First` means first occurrence this climb, not the first elapsed timer. A triggering line is a *proposal to present*; no hook is claimed to have been implemented. Only play one short transmission at a time, suppress stale lines when their condition no longer makes sense, and don't stack two lines over the same machine beat or an active hold. Recovery (22) is an event cue, not a mandatory point in the ascent. Give important bodily sounds and the Matron's hold priority over speech.

**Verbatim exception:** `receiving_iona_freight_one` already has three short sentences; its exact words are retained as requested rather than falsifying the preservation requirement to enforce the new one-or-two-sentence target. All other new lines are one or two sentences.

## Proposed transmissions

| # | Speaker | Proposed key | Exact words | Existing place/state trigger |
| --- | --- | --- | --- | --- |
| 01 | **IONA** | `receiving_iona_freight_one` | “Freight One is eleven metres up. Find firm footing there. The crane yard is ahead.” | First entry to the real S1 yard approach at grade: native `get_player_position()` in the x≈10, z≈-117 zone while `is_player_grounded()` on grade. |
| 02 | **BRENNER** | `foreman_s1_cage` | “Miller? It's Saturday. What are you doing on my tower?” | First native support `get_support_entity_id() == 2200` (S1's grade cage; existing foreman `s1_cage` beat). |
| 03 | **BRENNER** | `s1_valve_pawled` | “There goes the bucket. That thing works better when nobody's watching it.” | Native S1 valve pawled and catch released: `is_stack_s1_valve_pawled()` and `!is_stack_s1_catch_latched()`. |
| 04 | **IONA** | `receiving_iona_keep_the_height` | “You’ve found your way up. Keep the height. The Crown is still above you.” | First grounded tower support (`get_support_entity_id() == 11`) at player y≥22.5, on deck 2 after S1. |
| 05 | **IONA** | `c1_duct_hang` | “You're hanging from a ventilation duct. That's one way of filing a maintenance request.” | C1 duct hang at x≈20, z≈-123, y≈26–28: native `get_traversal_state()` hanging plus native position zone (the `touch_stack` duct catch). |
| 06 | **BRENNER** | `deck3_monorail` | “That little yellow beam is a monorail, Miller. It wasn't laid for your boots.” | First grounded crossing on C1's monorail x≈12.5, z≈-125 to -120, y≈34 (`src/sim/band_stack.cpp`; `touch_stack`). |
| 07 | **BRENNER** | `foreman_stack` | “Get down before somebody sees you.” | After S1 cage beat: native tower support id 11 and player y>44.5 (existing foreman `stack` beat). |
| 08 | **IONA** | `s2_walking_beam` | “The walking beam's moving. Let the cage carry the weight, not your knees.” | Native `get_stack_s2_cage_travel() > 1.0` with player support S2 cage entity 2210 (S2 between 44 and 66 m). |
| 09 | **BRENNER** | `deck6_66` | “Sixty-six metres. Still time to tell me this is a very elaborate wrong turn.” | First real grounded tower deck-6 support at player y≥66.5 (S2 top / C2 start). |
| 10 | **IONA** | `deck8_88` | “The next deck is solid. The space between them isn't.” | First real grounded tower/route support at player y≥88.5 following the C2 climb (first-reach latch, native support and height). |
| 11 | **BRENNER** | `s3_carriage` | “That carriage is on a counterweight, Jack. Don't try to stop it with your hands.” | Native `get_stack_s3_cage_travel() > 1.0` while supported by S3 cage entity 2220 (88→132 m). |
| 12 | **IONA** | `deck12_132` | “The trusses start ahead. I wouldn't expect the view to improve.” | First supported player position y≥132.5 after S3, on the real stack/deck-12 steel. |
| 13 | **BRENNER** | `deck14_154` | “Don't go near the locker on fourteen. I'm not explaining why over an open channel.” | First grounded support on deck 14 y≥154.5, before `get_matron_state()["released"]` becomes true. |
| 14 | **BRENNER** | `foreman_locker` | “What was that? Miller, get away from that locker.” | First `get_matron_state()["released"] == true` (existing foreman `locker` beat). |
| 15 | **IONA** | `locker_released` | “That noise wasn't a loose chain. Keep moving.” | First native Matron `released == true`, queued after the foreman's locker beat; no claim about an unseen actor's identity. |
| 16 | **IONA** | `pipe_first_climb` | “The pipe's rattling again. You're not the one climbing it, are you?” | First increment of native `get_matron_state()["pipe_links"]` while `on_pipe`/pipe contact is true (the deck-154 riser). |
| 17 | **BRENNER** | `matron_first_throw` | “Something hit the steel. Miller, tell me that was your toolbox.” | First increment of native `get_matron_state()["throws"]` (her actual loose-stock throw). |
| 18 | **IONA** | `matron_first_grab` | “Miller, don't answer the radio. Use whatever hand you've still got.” | First native `get_matron_hold_state()["grabs"] > 0` and actual hold `held` true (not a proximity guess). |
| 19 | **BRENNER** | `matron_grip_release` | “You're loose. Get away from the edge while it stays that way.” | Native hold transition `held: true→false`, player alive and `get_death_count()` unchanged; do not play for a fatality/reset. |
| 20 | **IONA** | `ring176` | “You made the next ring. I can't see what's under you from here.” | First grounded native support at y≥176.5 on the constructed counterweight-well ring; no fabricated Iona camera. |
| 21 | **BRENNER** | `foreman_ring220` | “Don't come down. She's below you. The only way off is the top.” | `get_matron_state()["released"]` and first player y>220.5 (existing foreman `ring_220` beat). |
| 22 | **IONA** | `receiving_iona_recovery` | “Find your footing. You can try another way up.” | Any first *survived* fall of ≥3 m: observed native airborne→grounded, high-water y minus landing y≥3 m, same native death count. Event-based and independent of story order. |
| 23 | **BRENNER** | `wet_d_pipe_closed` | “You've put the pipe back across the gap. Now let the water do the work.” | First `get_wet_state()["d_pipe_whole"] == true` (D float-ram fill line, 220→256 m). |
| 24 | **IONA** | `wet_e_chiller_fall` | “I heard the chiller go. Please tell me you weren't underneath it.” | First `get_wet_state()["e_chiller_travel"] <= -24.0` (real chiller drop driving the 256→298 m air cab). |
| 25 | **BRENNER** | `wet_f_hose` | “A hose on that ram makes a fine lever. Remember it's a terrible place to stand.” | First `get_wet_state()["f_hose_coupled"] == true` (F hydraulic accumulator platform). |
| 26 | **IONA** | `tp340` | “Three-forty. There's actual floor beneath you again.” | First grounded player on real transfer plate 340, y≥340.25, support entity 1005. |
| 27 | **BRENNER** | `shop_g_scaffold` | “You got that scaffold moving. I'd rather trust the rope than the handrail.” | First `get_shop_state()["g_platform_travel"] >= 33.7` (stage G netted scaffold, arriving at 374 m). |
| 28 | **IONA** | `shop_h_girder` | “The girder's tipped. Keep clear of its tail.” | First native transition `get_shop_state()["h_girder_latched"]: true→false` (stage H, 374→418 m). |
| 29 | **BRENNER** | `shop_i_monolith` | “Twenty tons of falling steel. You really do hate taking stairs.” | First `get_shop_state()["i_cage_travel"] >= 43.9` (stage I monolith cascade to 462 m). |
| 30 | **IONA** | `crane_j_rail_joint` | “The joint's back in the rail. That wagon has a road again.” | First `get_crane_state()["j_rail_whole"] == true` (stage J rail repair at 484 m). |
| 31 | **BRENNER** | `crane_k_pendulum` | “That's a retired crane, Jack. Nobody told the jib it was retired.” | First `get_crane_state()["k_cage_travel"] >= 43.9` (stage K jib pendulum and cage to 572 m). |
| 32 | **IONA** | `crane_l_cab` | “The cart goes down. Your cab goes up. I'd stay in the cab.” | First `get_crane_state()["l_cab_travel"] >= 67.9` (stage L counter-driven lift, TP-640). |
| 33 | **BRENNER** | `tp640` | “Six-forty. Funny how much farther the tower looks from up there.” | First grounded support on actual transfer plate 640 (native position y≥640.25, supported; no fake route advancement). |
| 34 | **IONA** | `service_m_reel` | “The reel paid out and the cage rose. It's a strange little trade.” | First `get_service_state()["m_cage_travel"] >= 21.45` (stage M, landing at deck 662). |
| 35 | **BRENNER** | `service_n_hopper` | “That gravel's louder than the radio. I hope the cage is quieter at the top.” | First `get_service_state()["n_cage_travel"] >= 21.45` (stage N hopper-driven cage, deck 706). |
| 36 | **IONA** | `deck750` | “Seven-fifty. You've still got to put your feet on the next piece of steel.” | First grounded/native-supported player y≥750.5 on the real high-service deck (as traversed in `touch_stack_upper`). |
| 37 | **BRENNER** | `summit_receiver` | “You're standing on the upper receiver, Miller. You going to tell me why?” | First grounded native-supported player y≥803.3 on the derrick's upper receiver (`touch_stack_upper`; physical summit, no teleport or semantic arrival). |

## Boundaries for approval

- **No imported fiction:** no Ro Vale as the climber, Ari/Dev, duplicate-person plot, occupancy survey, handover terminal, hanging-mesh personnel path or unbuilt swing stair. Iona is *only* the proposed second radio voice, never an actor with invented simulated travel.
- **The creature is not named or explained.** Dialogue never claims to know how it was made or why it hunts. The native Matron supplies its own movement, waking, contact, throws and holds, and no script overrides them.
- **No invented victory condition:** the only summit beat is the real supported upper receiver. Why Jack goes up and what happens after he reaches it remain Cory's decisions; this text is intentionally not a new mission contract.
- **No new audio automatically approved:** the old Iona recordings remain gated; all new dialogue requires approved, genuine human recordings before release. Walter's existing caption beats stay valid. This document makes no sound files, permissions or runtime edits.

## Three optional story directions for Cory

1. **The foreman's secret (recommended):** Walt knew enough to lock that room, but not enough to prevent it opening. He never confesses outright; his evasions get smaller as Jack gets higher.
2. **The unreliable second frequency:** Iona gives accurate mechanical guidance but speaks as if she can hear things that are no longer in range. Keep her human and the source uncertain, without inventing an impersonator.
3. **The indifferent tower:** Strip the mystery nearly bare. Most radio lines are weary shop talk, and the strongest horror comes when the machines keep running while Jack is being hunted.

**Owner decision:** Approve the base dialogue and the radio-only Iona voice, choose direction 1/2/3, or request edits. Approval of the script alone does not authorize implementation, replacement of the original recordings, a new Matron mechanic, or any alteration to the GDD's physical summit rule.
