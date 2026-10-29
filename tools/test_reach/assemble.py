import pickle,os,re,glob,subprocess,datetime
from common import R,S
g,f,fn,un,order,size,n,coref,allf,cutf,dirf=pickle.load(open(S+'/native_out.pkl','rb'))
gd=pickle.load(open(S+'/gd_out.pkl','rb'))
subj=pickle.load(open(S+'/subj.pkl','rb'))
SUBJ={'src/sim/matron_hold.cpp':'hold','src/sim/matron_fatality.cpp':'hold','src/sim/physics_world_grab.cpp':'hold','src/sim/physics_world_arm.cpp':'hold','src/sim/physics_world_fatality.cpp':'hold','src/sim/physics_world_one_arm.cpp':'hold',
 'src/sim/matron_climb.cpp':'climbing','src/sim/matron_traversal.cpp':'climbing','src/sim/matron_routecraft.cpp':'climbing','src/sim/matron_found_climb.cpp':'climbing','src/sim/matron_journey.cpp':'climbing','src/sim/matron_travel.cpp':'climbing','src/sim/matron_contact.cpp':'climbing',
 'src/sim/matron.cpp':'senses and hunt, balance, pace, cable','src/sim/matron_moves.cpp':'pace and senses','src/sim/matron_pipe.cpp':'corner pipe','src/sim/matron_line.cpp':'cable and catwalk','src/sim/physics_world_strike.cpp':'balance and blows'}
for _f,_t in SUBJ.items():
    fn=fn.replace('### `%s`'%_f,'### `%s`\n\n> State-gated code: T1 and O are weak here. Take the group from the **%s** row of section 4, then confirm it with this row.\n'%(_f,_t),1)
