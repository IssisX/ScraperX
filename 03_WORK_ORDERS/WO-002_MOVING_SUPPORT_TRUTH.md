# Work Order 002 — Moving-Support Truth

## Objective

Make moving support physically truthful in the native player controller:

`native translating/rotating support → actual contact-point velocity → support-relative player motion → inherited momentum on detach/jump → Godot render mirror`

This work order ends at moving-support truth. It does not add mantle, vault, ledge, climb, freight machinery, structural failure, or process simulation.

## Existing proven truth

At source commit `4ed23093078eeff6d073d523219e60824ea7826a`:

- the Godot 4.7 host loads the current-source C++ GDExtension;
- `scraperx_sim` advances at the authoritative 90 Hz fixed step;
- the player is a native Jolt-backed capsule owned by `scraperx_sim`;
- native contact establishes static support entity `1`;
- bounded desired-velocity locomotion works on the static deck;
- host tests, Godot runtime capture, Android arm64 native compilation, and APK export were proven by WO-001 CI;
- APK installation, Android execution, Fold 6 observation, and moving support were not yet proven.

## Governing authority

- Governing Law 4 — Athletic industrial parkour is foundational.
- Governing Law 5 — Moving supports preserve motion.
- Governing Laws 22–23 — one owner per consequential fact; authority boundaries precede integration.
- Governing Law 26 — no physics theater.
- Governing Laws 28–29 — Fold-class Android is shipping truth; delivery claims remain distinct.
- TDD §4 — authoritative fixed-step ordering.
- TDD §8 — native player/parkour authority and support-point law.
- TDD §24 — WO-002 moving-support truth precedes athletic traversal.

## Owner

`scraperx_sim` owns player pose, velocity, grounded/support state, support contact point, and inherited momentum.

Native Jolt owns rigid-body/collision/contact/kinematic substrate.

Godot owns input and presentation only. It may mirror native support poses and telemetry; it may not parent the player to a platform or independently add platform velocity.

## Allowed seam

Extend the existing native `PhysicsWorld`, immutable `Snapshot`, thin GDExtension bridge, current Godot presentation mirror, and current host/runtime/Android verification workflow.

Use stable native support entity IDs. Moving supports must be actual Jolt bodies advanced through the same authoritative tick as the player.

## Forbidden shortcuts

- Godot node parenting as the motion mechanism.
- Animation-owned platform transforms.
- Hard-coded visual velocity added only in presentation.
- A grounded/support flag without real native contact.
- A jump script that teleports position or overwrites horizontal velocity with a canned value.
- World-relative locomotion that silently cancels support motion.
- Fake rotating support that reports angular velocity without producing `ω × r` point velocity.
- Starting WO-003 climbing/traversal primitives before this discriminator passes.

## Proof path

1. Host tests retain WO-001 fixed-step, invalid-input, static-support, and static-locomotion coverage.
2. Host tests settle the player on a translating native support and prove zero-input world velocity follows the support point.
3. Host tests jump from that support and prove material inherited horizontal momentum remains after detachment.
4. Host tests settle the player off-axis on a rotating native support and verify measured contact-point velocity against `v_point = v_linear + ω × r`.
5. Godot 4.7 loads the same native extension, mirrors authoritative moving-support poses, performs a CI jump from support entity `3`, and logs inherited momentum.
6. The same source cross-compiles for Android arm64 and exports an APK containing the native library.

## Completion

WO-002 is complete only when all proof steps above pass from one source commit.

That proves: **implemented + host-built/tested + desktop Godot-executed/observed + Android arm64 built + APK produced**.

It does not prove: **APK installed, Android executed, observed on Fold 6, sustained 45 FPS, thermals, or touch ergonomics**. Those claims remain separate.
