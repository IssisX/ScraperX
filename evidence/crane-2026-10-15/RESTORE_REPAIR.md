# Crane checkpoint restore repair — 2026-10-15

`YardCrane::restore` now initializes the replacement hoist from the saved rest
(stock) length before restoring its saved state. Previously it initialized from
endpoint distance, incorrectly applying the constructor stock range (4–34 m) to
valid slack or finite cable extension. `PoweredCable` validation, force/work laws,
body geometry and rest-length limits are unchanged.

The narrow `--restore-slack` regression authors a physical body snapshot with 2 m
endpoint separation after nine actual hoist ticks have spent finite inventory.
The saved rest length remains in range. It asserts restoration succeeds, preserves
stock/energy/work and actual slack geometry, keeps native body identities, pose,
velocity and constraint count, and clears transient command/force as prescribed
by the existing cable restore contract. This is native checkpoint-lifecycle
regression evidence, not a claim that this authored pose was reached in gameplay.

The unchanged frozen v4 owner fails with `RESTORE_EXCEPTION PoweredCable invalid
config`; the repaired owner passes. Both test and production compilation used
C++17, optimization and warnings-as-errors. Host proot emitted its existing
`/proc/self/fd` binding warnings; compilers and linker returned zero.

Prior loaded-cycle trial12 and individual-leg trial13 remain historical accepted
evidence and were not rerun. No new full-world, bridge, Godot, APK or device claim
is made here. Source-v4 headers, geometry and archive provide a frozen dependency
closure, avoiding concurrent root geometry changes.

Workspace: `/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo`
Branch: `ChatGPT`; HEAD: `13aa36f67c1f5cb95ed5d974edd8cd15b58b3ebd`.
Only this note, `src/sim/yard_crane.cpp` and `tests/yard_crane_tests.cpp` are owned
by this repair; existing concurrent changes were preserved.

Commands were orchestrated sequentially (one compiler at a time) by:

```sh
python3 /data/data/com.termux/files/usr/tmp/crane-restore-5mHcK5/run.py red
python3 /data/data/com.termux/files/usr/tmp/crane-restore-5mHcK5/run.py green
```

Exact subprocess commands and actual exits:

`compile-test`, exit 0:

```sh
proot-distro login ubuntu -- /usr/bin/c++ -O3 -DNDEBUG -std=c++17 -pthread -Wall -Wextra -Wpedantic -Werror -DSCRAPERX_HAS_JOLT=1 -DJPH_DEBUG_RENDERER -DJPH_OBJECT_STREAM -DJPH_PROFILE_ENABLED -I/data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015/source-v4/src -isystem /data/data/com.termux/files/usr/tmp/scraperx-ci-soft-velocity-20261015/jolt-src -c /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo/tests/yard_crane_tests.cpp -o /data/data/com.termux/files/usr/tmp/crane-restore-5mHcK5/test.o
```

`link-red`, exit 0:

```sh
proot-distro login ubuntu -- /usr/bin/c++ -O3 -DNDEBUG -std=c++17 -pthread -Wall -Wextra -Wpedantic -Werror -DSCRAPERX_HAS_JOLT=1 -DJPH_DEBUG_RENDERER -DJPH_OBJECT_STREAM -DJPH_PROFILE_ENABLED -I/data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015/source-v4/src -isystem /data/data/com.termux/files/usr/tmp/scraperx-ci-soft-velocity-20261015/jolt-src /data/data/com.termux/files/usr/tmp/crane-restore-5mHcK5/test.o /data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015/build-v4/libscraperx_sim.a /data/data/com.termux/files/usr/tmp/scraperx-ci-soft-velocity-20261015/jolt-build/libJolt.a -o /data/data/com.termux/files/usr/tmp/crane-restore-5mHcK5/red
```

`red`, exit 1:

```sh
proot-distro login ubuntu -- /data/data/com.termux/files/usr/tmp/crane-restore-5mHcK5/red --restore-slack
```

`compile-owner`, exit 0:

