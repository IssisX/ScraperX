#pragma once

// J-092 (#410): the two-ton slab trap. An overhead service cell under the 176
// ring's north band, over her lane on the 154 m deck between her locker and
// the ladder to the ring, outside the bay: a 2,000 kg slab of plate
// (LooseStock::PlateSlab, the yard's own kind) hung by its eye on a sling from
// a hook under the ring's steel, in plain view from the deck.
//
// The hook is a pelican hook (Kit::add_slip): it holds while its trigger, a
// short weighted lever beside it, lies on its stop. A pull cord runs from the
// trigger's tail up over a sheave under the ring, along under the band to a
// second sheave west of the slab, and hangs down to a yellow handle at hand
// height on the deck (Kit::add_trip_line). He takes the handle and steps back
// with it: the cord lifts the trigger's tail, the hook opens and the sling
// runs out for good. Gravity alone gives the slab its speed from there; it
// meets whatever stands under it, or the deck. Nothing pushes it, steers it
// or knows where she is: what it does to her is her balance's to judge
// (Matron::take_blow), and a slab that meets only the deck adds nothing.
//
// Its numbers are the probe's (tests/sim_tests_matron_slab_drop.cpp, #410):
// its top 1 m under the ring's steel (175.75), 18.6 m over the top of her
// body standing, about 2 s of fall.
//
// What holds it is in plain view: a short hoist beam bolted under the band
// along z, the hook block hung from the beam with the trigger's cheeks, and a
// sheave block for the cord over the trigger's tail and another over the
// handle. One static kit body; the sling is made fast to it at the hook's
// eye.
//
// Its kit bodies are built after every other the world builds (the slab
// next in the loose-stock table after the Matron's kinds, then the frame,
// the trigger and the handle), so saves made before it line up.

#include "sim/mechanism_kit.hpp"

#include <Jolt/Jolt.h>

#include <cstdint>

namespace scraperx::sim {

struct SlabCell final {
    static constexpr std::uint64_t kTriggerEntity = 2670;
    static constexpr std::uint64_t kHandleEntity = 2671;
    // The frame (beam, hook block, sheave blocks): static, so a static id.
    static constexpr std::uint64_t kFrameEntity = 1672;

    static constexpr float kDeckTop = 154.0F;
    static constexpr float kRingUnderside = 175.75F;   // the 176 ring's north band (world_solids.inc, UpperDeck)
    // The slab's middle over the deck: under the band 2.5 m in from its
    // inner edge (-128.91), long side along z, 0.7 m east of the ladder's
    // rails (x 5) and 3.2 m west of her locker's face. On the outer side of
    // the deck's walkway (J-092, 11-11: 1.5 m south of where it was first
    // hung), so the cord's handle under it hangs clear of the way he and she
    // cross between the bay and her locker.
    static constexpr float kSlabX = 6.6F;
    static constexpr float kSlabZ = -126.4F;
    static constexpr float kSlabUnderside = kRingUnderside - 1.0F - 0.3F;   // its top 1 m under the steel
    // The hook's eye, where the sling is made fast.
    static constexpr float kHookY = kRingUnderside - 0.45F;
    // The trigger: hinged on the hook's west cheek, its tail out west, level
    // on its stop by its own weight; lifted kReleaseRad, the hook opens.
    static constexpr float kTriggerPivotX = kSlabX - 0.15F;
    static constexpr float kTriggerPivotY = kHookY + 0.05F;
    static constexpr float kTriggerTail = 0.45F;
    static constexpr float kTriggerKg = 15.0F;
    // 6.7 cm of cord: a brush against the handle (it swings on 20 m of
    // cord, so a metre aside draws 2.5 cm) leaves it shut.
    static constexpr float kReleaseRad = 0.15F;
    static constexpr float kTriggerTravelRad = 0.6F;
    // The cord's sheaves under the band: over the trigger's tail, and west
    // of the slab over the handle. Holding the handle he stands 3.2 m west
    // of the slab's middle: looked away from, she closes to 3 m of him and
    // holds there (matron_stalk.cpp, kStalkNearM), which is under the slab
    // when she comes along the band; under his gaze she freezes. The
    // handle hangs 1 m in from the deck's outer rail, off the walkway: at
    // z -127.0 it hung where the ways from the bay's east end to her locker
    // and to the door cross (3.9, -127.2), and a walk or a carry there
    // brushed it.
    static constexpr float kSheaveY = kRingUnderside - 0.1F;
    static constexpr float kHandleX = 4.0F;
    static constexpr float kHandleZ = -125.5F;
    // The handle's top, where his hands hold what they carry (1.24 m over
    // the deck): taking it draws no cord, stepping back with it does.
    static constexpr float kHandleTopY = kDeckTop + 1.28F;
    static constexpr float kHandleHalfY = 0.04F;
    static constexpr float kHandleKg = 3.0F;

    kit::BodyIndex slab;
    kit::BodyIndex frame;
    kit::BodyIndex trigger_body;
    kit::BodyIndex handle;
    kit::RopeIndex sling;
    kit::LeverIndex trigger;
    kit::SlipIndex hook;

    // Builds the frame, hangs `slab_body` (built where
    // LooseStock::lay_hung_slab put it) from it on the sling, and builds the
    // trigger, the cord and its handle.
    void build(kit::Kit &kit, kit::BodyIndex slab_body);
};

}  // namespace scraperx::sim
