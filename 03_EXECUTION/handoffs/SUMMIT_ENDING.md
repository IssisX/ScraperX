# The ending: seat the crew's last beam

Base: `ScraperX-Claude` at `1bd28c1`. Work branch: `chatgpt/summit-ending-20261029`.
Status: implemented candidate; native, touch and CI proof in progress. Not cleared to land.

## Before and after

Before: reaching the summit displayed THE TOP. The beacon and parachute runway
worked, but the overtime crew's unfinished job had no physical counterpart or payoff.

After: a 900 kg I-beam hangs on two chain blocks from an erection strongback
beside the summit arrival deck. The stools, guides, load and hoist are native kit
parts, drawn from the same shapes that collide. The hoist reuses the yard crane's
tension-only RopeConstraint. Holding Action at the brake pays out the chains;
gravity lowers the beam, and lifting the thumb brakes it where it is. Nothing
writes a body position or velocity. The two bearing stools are separate bodies:
actual contacts at both flanges, slow motion and unloaded, slack chains must all
hold before seating is accepted. Arrival, time or a suspended near miss do not count.

The second action, FREE SLING, removes both rope constraints only after the load
rests on steel. The beam stays on its bearings without the hoist. The foreman's
caption follows the seating state once per launch:

> I signed her accident off as her fault. The beam's home, Jack. Your turn.

This is a draft line, text only. The radio's existing machine squelch remains;
no speech or voice asset is added or approved. The owner can revise the wording.

The existing runway opens onto the city. The player runs off it, deploys the
existing parachute and reaches ground alive. The native observes that flight;
stowing the canopy or landing above grade clears its flight qualification.
Only the live canopy landing at grade earns the quiet SCRAPERX end card and credit.
The world continues to simulate; no forced camera ride or false arrival is used.

## State and restore

Summit owns placement, the rig and ending phase. Native snapshots expose that
state to the input context, radio and HUD. The input context does not decide
when the beam is seated. New bodies append after every existing kit body.
The existing save layout stores those bodies. Restore rebuilds a safe attached
hoist at the restored beam pose without moving it; a saved seated load can be
unrigged again. Radio and the final descent replay per launch.

## Proof

- Native: `SCRAPERX_ONLY=summit-ending build/host/scraperx_sim_tests`.
- Nearby regressions: `summit` and `save` (summit geometry, restores).
- Touch: `--uitest=touch_summit_ending`, registered in the driver's summit spawn
  and the CI touch loop. Real viewport touch lowers, brakes and frees the load,
  then walks the runway and deploys the canopy. It checks the native results,
  the final caption once, and a living landing at ground.
- Stills: the same scenario with `--capture=<prefix>` produces hanging beam,
  seated beam with final caption, city view, parachute, and end card.
- Final integration proof: same source commit on
  `chatgpt/integrate-summit-ending-20261029`; protected PR into `ScraperX-Claude`.

## Progress (Central)

- 14:34: Native first playthrough reached physical seating, unrigging, canopy and ground.
- 14:36: Independent source review found the near-seat suspended-load and stowed-canopy gaps;
  tightened both predicates and added falsifiers. Candidate is rebuilding.