```sh
proot-distro login ubuntu -- /usr/bin/c++ -O3 -DNDEBUG -std=c++17 -pthread -Wall -Wextra -Wpedantic -Werror -DSCRAPERX_HAS_JOLT=1 -DJPH_DEBUG_RENDERER -DJPH_OBJECT_STREAM -DJPH_PROFILE_ENABLED -I/data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015/source-v4/src -isystem /data/data/com.termux/files/usr/tmp/scraperx-ci-soft-velocity-20261015/jolt-src -fPIC -c /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo/src/sim/yard_crane.cpp -o /data/data/com.termux/files/usr/tmp/crane-restore-5mHcK5/owner.o
```

`link-green`, exit 0:

```sh
proot-distro login ubuntu -- /usr/bin/c++ -O3 -DNDEBUG -std=c++17 -pthread -Wall -Wextra -Wpedantic -Werror -DSCRAPERX_HAS_JOLT=1 -DJPH_DEBUG_RENDERER -DJPH_OBJECT_STREAM -DJPH_PROFILE_ENABLED -I/data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015/source-v4/src -isystem /data/data/com.termux/files/usr/tmp/scraperx-ci-soft-velocity-20261015/jolt-src /data/data/com.termux/files/usr/tmp/crane-restore-5mHcK5/test.o /data/data/com.termux/files/usr/tmp/crane-restore-5mHcK5/owner.o /data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015/build-v4/libscraperx_sim.a /data/data/com.termux/files/usr/tmp/scraperx-ci-soft-velocity-20261015/jolt-build/libJolt.a -o /data/data/com.termux/files/usr/tmp/crane-restore-5mHcK5/green
```

`green`, exit 0:

```sh
proot-distro login ubuntu -- /data/data/com.termux/files/usr/tmp/crane-restore-5mHcK5/green --restore-slack
```

Runtime output:

```text
proot warning: can't sanitize binding "/proc/self/fd/1": No such file or directory
proot warning: can't sanitize binding "/proc/self/fd/2": No such file or directory
RESTORE_EXCEPTION PoweredCable invalid config
FAIL yard crane: valid slack hoist snapshot restores without constructor rejection
proot warning: can't sanitize binding "/proc/self/fd/1": No such file or directory
proot warning: can't sanitize binding "/proc/self/fd/2": No such file or directory
PASS yard crane valid slack hoist restoration preserves stock, finite inventory and bodies
```

SHA-256:

- `/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo/src/sim/yard_crane.cpp`: `9f562bd2b7aa5f7c9b613bbdb2219baec502e4a3fec563c97f02466fcfd63a34`
- `/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo/tests/yard_crane_tests.cpp`: `ce71092e11d2f829d6cdca54f861e32fd3fe8f96ca1d873279de20921d9b5b25`
- `/data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015/build-v4/libscraperx_sim.a`: `6d7d4e0ef1fc5f9272183ae0845cee4a2d031894e99106ef7b4739a92c8edffa`
- `/data/data/com.termux/files/usr/tmp/scraperx-ci-soft-velocity-20261015/jolt-build/libJolt.a`: `8711a784f394fdf91af3366ab0b6695e80322cb73b46a8d1659e775b20aced1b`
- `/data/data/com.termux/files/usr/tmp/crane-restore-5mHcK5/green`: `b1a915a2634d2ac59caaf2d2d9d062e53e7ce8999d1cf71a3f9e02e668eaf120`
- `/data/data/com.termux/files/usr/tmp/crane-restore-5mHcK5/red`: `6d57322e232ee3f5418f004eb01f610bf33fb558b7a96aa049dc32871cce28df`
- `/data/data/com.termux/files/usr/tmp/crane-restore-5mHcK5/test.o`: `bc47c62308a50f3fbe69fc8a9da5f50c440ee81e94123ae06c1ac0d73a3e80ed`
- `/data/data/com.termux/files/usr/tmp/crane-restore-5mHcK5/owner.o`: `f89af49e4aae9330ea4962fd138583bc7bab975626bf4ce089b1d996836a7769`
