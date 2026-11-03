# PE and Builder: the jobs (whole)

Handed out by the integrator on 11-02 (board #87, now #107). Rulings since are on board #107.

Laws for every job here:
- Build with CI's compilers: host g++ 13.3 with the CMake flags in `.github/workflows/wo000-delivery-spine.yml`, and NDK `29.0.14206865` clang. Other compilers do not count.
- Run only the tests your change reaches. CI runs the rest: push `<you>/integrate-<job>`, one per job, reused.
- Protected changes go as their own PR into `ScraperX-Claude`, with a plain before/after: a test's checks, limits, timeouts, world geometry, and her force/power/mass/leg/launch limits.
- Run `python3 tools/rule_check/rule_check.py <base> <head>` before each PR.
- Her budget stays 8 kN / 44 kW.
- Reviewer: a different model from yours (ChatGPT or Muse).

## PE 1: Machines she hears (gap 10 in `03_EXECUTION/GAP_LIST.md`; revives #48)
- **Goal.** Every loud machine and hard hit in the tower reaches her hearing, with loudness from measured energy and material: cages, counterweights, plates, doors, lifts and wall hits, not only loose stock, the forklift and the yard crane.
- **Why it is hard.** The sound comes from measured physics (contact impulse and motion), not scripted events. The contact listener serves the whole world. It must stay cheap per tick and deterministic, without changing body order.
- **Owns:** the impact and sound feed in `PhysicsWorld::step`; `ImpactLedger`; impact reporting in the contact listener; new sound sources in the machine files. `Matron::hear` is only called, not changed.
- **Finish facts:** each listed machine makes a heard event when it moves loudly or hits hard, with loudness from measured energy; a quiet machine draws nothing; one test per source; the full native suite green in CI; the determinism group passes.
- **Protected:** the new checks (their own PR). The behavior lands without one. (Now #98 and #99.)

## PE 2: Stock above 396 m (gap 14)
- **Goal.** `LooseStock::dress_tower` (`src/sim/loose_stock.cpp`) builds lived-in rings above 396.25 m to the top of the route.
- **Why it is hard.** New bodies change world body order, so tests drift (use the `test-drift` skill; never weaken a check). Pieces sit lawfully on decks he can reach, block no route or machine, and stay deterministic and cheap on the phone.
- **Owns:** `LooseStock::dress_tower`, the new ring rows in `LooseStock::placements`, `run_loose_stock`, the new upper entity range, and its one call in the `PhysicsWorld` constructor (ruling 11-02 5:25 PM).
- **Finish facts:** every new ring holds at least one piece and maps to a ring; every route test is green; the full native suite and touch scenarios are green in CI; body count and step time are measured before and after.
- **Protected:** yes. (Now #101.)

## PE 3: A wrecked machine leaves a way on (gap 18, rule 51)
- **Goal.** For every machine that can be wrecked, wreckage leaves a physical way onward, proved by a test like `run_plate_wreckage`. Where it does not, fix it.
- **Owns:** the new wreck tests, and the wreck behavior functions in each machine file. Route geometry in `band_plate_shop.cpp` and `band_facade_crane.cpp` is Builder's.
- **Rulings (5:25 PM):** an empty S2 or S3 cage returns by itself, as S1's does. A spent F platform returns to where a way on exists; if physics can't do that, Builder adds a backup way as a protected PR.
- **Protected:** yes (new checks; any geometry fix). (Tests are #104.)

## B1: Her wrecking, stock first (gap 9; Cory's pick)
- **Goal.** She undoes his work with stock, physically: she notices stock he placed or used, carries it off or throws it, and shoves it off an edge on purpose, all through her real drive within 8 kN / 44 kW, and he can undo it.
- **Now:** `notice_moved_stock`, `fetch_stock`, `take_stock` and `throw_stock` exist (`src/sim/matron.cpp`). Fetching is Pest-stage only; `kTakeMaxKg` is 25 kg; the throwables are three pieces on deck 154; she has no deliberate edge shove.
- **Why it is hard.** The shove goes through her own walk drive with no snap, at an edge that matters to his route, without breaking her hunt, hold or Turn states, on every deck, deterministically.
- **Owns:** those four functions, the new shove, `lay_matron_throwables`, `kMatronThrowables` and `kTakeMaxKg`.
- **Finish facts:** tests show her shoving a crate he placed off an edge, and carrying or throwing stock, within budget and with no snap; he can put it back; the Matron native groups and `touch_matron_act` are green; screenshots by eye.
- **Protected:** the new checks; any change to `kTakeMaxKg` or her limits is its own PR.

## B2: The route above 221 m (gap 1; rules 52 and 53)
- **Goal.** Repeated board-and-ladder segments and pull-started lifts above 221 m become stages that are each a new physical action; a few ladders and easy backups stay.
- **First deliverable: a plan, not code.** One post listing each segment in the plate-shop band (340.25–484 m, `band_plate_shop.cpp` `build_climbing_route`) with its proposed action. The integrator checks it against rules 52 and 53 and brings any big game call to Cory. Then build that band, then `band_facade_crane.cpp`.
- **Protected:** yes (world geometry).

## B3: Her stock reach with real arms (after the hold PRs land)
- **Goal.** Her drawn hands meet the stock she carries, as the hold's real-arm fix did for his ankle. Today the reach stretches her arm up to `PROP_STRETCH_MAX` 2.7, with a 3.22x two-arm stretch in `touch_matron_act` near t=34.97 s.
- **Owns:** the two-handed stock reach in `matron_rig.gd`.
- **Finish facts:** the arm stretch at or under 1.05 on stock frames, measured by the drawn-body checks (#100); screenshots by eye.
