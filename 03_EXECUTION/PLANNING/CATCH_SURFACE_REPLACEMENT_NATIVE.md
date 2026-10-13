# Catch surface replacement: native implementation

Source base: `ChatGPT` / `d37b793f5e6c12a0338b59552acda68350b3a338`; integrated candidate updated 2026-10-13. Native owners are `src/sim/cargo_net.hpp`, `src/sim/cargo_net.cpp`, `src/sim/catch_net_route.cpp` and the simulation/bridge owners. Lead owns integration and delivery. This note separates observed host execution from pending normal GitHub/Android validation.

## Current physical implementation

The catch entity opts into `CargoNetSurface::ContinuousMembrane`: a 41x41 grid at 0.3m spacing spans the original 12x12m extent, with 1681 real vertices, 3200 triangles and 9600 indices. Each node carries 150/1681kg, preserving 150kg total. Gravity and native Jolt contact response produce deformation and rebound. The original vertical cargo net 1953 remains open weave.

The existing finite dynamic frame and foundation mounts carry 160 perimeter attachments, each with a visible clip back. Per-attachment stiffness is 12000N/m and damping 60N·s/m, retaining aggregate 1,920,000N/m and 9600N·s/m. Existing clip integration and reciprocal frame reaction remain authoritative. Jolt distance constraints retain stretch 5e-6, shear 5e-5 and bend 2e-3 compliance in m/N; these are reduced-order edge properties, not measured continuum material properties. Axial damping remains 0.05 critical ratio, body drag 1.2, 12 solver iterations and global 90Hz.

The native ground tether attaches at membrane centre `(20,20)` and actual finite-frame `anchorBodyLocal=(-34,0.4,-150)`. Rest length is 2.5m, stiffness 600N/m and damping 300N·s/m. The primitive uses the actual three-point membrane weights and finite-frame point inertia, including reciprocal torque. Slack force is zero. The orange 20mm cord represents an abstract massless tension-only connection; it does not establish discretized rope collision or elastomer behavior. A reinforced attachment patch is an owner-requested follow-up, not implemented behavior.

## Sampling, collision and lifecycle

`CargoNetSurface::OpenWeave` remains the default with its four-corner layout and bilinear averaging. Continuous membrane samples use three unique vertices and triangle barycentric weights on the native 00-to-11 diagonal. Position, interpolated position, velocity, inverse mass, impulse transpose and owner force share this map. Inverse mass sums squared weights times actual vertex inverse masses. Explicit active sample counts are 3 for membrane and 16 for weave. Restore cancels transient pending forces; existing callbacks, sleep, readback and hand/foot ownership remain in use.

All affected native clients must be rebuilt after the `CargoNetConfig`/`CargoNet` layout change. Snapshots expose actual rows/columns and geometry. The bridge also exposes derived tether render endpoints through the new getter; rendering does not reconstruct the force law or anchor pose.

The initial 625-node/0.5m candidate failed a second 36m catch as deformed vertices reached capsule-cylinder contacts. The current 0.3m grid follows the bottom-cap coverage bound in the [contact diagnosis and evidence](../../evidence/canvas-contact-resolution-2026-10-13/README.md). Jolt discrete soft contact remains vertex-based: visible filled triangles alone do not guarantee persistent triangle collision. No hidden collision support or scripted elevation supplies success.

## Observed verification and boundaries

Lead-reported execution of rebuilt current native source passed:

- Unchanged 45m loaded-model test: late peak 0.139552m, late range 0.0233545m, first apex 23.8823m and minimum contact 2.54769m.
- Native force checks on both triangles and off-centre unequal-mass samples, including slack/taut actual linear momentum, angular momentum and energy checks.
- Final 36m player fall, ordinary exit, cell impact, frame Jump, miss, rim, overload, bottom-out, spill and vertical-side cases.
- Underside contact using an ordinary finite anchor Jump: actual head contact supplies no footing, Jump or checkpoint. Menu restart also passed.
- Real current Godot bridge host script: 66,749 checks across five poses, actual-input grade exit and onward cargo approach, using day/LOW with softpipe.

Evidence and captures are in `evidence/canvas-contact-resolution-2026-10-13/` and the private host workspace `/data/data/com.termux/files/usr/tmp/scraperx-membrane-candidate`. The first host llvmpipe renderer crashed with SIGILL; softpipe resolved that host rendering failure. Host execution used a private ARM GDExtension mapping; the original Android mapping is unchanged.

Earlier focused Clang/GNU syntax checks and the isolated 625-node presentation fixture are historical evidence, not execution proof of this final topology. Normal GitHub/Android GREEN remains pending. No phone performance, loaded mobile cost or full-system energy acceptance is claimed.