_head=subprocess.check_output(['git','-C',R,'rev-parse','--short','HEAD'],text=True).strip()
_branch=subprocess.check_output(['git','-C',R,'rev-parse','--abbrev-ref','HEAD'],text=True).strip()
built=f"{datetime.date.today().isoformat()} on {_branch} at commit {_head}"
ncpp=len(glob.glob(R+'/src/sim/**/*.cpp',recursive=True))
nscen=gd['n']
md=f'''# Test-reach map: which tests a change can reach

Look up the file (or function, with its line number) you changed; the row names the native groups and touch scenarios that run it. This serves `CLAUDE.md` "Test only what a change reaches": run those, cheapest first, once, and say in the report which and why. It is a lookup, not a verdict: read "How far to trust it" first.

Built {built}. Regenerate when the test lists, the kit's body order or the call structure change a lot (the method is below); a stale map is worse than none.

## How to use it (30 seconds)

1. Changed a `src/sim/*.cpp` or `.hpp`? Find the file in section 2. Changed a function in a big file? Section 3 has it with its line range. Changed Matron behavior? Start from the subject index (section 4).
2. Take the tiers left to right: **D** (a test calls this code straight from its own body), **T1+O**, **O**, **T1** (weaker, see below). Run the first group of the strongest tier present; add a second only if the change spans subjects. Groups inside a tier are listed narrowest first (fewest `src/sim` functions reached without the per-tick step), so the first is the cheapest.
3. Run it as `SCRAPERX_ONLY=<group> ./build/host/scraperx_sim_tests` (the 13 named in `CMakeLists.txt` are also ctest names, `ctest -R scraperx_sim.<group>`). Under 3 groups should be enough for one change.
4. Changed `godot/presentation/**/*.gd`? Section 7 (files) and section 8 (functions in big files) name the touch scenarios (`--uitest=<scenario>`); a `touch_*` scenario also runs as `<name>_buttons` on the BUTTONS scheme.
5. Row says `tick only`, `no scenario or test reaches it` or `**no test reaches**`: nothing gives direct evidence; say so in the report instead of picking a random group.

## How far to trust it

Built from the code, not from names: every `src/sim`, `src/bridge` and `tests/*.cpp` file was parsed with libclang (the build's include paths and defines) into a call graph; each group's seed functions are the `run_*`/`test_*` calls in its block of `tests/simulation_tests.cpp` main(); the closure of those through the graph is what the group "reaches". GDScript was parsed by a small script: each scenario's `match` arm in `ui_test_driver.gd` is the seed, calls are resolved by function name (and by receiver name for `obj.f()` calls), and `_native.f()` calls are resolved to the bridge method bound under that name (`src/bridge/scraperx_simulation.cpp`) and from there to `src/sim`.

What it cannot see, and what I did about it:

- **Every group ticks the world.** `Simulation::advance_frame` reaches {len(coref)} of the `src` files from any group, so plain reachability says "all tests reach everything". The per-tick step is therefore cut out: **T1** is reach without passing through it (setup calls, debug hooks, requests, accessors). Code that only runs in the tick when the right state holds is not separated by T1.
- **O (observes).** A group counts as observing a function if it reaches the function (tick included) and its test code reads a struct field (`MatronSnapshot.grabs`, `Snapshot.zipline_riding`, ...) that the function also touches. Fields read by more than 35% of groups are ignored as noise. This is evidence a test watches the function's effect, not proof it runs the branch you changed.
- **D** is the strongest: some test function calls the function directly. It is rarely complete (a group that reaches the code through the tick is not a D).
- **Virtual calls** are followed to every override; Jolt callbacks (contact listener and layer filters) are tied to every `PhysicsSystem::Update`; `make_unique`/`emplace` construct edges go to the class's constructors; a destructor counts with its constructor. Function pointers stored in tables, `std::function`, and GDScript signals/`call_deferred` by string are followed only when the name is a literal in a `connect`/`call_deferred`/`Callable` call; anything else is missed (GD side: **may run via signals/scene wiring**).
- **Nothing was run.** This map is static, per the job. The ctest names and run command are from `CMakeLists.txt`.

## 1. Native test groups

{n} entries: the {n-3} `SCRAPERX_ONLY` groups of `tests/simulation_tests.cpp`, the full run (`scraperx_sim.athletic_traversal`: every `run_*` in main(); the groups `matron-chase` and `matron-grab` run only in their own slice, as the comments in main() say), and the two other test executables.

"Src functions reached" is the group's size without the per-tick step: the smaller, the cheaper and more focused. The last column lists the `src/sim` and bridge `.cpp` files that no more than 30% of groups reach: what makes the group different.

{g}

Not groups but also tests: `tests/twin_geometry_test.py <pre-relocation commit>` (compares `src/sim/world_solids.inc` boxes against a base commit; no `src/sim/*.cpp` is run); CI runs `ctest` plus a bare `./build/host/scraperx_sim_tests` (every group in the default order).

## 2. Every `src/sim` file and the native groups that reach it

`Run these` lists tiers D, T1+O, O, T1 for the union of the file's functions (a group appears in its strongest tier only; `~all` means 80% or more of groups). Columns 4 and 5 are counts of groups that reach the file without / only through the per-tick step. `core: ~every group` means a function in the file is reached by 80% or more of groups without the tick (usually constructors, accessors or world building): use section 3 for the function you changed. `src/bridge` is not in any native group (the native tests do not link the bridge); section 9 covers it.

{f}

Header-only files have inline functions only; `matron_mover.hpp` is a pure interface (no function bodies), so no test "reaches" it by itself: its `Mover::step` is reached through the ways that implement it (`TravelWay`, `PipeWay`, the climb and line ways).

## 3. Big files, function by function

Every function of 25 or more lines (40 in headers) in a file of 500 or more lines. `Reached w/o tick / only via tick` counts groups. A function with no reach has the note `**no test reaches**`: the native tests never run it (compare section 9 for the bridge).

{fn}

## 4. Matron subject index (what her tests watch)

Her code is one state machine, so reach alone cannot tell the holds tests from the climbing tests. This table groups her `MatronSnapshot` fields (and the hold/fatality test result structs) into subjects, and lists the groups whose test code reads any field of the subject beyond the ubiquitous ones, most fields first. The source-file column is my reading of each file's header comment, not computed. For a change in her hold code, run the first group here that also shows up in the function's row in section 3.

{subj}

## 5. Touch scenarios

`godot/presentation/ui/ui_test_driver.gd` has {nscen} scenarios (`SCENARIOS`), each a `match _scenario` arm calling a driver function in `ui_test_base.gd`, `_moves`, `_rides`, `_chase`, `_act`, `_bands`, `_service`, `_stack` or `_uncanny` (all one script by `extends`). Run one as `godot --headless --fixed-fps 60 --path godot -- --uitest=<scenario>` (CI runs the set in `wo000-delivery-spine.yml`; `look_*` and `perf_*` render only and are not in CI). The `touch_*` ones also run as `<name>_buttons`; `pad_*` and `keyboard_*` inject device events.

Columns: spawn is the id passed to `configure_initial_spawn`; "game functions" are `presentation` functions the scenario's own code calls (file:function, first five); "bridge methods" are native methods it calls (first five); the last column lists the `src/sim` files its bridge calls run without the per-tick step, ignoring files that more than 35% of scenarios reach.

{gd['scen_table']}

## 6. Which `src/sim` files the touch scenarios run

**N**: the scenario calls a bridge method whose body runs the file (cut at the per-tick step). **O**: the scenario reads a bridge value the file's code writes, so the file runs in its ticks. Every scenario that advances the world also ticks everything; files with neither column set are reached by ticks and world building alone (`tick only`).

{gd['sim_table']}

## 7. Presentation files

`Per-frame core` counts the file's functions reachable from engine callbacks (`_ready`, `_process`, `_input`, ...) of the game scripts: they run in every scenario. `Scenarios that call into it` are the scenarios whose own code (or the chain of game functions it starts) calls a function in the file that is not in the per-frame core. `no scenario or test reaches it` means no static call path from a scenario or an engine callback; signals and scene wiring are invisible to this, so check `grep -rn <file stem> godot/` before treating it as dead.

{gd['gd_table']}

## 8. Big presentation files, function by function

{gd['gd_fn']}

## 9. Headless Godot tests (`godot/tests/*.gd`)

Run with `tools/godot/godot --headless --path godot --script res://tests/<name>.gd`. They load the scripts named; the scripts' own functions are what they run.

{gd['head_table']}

## 10. Not reached by any test

- Native (`src/sim`): `PhysicsWorld::slingshot_prediction` (`physics_world.cpp` 939-974), `Slingshot::reset` (`slingshot.cpp` 969-1005), `Slingshot::prediction` (`slingshot.cpp` 1007-1071): no native group calls them. The bridge binds `get_slingshot_prediction`, which `touch_slingshot` can reach (section 6).
- `src/sim/matron_mover.hpp`: interface only, no code.
- Bridge (`src/bridge/scraperx_simulation.cpp`): the native groups do not link it; every `get_*_state` and `_bind_methods` is reached only by the touch scenarios and the game.
- Presentation: {", ".join("`"+x.replace("godot/presentation/","")+"`" for x in gd["unreach"])}. See section 7 for what that means.

## Method (to regenerate)

`tools/test_reach/` (Python with libclang, `pip install libclang`) parsed every `src/**/*.cpp` and `tests/*.cpp` with the flags of the build's `compile_commands.json` (or, without one, the flags of `build/host`) (`-std=c++17`, Jolt include path and `JPH_*` defines, `-DSCRAPERX_HAS_JOLT=1`), keyed functions by canonical declaration location, and recorded call edges (`CALL_EXPR` references), override links, constructor edges for `make_unique`/`emplace*`, and struct-field references. A second script did the GDScript. The scripts are in `tools/test_reach/` (README.md there); they need libclang and a configured build for the Jolt and godot-cpp headers.
'''
os.makedirs(R+'/03_EXECUTION',exist_ok=True)
open(R+'/03_EXECUTION/TEST_REACH.md','w').write(md)
print(len(md))
