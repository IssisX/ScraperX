# Single normal-world teeter discriminator: no legacy vault

Uses the same privately frozen existing host archive as `README.md`, source `ChatGPT` / `3f295ca85bd5d9913cae94b689c50d20dc29e2c9`. Repository remained clean at that revision. Previous fixture evidence is preserved.

`normal-teeter.cpp` instantiates `Simulation(InitialSpawn::TeeterEntry)` in the ordinary world, without `RegressionFixtures`. It follows the existing supported approach from `tests/teeter_route_tests.cpp:86`: settle.5s then walk to `(27.2,-139)`, `(27.8,-138.58)`, `(30.5,-138.58)`, `(29.7,-139.45)` through actual movement inputs, braking to a supported arrival and waiting.2s at each. Face−X and wait.1s. The selected Z derives from the ballast's authored localZ−.45 at pivotZ−139 (`src/sim/teeter_rise.cpp:124`); measured ballast centreZ is−139.449966431.

At tick493 the real offer is ballast2801, with beam2800 support, position `(29.8154335022,67.0238876343,-139.450027466)` and near-zero world velocity. One Action at tick494 enters **Climbing(state4), gravity1, hands2**, with traversal support2801 and target `(28.9388332367,68.044883728,-139.450027466)`. It does not enter `Vaulting` or the gravity-off legacy owner. This confirms the existing finite-hand mantle dispatch for this particular normal contact; it does not establish normal-world legacy-vault success.

Per the bounded assignment, stop after that result: no stance sweep, second Action scenario, ordinary-Jump variant, full route run or repository edit. The optional Jump check was conditional on finding a vault at this pose, which did not occur. The complete approach and entry readout is `normal-teeter.csv`.

Actual command exited0; the only diagnostic was the same proot `/proc/self/fd` binding warnings:

```sh
proot-distro login ubuntu -- /bin/sh -c 'cd /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/physical-vault-baseline && /usr/bin/c++ -O3 -DNDEBUG -std=c++17 -pthread -Iinclude normal-teeter.cpp libscraperx_sim.a libJolt.a -lpthread -o normal-teeter && ./normal-teeter > normal-teeter.csv'
```
