# ScraperX

ScraperX is an Android first person machine climbing game. This branch has been
reset to one playable ground ascent: an Archimedes screw pumps water into an
upper tank, the player opens a valve to fill a counterweight bucket, then rides
a cage to the fixed deck at **+8 m**. There are no stairs or ramps to that deck.

The former mechanisms, temporary props, and broad simulation test suite were
removed. Their source remains in Git history. The current scene and native
model own just this first ascent.

## Play the ground ascent

1. Approach the amber pump control and press **E** or **OPERATE** to start the screw.
2. Let the upper tank fill. Press **E** again to stop the motor; its rotor coasts.
3. Press **V** or **VALVE** to transfer the tank water into the lift bucket.
4. Walk onto the cage and press **E** or **RELEASE**. The bucket descends while
   the cage carries you upward. Step east onto the fixed landing.
5. After the bucket drains, reset the empty cage from the upper control or the
   grade pump control. **R** reverses a stopped screw.

On desktop, WASD moves, the mouse looks, Shift runs, and Space jumps. The
Android UI provides a movement pad, look drag, jump, and nearby machine actions.

## Source and proof

- `src/sim/water_screw.*`: finite torque, inertia, head, immersion, displacement,
  leakage, capacity, and hydraulic work.
- `src/sim/ground_stage.*`: conserved water transfer, 2:1 bucket and cage
  constraint, rider mass, friction, governor, catches, drain, and reset.
- `godot/presentation/`: the rendered machine, moving cage collider, physical
  player, controls, and upper dock.
- `tests/ground_screw_tests.cpp`: focused causal and failure tests.
- `godot/tests/ascent_smoke.gd`: runs the real scene with a real player body and
  requires contact with both the moving cage and fixed upper landing.

See [the ground ascent design](docs/ground-ascent.md) for the backward design,
physics assumptions, evidence boundaries, and recovery path.

Host native test:

```sh
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release -DSCRAPERX_BUILD_BRIDGE=OFF
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

The `ChatGPT` branch workflow builds the native bridge, runs the scene level
ride in Godot 4.7, then exports an Android arm64 APK. A successful export
proves the package was built; a separate device run is still needed to measure
phone specific performance and touch feel.
