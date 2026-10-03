# Rope ladder: west service-mast placement screen

Profile: MACRO-TRAVERSAL-STRICT [DEFAULT]. Source screened: ChatGPT `0ecf34196c59d9e521f11e529e3da65644ec29a9`. Private proposal, not compiled assembly or implemented route.

Outcome: connect the proven +121 m ring to the +132 m ring by a freely hanging flexible ladder with a long tail, visible top support and recoverable entry/departure. Native C++/Jolt owns gravity, wind, rope tension, contact and paid hand coupling; Godot draws those states.

MEASURED source geometry: Tower centre X=0/Z=-150, outer half extent26 m, ring spacing11 m, west deck X=[-26,-17], outer edge beam at X=-26; west column at Z=-150. AS-026 exits at X≈9/Z≈-174.53/Y=121.9 on support11. The player may walk the existing north/west ring to the new approach; that walk has not been exercised.

CHOSEN screening geometry: top attachments centred X=-29.5/Y=135.6/Z=-158, ladder width1 m along Z, length24 m downward, nominal tail Y=111.6. Attachments project visibly from a supported service frame. Screen entry at ring top121 and receiving tongue at132, both leaving an air gap to the rope plane; a lower catch deck at110 receives misses. These new decks/frame do not exist yet. Entry must not intersect the hanging tail or pin the rungs. Receiving footing must permit physical release away from the ladder toward the tower rather than a relocation/top-out shortcut.

DERIVED screening relations: the useful ring rise is132-121=11 m, distinct from24 m hanging length. Nominal tail clearance over proposed110 m catch deck is1.6 m before material extension, rung dimensions or sway. At85 kg, lifting11 m requires at least85*9.81*11=9172.35 J of player work before losses; gravity and a light breeze cannot supply free climbing energy. Actual hand work must be derived from authoritative bounded coupling and targets.

MEASURED static screen: `screen.py` checks all1316 generated normal box records and666 hull records conservatively by world AABB against CHOSEN volume X=[-30.5,-28.5],Y=[110.6,136.6],Z=[-159.5,-156.5]. No intersection in these records; `box-screen.json` hashes the exact generated source. This is not a measured swept envelope or complete collision clearance: native Kit assemblies, real rope/player motion and the new support/decks remain to be evaluated. The chosen1 m horizontal allowance is not a physical travel stop or a claimed sufficient bound.

Authoritative integration seams: `find_grip` uses narrowphase candidates, a400 kg dynamic filter and `leaf_box`; cylindrical lightweight rungs require their own explicit reachable rung-axis query. Route catches dispatch before the legacy gravity-off climbing controller. Hand attachments need distinct body IDs/local points, finite force/work, gravity on and no legacy weight duplication. Real body/rung momentum persists on release. Kit owns body/collision/render pose/checkpoint capture; a rope subsystem owns unilateral strand constitutive state, wind phase and hand constraints, reconciling its topology on checkpoint restore.

UNRESOLVED: admissible elastic strand law and material parameters; initialization/equilibrium and numerical floor; real swept volumes; exact platform edges and entry/departure actions; support capacity; stable entity/AS allocation; wind/checkpoint state; render/audio/haptic integration and device cost. The hard inextensible60-rung prototype currently fails the quiet-equilibrium refinement discriminator. Do not implement this geometry as if that physics were compiled.

Next deciding work: close the private prototype's source/ledger counterexample, then evaluate one materially honest tensile strand model with actual initialization and matched90Hz/h/4 before production assembly/controller integration. No new IDs or source files are reserved by this screen.
