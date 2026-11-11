# J-108 bait lift: evaluator gate (#427, step 1)

`stage1dof.py` (the causal-mechanism skill's, as M, N and O), h = 1 ms, every case rerun at h/4: INTEGRATED.
q is A's descent = B's rise; the stop at 22.0 m is A on the 662 deck. Guide friction is one Coulomb value
(`set_guide_friction`: static = kinetic). "Arrive" is the speed when A meets the deck.

## The plan as written (`plan.json`): A 250 + her 220 + chain 2.65 x 22 m against B 373 + him 85

| f (N) | 250 | 300 | 350 | 400 | 450 | 500 | 550 | 600 |
|---|---|---|---|---|---|---|---|---|
| top (m) | 22.0 | 22.0 | 22.0 | 22.0 | 18.28 | 14.49 | 10.69 | 6.88 |
| arrive (m/s) | 2.63 | 2.15 | 1.53 | 0.19 | stops | stops | stops | stops |
| time (s) | 11.5 | 12.6 | 14.3 | 18.4 | 19.1 | 19.2 | 19.2 | 19.3 |

From 450 N up both of them are left at rest mid-shaft.

## Why no tuning holds 250 to 600 N

Friction is what holds him alone on A, so his net pull must stay under 250 N. Her on A and him on B pull
(220 - 85) - 85 = 50 kg, 490 N, more than him alone, so the ride's drive is at most 740 N. The two ends of the
band then differ by 350 N x 22 m = 7.7 kJ at the top, whatever the chain, A or B: with the roughly 930 kg
that moves, if 600 N just arrives, 250 N arrives at 4.1 m/s. Best tuning found (`best_wide.json`: chain
1.29 kg/m, B 337.9 kg, him alone at 249 N against 250 N, no margin):

| f (N) | 250 | 300 | 350 | 400 | 450 | 500 | 550 | 600 |
|---|---|---|---|---|---|---|---|---|
| arrive (m/s) | 4.12 | 3.82 | 3.49 | 3.12 | 2.70 | 2.21 | 1.57 | 0.16 |
| time (s) | 9.6 | 10.2 | 10.9 | 11.8 | 13.0 | 14.7 | 17.5 | 25.6 |

Heavier platforms narrow the spread only by slowing it: A and B near 2 t give 1.9 to 0.05 m/s, but 57 s at
600 N; near 4 t, 1.35 m/s but 78 s.

## Two ways that work (the integrator rules)

- (a) M, N and O's band width: declared 400 N, plus the engine's 35 N (AS-010 M), plus or minus 50 N.
  Chain 2.75 kg/m x 22 m, A 250, B 365.2 (`option_a.json`); him alone 297 N against 385 N, nobody on B's
  stop with 537 N. Arrives at 2.18 / 1.90 / 1.56 / 1.14 / 0.36 m/s at 385 / 410 / 435 / 460 / 485 N,
  12.4 to 17.5 s. B's guide runs 0.5 m past the floor, so at 2.2 m/s B coasts 0.24 m on and drops back onto
  a dog: its floor ends 0 to 0.2 m over the plant floor.
- (b) The whole band with a governor on B's guide (the kit's, as J, K and L's cages): no chain, A 250,
  B 316.7, him alone 180 N, governor 1.5 m/s, level 0.5 m/s^2. Every friction from 250 to 600 N arrives at the
  governor's 0.08 m/s creep, in 17.3 to 25.0 s; she alone and both on A land at the creep too. That is a
  sum from the same equation (the evaluator has no governor), and it parts from N's "no governor".
