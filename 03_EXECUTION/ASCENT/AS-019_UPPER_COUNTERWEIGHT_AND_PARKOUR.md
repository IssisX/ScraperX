# AS-019 — upper counterweight and exterior climb

**Status: local ARM native and headless touch candidate passed; rendered exact-source CI pending. No Android play claim.** Profile: `MACRO-TRAVERSAL-STRICT`. This slice begins on the proven +44 m tower ring and ends only when the normal player stands on the native +66 m ring. The ordinary tower staircase remains an optional fallback; it cannot satisfy this slice's route proof.

## Physical route

The player walks east along the +44 m front band to a 4 m wide fixed approach at X=[2,6], Z=[−124,−120]. A 4 × 4 m platform outside the frame occupies X=[2,6], Z=[−120,−116]. Its walking top begins at +44 m and rises on exposed guides. A 3.2 t counterweight at X=10, Z=−118 descends on a separate guide. One visible cable runs from the off-centre platform eye over its +67.8 m sheave, across to the weight sheave, and down to the weight eye. A hand chain at Z=−115.8, outside the platform's vertical sweep, releases a mechanical catch on the weight. The player boards and pulls from the platform edge. The nominal platform mass is 2.8 t; an 85 kg rider reduces the gravitational imbalance to 315 kg. The counterweight releases about 34 kJ over 11 m with that rider before friction and stopping losses; these values are **chosen and derived design figures**, not measured runtime work. The effective pulley ratio and rider coupling still need a direct force/energy ledger.

The weight descends into a wide visible 1.6 m yielding timber bed whose initial top is +45.3 m. Its chosen constitutive values are 15 kN yield force, 200 kN/m elastic stiffness, and 1.5 kN·s/m damping. That bed is the energy sink; guides and sheaves transmit force and reactions into the tower. Its consumed stroke and work counters are captured with the checkpoint. A slow upper catch seats the platform at body Y=54.76 m, leaving its walking surface nearly flush with the +55 m fixed connector. The upper exit goes back to the real +55 m ring without an airborne leap. The two connector decks stay outside the platform's swept volume. Every static or moving part visible to the player is a Kit collision part.

From the +55 m ring the player traverses east to a compact service cabinet, catches the lip of a suspended duct, shimmies, and climbs a narrow vent/fascia onto the +66 m ring. This exterior path is a real use of the existing jump, hang, shimmy and climb verbs. It is authored away from the alternating ordinary stairwell. At +44 m the player chooses the exposed lift or the retained internal stairs; after the lift, the exterior parkour is the authored continuation. Neither choice disables the other.

## Closure and falsifiers

| Closure | Owner and required check |
|---|---|
| Kinematic | One 11 m platform rise corresponds to one 11 m weight fall through a taut 1:1 cable. Native guide travel and cable endpoints agree. |
| Force | Weight minus platform/rider gravity exceeds measured guide/bearing resistance; the bed's upward force arrests the falling weight. |
| Energy | The falling weight supplies platform/rider potential gain, kinetic energy, friction and finite timber deformation. The operator only releases a catch. |
| Reaction | Sheave bearings and both guides load the fixed gantry; the catch carries the armed weight at rest; the timber bed transfers terminal force to the frame. |

No pull must leave the platform at +44 m. No weight or no connected cable must prevent ascent. Early boarding and late boarding must have coherent outcomes. A missed exit must leave a reachable recovery route. A real checkpoint captured during the ride must restore platform, weight, cable, catch and bed history after a fatal fall. The acceptance run must start at grade, operate the pipe bridge and swinging stair with normal input, use the lift, perform the exterior climb, and end grounded on the +66 m tower support. Require first-person rendered touch proof, collision/visual agreement, retained regressions, exact-source Android export, and separate Fold play and sustained performance observations before any mobile-ready claim.

## Local evidence and remaining checks

- **MEASURED, local ARM native, 90 Hz:** grade-to-+66 m normal-input mode 5 ends grounded on support entity 11, no deaths. Lift peak speed is 2.79 m/s; the largest 0.1 s velocity-window acceleration is 2.73 m/s². A one-frame 0.12 m/s launch settling impulse remains visible in the raw derivative; it is below the distance and duration of a sustained ride jerk. After 3 s the platform is at Y=54.76 m with zero velocity and the player is supported on it.
- **MEASURED, local ARM native:** mode 6 leaves the unpulled lift latched through 25 simulated seconds. Mode 7 captures the seated lift, jumps the player to a fatal fall, restores platform, weight and player within the checked tolerances, then reaches supported +66 m with one death.
- **MEASURED, local ARM Godot 4.7 headless:** continuous `touch_upper` uses normal touch inputs from grade through the pipe bridge, façade, swinging stair, side release chain, seated lift and exterior parkour. It ends at player Y=66.900 on support entity 11 with zero deaths. The headless run does not prove rendered first-person framing.
- **MEASURED, local ARM regression:** 17 of 18 native tests passed. The only failure is the legacy AS-002 mid-landing fixture at the same point observed on the preceding green source; the x86 CI suite must pass before release.
- **Pending:** rendered x86 first-person screenshots and whole-workflow gate; no-weight/no-cable falsifiers; quantified Jolt energy ledger and parameter-band sweep; Android export identity and actual Fold play/performance. The current local measurements establish a functional candidate, not a completed strict compiler proof.
