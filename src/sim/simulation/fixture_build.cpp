#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

void Simulation::PhysicsWorld::build_intake_rise(JPH::BodyInterface &bodies) {
    const auto track = [this](const JPH::BodyID id) {
        machine_bodies_.push_back(id);
        return id;
    };
    const auto fixed = [&](const JPH::Vec3 half_extent, const JPH::RVec3 position,
                           const std::uint64_t entity_id,
                           const JPH::Quat rotation = JPH::Quat::sIdentity()) {
        return track(add_box(bodies, half_extent, position, JPH::EMotionType::Static,
                             object_layers::kStatic, 0.85F, entity_id, rotation));
    };

    // ---- the bay: back, sides, and the front wall carrying the throat --
    const float bay_half_z = (kIntakeBayFrontZ - kIntakeBayBackZ) * 0.5F;
    const float bay_center_z = (kIntakeBayFrontZ + kIntakeBayBackZ) * 0.5F;
    const float wall_half_y = kIntakeBayWallHeight * 0.5F;

    fixed(JPH::Vec3(kIntakeBayHalfX, wall_half_y, kIntakeBayWallHalfZ),
          JPH::RVec3(kIntakeBayCenterX, wall_half_y, kIntakeBayBackZ),
          Simulation::kIntakeBayEntityId);
    for (const float sx : {1.0F, -1.0F}) {
        fixed(JPH::Vec3(kIntakeBayWallHalfZ, wall_half_y, bay_half_z),
              JPH::RVec3(kIntakeBayCenterX + sx * kIntakeBayHalfX, wall_half_y, bay_center_z),
              Simulation::kIntakeBayEntityId);
    }

    // Front wall: two jambs plus a solid lintel over the throat. The
    // lintel is what stops the throat from being mantled over once the
    // dog is shut -- with it, a closed dog leaves no standable surface
    // anywhere in the opening.
    const float jamb_half_x = (kIntakeBayHalfX - kIntakeThroatHalfWidth) * 0.5F;
    JPH::BodyID west_jamb;
    for (const float sx : {1.0F, -1.0F}) {
        const JPH::BodyID jamb =
            fixed(JPH::Vec3(jamb_half_x, wall_half_y, kIntakeBayWallHalfZ),
                  JPH::RVec3(kIntakeBayCenterX + sx * (kIntakeThroatHalfWidth + jamb_half_x),
                             wall_half_y, kIntakeBayFrontZ),
                  Simulation::kIntakeBayEntityId);
        if (sx < 0.0F) {
            west_jamb = jamb;
        }
    }
    const float lintel_half_y = (kIntakeBayWallHeight - kIntakeThroatHeight) * 0.5F;
    fixed(JPH::Vec3(kIntakeThroatHalfWidth, lintel_half_y, kIntakeBayWallHalfZ),
          JPH::RVec3(kIntakeBayCenterX, kIntakeThroatHeight + lintel_half_y,
                     kIntakeBayFrontZ),
          Simulation::kIntakeBayEntityId);

    // ---- MOD-DOG-A ----------------------------------------------------
    // Hinged at the west jamb, extending east to fill the throat at angle
    // zero. Positive rotation about +Y carries its far end toward -Z, i.e.
    // into the bay, which is where the pack is standing.
    JPH::Body *dog = add_shape_body(
        bodies,
        new JPH::BoxShape(JPH::Vec3(kIntakeDogHalfWidth, kIntakeDogHalfHeight,
                                    kIntakeDogHalfThickness)),
        JPH::RVec3(kIntakeBayCenterX, kIntakeDogCenterY, kIntakeBayFrontZ),
        JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, object_layers::kMoving, 0.7F,
        Simulation::kIntakeDogEntityId, kIntakeDogMassKg);
    intake_dog_id_ = track(dog->GetID());
    add_vertical_hinge(west_jamb, intake_dog_id_,
                       JPH::RVec3(kIntakeDogHingeX, kIntakeDogCenterY,
                                  kIntakeBayFrontZ),
                       0.0F, kIntakeDogRetractAngle, kIntakeDogMaxTorqueNm,
                       &intake_dog_hinge_);
    if (intake_dog_hinge_ != nullptr) {
        // Commanded open from build time and never commanded otherwise.
        // The pack is the only thing holding it shut.
        intake_dog_hinge_->SetTargetAngularVelocity(kIntakeDogRetractSpeed);
    }

    // ---- MOD-INTAKE-BELT and the CAP-PENDANT catwalk -------------------
    intake_belt_id_ = track(add_box(
        bodies, JPH::Vec3(kIntakeBeltHalfX, kIntakeBeltHalfY, kIntakeBeltHalfZ),
        JPH::RVec3(kIntakeBeltX, kIntakeBeltTopY - kIntakeBeltHalfY, kIntakeBeltCenterZ),
        JPH::EMotionType::Kinematic, object_layers::kMoving, 0.9F,
        Simulation::kIntakeBeltEntityId));

    fixed(JPH::Vec3(1.6F, kIntakeBeltHalfY, 3.0F),
          JPH::RVec3(kIntakePendantX, kIntakePendantTopY - kIntakeBeltHalfY,
                     kIntakePendantZ),
          Simulation::kIntakeApronEntityId);
    // Ramp up to the catwalk so the pendant is reachable on foot without
    // spending a mantle, matching how every other stair in this world works.
    {
        const float rise = kIntakePendantTopY;
        const float run = 4.0F;
        const float length = std::sqrt(run * run + rise * rise);
        const float pitch = std::atan2(rise, run);
        fixed(JPH::Vec3(1.6F, 0.15F, length * 0.5F),
              JPH::RVec3(kIntakePendantX, rise * 0.5F, kIntakePendantZ + 3.0F + run * 0.5F),
              Simulation::kIntakeApronEntityId,
              JPH::Quat::sRotation(JPH::Vec3::sAxisX(), pitch));
    }

    // ---- MOD-YARD-JIB --------------------------------------------------
    const JPH::BodyID mast = fixed(
        JPH::Vec3(0.45F, kIntakeBoomHeight * 0.5F, 0.45F),
        JPH::RVec3(kIntakeJibMastX, kIntakeBoomHeight * 0.5F, kIntakeJibMastZ),
        Simulation::kIntakeJibMastEntityId);

    // Built already pointing -Z, down the line to the pack. The hinge takes
    // its world-space normal axes at construction, so "angle zero" is this
    // as-built pose and the +-0.90 rad slew limit sweeps symmetrically
    // around the throat.
    const JPH::Quat boom_rotation =
        JPH::Quat::sRotation(JPH::Vec3::sAxisY(), static_cast<float>(kPi * 0.5));
    JPH::Body *boom = add_shape_body(
        bodies, new JPH::BoxShape(JPH::Vec3(kIntakeBoomLength * 0.5F, 0.22F, 0.22F)),
        JPH::RVec3(kIntakeJibMastX, kIntakeBoomHeight,
                   kIntakeJibMastZ - kIntakeBoomLength * 0.5F),
        boom_rotation, JPH::EMotionType::Dynamic, object_layers::kMoving, 0.5F,
        Simulation::kIntakeJibBoomEntityId, kIntakeJibBoomMassKg);
    intake_boom_id_ = track(boom->GetID());
    add_vertical_hinge(mast, intake_boom_id_,
                       JPH::RVec3(kIntakeJibMastX, kIntakeBoomHeight, kIntakeJibMastZ),
                       -kIntakeJibSlewLimitRadians, kIntakeJibSlewLimitRadians,
                       kIntakeJibSlewTorqueNm, &intake_slew_hinge_);

    // The pack sits on the apron under the boom tip, pre-slung (Atlas B00
    // allows the rigging pre-placed; the hook is still a real constraint).
    const float pack_start_y = kIntakePackHalfY;
    const float pack_link_y = pack_start_y + kIntakePackHalfY;
    const float hook_start_y = pack_link_y + 0.20F;
    JPH::Body *pack = add_shape_body(
        bodies,
        new JPH::BoxShape(JPH::Vec3(kIntakePackHalfX, kIntakePackHalfY, kIntakePackHalfZ)),
        JPH::RVec3(kIntakeBayCenterX, pack_start_y, kIntakePackZ), JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic, object_layers::kMoving, 0.6F,
        Simulation::kIntakePackEntityId, kIntakePackMassKg);
    intake_pack_id_ = track(pack->GetID());

    JPH::Body *hook = add_shape_body(
        bodies, new JPH::BoxShape(JPH::Vec3(0.20F, 0.20F, 0.20F)),
        JPH::RVec3(kIntakeBayCenterX, hook_start_y, kIntakePackZ), JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic, object_layers::kMoving, 0.4F,
        Simulation::kIntakeJibHookEntityId, kIntakeJibHookMassKg);
    intake_hook_id_ = track(hook->GetID());
    // AS-002 makes this releasable (release_pack_to_cradle() /
    // sling_pack()): create_sling_pin() uses track_for_teardown=false,
    // exactly the needle pins' contract, so it is intake_sling_pin_'s
    // sole owner rather than also sitting in machine_constraints_, which
    // Jolt's ConstraintManager::Remove would then assert on if it had
    // already been removed once at runtime. Unchanged from AS-001: the
    // pin point is pack_link_y, 0.20 m below the hook's own built
    // hook_start_y -- not the hook's position -- so this is byte-
    // identical to the add_point_link() call it replaces.
    create_sling_pin(JPH::RVec3(kIntakeBayCenterX, pack_link_y, kIntakePackZ));

    const float hoist_travel = (kIntakeBoomHeight - 0.45F) - hook_start_y;
    add_motorized_slider(intake_boom_id_, intake_hook_id_, 0.0F, hoist_travel,
                         kIntakeJibWinchForceN, &intake_hoist_slider_);

    // The 9 t pack on its own fixed stand, permanently commanded up at the
    // same rated winch force. It proves the rating is enforced by the
    // solver without staging that failure as an unsafe lift on the working
    // jib (Atlas B00: "9 t overweight must stall").
    const JPH::BodyID overweight_mast = fixed(
        JPH::Vec3(0.35F, kIntakeOverweightMastHeight * 0.5F, 0.35F),
        JPH::RVec3(kIntakeOverweightStandX, kIntakeOverweightMastHeight * 0.5F,
                   kIntakeOverweightStandZ),
        Simulation::kIntakeJibMastEntityId);
    JPH::Body *overweight = add_shape_body(
        bodies,
        new JPH::BoxShape(JPH::Vec3(kIntakePackHalfX, kIntakePackHalfY, kIntakePackHalfZ)),
        JPH::RVec3(kIntakeOverweightStandX, kIntakePackHalfY, kIntakeOverweightStandZ),
        JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, object_layers::kMoving, 0.6F,
        Simulation::kIntakeOverweightPackEntityId, kIntakeOverweightPackMassKg);
    intake_overweight_pack_id_ = track(overweight->GetID());
    JPH::Ref<JPH::SliderConstraint> overweight_slider;
    add_motorized_slider(overweight_mast, intake_overweight_pack_id_, 0.0F,
                         kIntakeOverweightMastHeight - 2.0F * kIntakePackHalfY,
                         kIntakeJibWinchForceN, &overweight_slider);
    overweight_slider->SetTargetVelocity(kIntakeJibHoistRateMetersPerSec);

    // ---- MOD-STAIR-A ---------------------------------------------------
    // One inclined slab per flight, switchbacking along X, with a landing
    // pad at each turn. Same pattern as build_stack(): what you see drawn
    // on top and what you stand on are the same body.
    const float flight_length =
        std::sqrt(4.0F * kIntakeStairHalfRun * kIntakeStairHalfRun +
                  kIntakeStairFlightRise * kIntakeStairFlightRise);
    const float pitch = std::atan2(kIntakeStairFlightRise, 2.0F * kIntakeStairHalfRun);
    // An inclined slab's walking surface stands this far above the line
    // through its ends. Landings are raised to match, so a turn is a flush
    // step and not a lip the no-step-assist capsule has to climb.
    const float tread_surface = kIntakeStairSlabHalfY / std::cos(pitch);
    const auto flight_side = [](const int flight) {
        return (flight % 2 == 0) ? 1.0F : -1.0F;
    };
    for (int flight = 0; flight < kIntakeStairFlightCount; ++flight) {
        const float base_y = static_cast<float>(flight) * kIntakeStairFlightRise;
        // Even flights climb toward +X in the south lane, odd flights back
        // toward -X in the north lane.
        const float side = flight_side(flight);
        fixed(JPH::Vec3(flight_length * 0.5F, kIntakeStairSlabHalfY, kIntakeStairHalfWidth),
              JPH::RVec3(kIntakeBayCenterX, base_y + kIntakeStairFlightRise * 0.5F,
                         kIntakeStairZ + side * kIntakeStairLaneOffset),
              Simulation::kIntakeStairEntityId,
              JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), side * pitch));
    }
    // One landing at the foot of every flight, plus the one flight 5
    // arrives on. Each spans both lanes, so a turn is a walk across it.
    for (int flight = 0; flight <= kIntakeStairFlightCount; ++flight) {
        const float base_y = static_cast<float>(flight) * kIntakeStairFlightRise;
        fixed(JPH::Vec3(kIntakeStairLandingHalfX, kIntakeStairSlabHalfY,
                        kIntakeStairLandingHalfZ),
              JPH::RVec3(kIntakeBayCenterX - flight_side(flight) * kIntakeStairLandingX,
                         base_y + tread_surface - kIntakeStairSlabHalfY, kIntakeStairZ),
              Simulation::kIntakeStairEntityId);
    }

    // ---- the +24 m handoff ---------------------------------------------
    // Reaches from the top of flight 5 south past the bay wall to meet the
    // SKIN ladder head, so both braids arrive on the same deck.
    const float handoff_half_z = (kIntakeHandoffSouthZ - kIntakeHandoffNorthZ) * 0.5F;
    const float handoff_center_z = (kIntakeHandoffSouthZ + kIntakeHandoffNorthZ) * 0.5F;
    fixed(JPH::Vec3(kIntakeHandoffHalfX, kIntakeHandoffHalfY, handoff_half_z),
          JPH::RVec3(kIntakeHandoffCenterX,
                     kIntakeHandoffY + tread_surface - kIntakeHandoffHalfY,
                     handoff_center_z),
          Simulation::kIntakeHandoffEntityId);

    // ---- MOD-SKIN-LADDER-S ---------------------------------------------
    for (int rung = 1; rung <= kIntakeSkinRungCount; ++rung) {
        const float top_y = static_cast<float>(rung) * kIntakeSkinRungRise;
        const float rung_z =
            kIntakeSkinHeadZ + 2.0F * kIntakeSkinRungHalfZ *
                                   static_cast<float>(kIntakeSkinRungCount - rung);
        fixed(JPH::Vec3(kIntakeSkinRungHalfX, kIntakeSkinRungHalfY, kIntakeSkinRungHalfZ),
              JPH::RVec3(kIntakeSkinCenterX, top_y - kIntakeSkinRungHalfY, rung_z),
              Simulation::kIntakeSkinEntityId);
    }
}

void Simulation::PhysicsWorld::build_water_screw(JPH::BodyInterface &bodies) {
    const auto part = [this, &bodies](const JPH::Vec3 half, const JPH::RVec3 at,
                                      const std::uint64_t entity,
                                      const JPH::Quat rotation = JPH::Quat::sIdentity()) {
        machine_bodies_.push_back(add_box(bodies, half, at, JPH::EMotionType::Static,
                                          object_layers::kStatic, 0.75F, entity,
                                          rotation));
    };

    const JPH::Quat incline =
        JPH::Quat::sRotation(JPH::Vec3::sAxisX(), kWaterScrewIncline);
    const JPH::RVec3 mid(kWaterScrewX, (kWaterScrewLowY + kWaterScrewUpperY) * 0.5F,
                         (kWaterScrewLowZ + kWaterScrewUpperZ) * 0.5F);
    part(JPH::Vec3(0.72F, 0.10F, kWaterScrewLength * 0.5F), mid,
         Simulation::kWaterScrewFrameEntityId, incline);
    for (const float side : {-1.0F, 1.0F}) {
        part(JPH::Vec3(0.10F, 0.42F, kWaterScrewLength * 0.5F),
             JPH::RVec3(mid.GetX() + side * 0.62F, mid.GetY() + 0.25F, mid.GetZ()),
             Simulation::kWaterScrewFrameEntityId, incline);
    }

    part(JPH::Vec3(1.60F, 0.65F, 0.10F), JPH::RVec3(-18.0, 0.65, -96.4),
         Simulation::kWaterScrewFrameEntityId);
    part(JPH::Vec3(1.60F, 0.65F, 0.10F), JPH::RVec3(-18.0, 0.65, -98.6),
         Simulation::kWaterScrewFrameEntityId);
    part(JPH::Vec3(0.10F, 0.65F, 1.00F), JPH::RVec3(-19.1, 0.65, -97.5),
         Simulation::kWaterScrewFrameEntityId);
    part(JPH::Vec3(0.10F, 0.65F, 1.00F), JPH::RVec3(-16.9, 0.65, -97.5),
         Simulation::kWaterScrewFrameEntityId);

    part(JPH::Vec3(2.2F, 0.15F, 2.2F), JPH::RVec3(-18.0, 4.85, -108.2),
         Simulation::kWaterScrewTankEntityId);
    for (const float side : {-1.0F, 1.0F}) {
        part(JPH::Vec3(0.12F, 0.45F, 1.15F),
             JPH::RVec3(-18.0 + side * 1.12F, 5.35, -108.2),
             Simulation::kWaterScrewTankEntityId);
        part(JPH::Vec3(1.12F, 0.45F, 0.12F),
             JPH::RVec3(-18.0, 5.35, -108.2 + side * 1.12F),
             Simulation::kWaterScrewTankEntityId);
    }
    for (const float sx : {-1.0F, 1.0F}) {
        for (const float sz : {-1.0F, 1.0F}) {
            part(JPH::Vec3(0.14F, 2.35F, 0.14F),
                 JPH::RVec3(-18.0 + sx * 1.75F, 2.35, -108.2 + sz * 1.75F),
                 Simulation::kWaterScrewTankEntityId);
        }
    }
    part(JPH::Vec3(0.32F, 0.85F, 0.32F),
         JPH::RVec3(kWaterScrewStationX, 0.85, kWaterScrewStationZ),
         Simulation::kWaterScrewStationEntityId);
}

void Simulation::PhysicsWorld::build_legal_forty(JPH::BodyInterface &bodies) {
    const auto track = [this](const JPH::BodyID id) {
        machine_bodies_.push_back(id);
        return id;
    };
    const auto fixed = [&](const JPH::Vec3 half_extent, const JPH::RVec3 position,
                           const std::uint64_t entity_id,
                           const JPH::Quat rotation = JPH::Quat::sIdentity()) {
        track(add_box(bodies, half_extent, position, JPH::EMotionType::Static,
                      object_layers::kStatic, 0.85F, entity_id, rotation));
    };

    const float handoff_pitch = std::atan2(kIntakeStairFlightRise, 2.0F * kIntakeStairHalfRun);
    const float handoff_tread_surface = kIntakeStairSlabHalfY / std::cos(handoff_pitch);
    const float handoff_surface_y = kIntakeHandoffY + handoff_tread_surface;
    const float mid_landing_surface_y = handoff_surface_y + kLegalFortyFlightRiseMeters;
    const float hall_deck_surface_y = mid_landing_surface_y + kLegalFortyFlightRiseMeters;

    // ---- MOD-STAIR-A-SWING: the dynamic bascule flight -------------------
    const JPH::RVec3 hinge_point(kIntakeSwingHingeX, mid_landing_surface_y, kIntakeSwingHingeZ);
    // R(phi), phi = pi/2 - theta, maps local +X to world (sin theta,
    // cos theta, 0) about the world +Z hinge axis -- verified against
    // Jolt::Quat::sRotation's own [axis*sin(angle/2), cos(angle/2)] form
    // (see kIntakeSwingHingeTravelRadians's comment above). Built at
    // theta_stowed, so this is also the body's as-built rotation.
    const float stowed_phi = static_cast<float>(kPi) * 0.5F - kLegalFortyStowedThetaRadians;
    const JPH::Quat stowed_rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), stowed_phi);
    // Body centre = hinge - R(phi) * (half_length, 0, 0): the hinge sits at
    // the body's own local (+half_length, 0, 0), its "top end".
    const JPH::Vec3 hinge_to_centre_world =
        stowed_rotation * JPH::Vec3(kLegalFortyFlightHalfLength, 0.0F, 0.0F);
    const JPH::RVec3 swing_build_centre = hinge_point - hinge_to_centre_world;

    JPH::Body *swing_flight = add_shape_body(
        bodies,
        new JPH::BoxShape(JPH::Vec3(kLegalFortyFlightHalfLength, kIntakeStairSlabHalfY,
                                    kLegalFortyFlightHalfWidth)),
        swing_build_centre, stowed_rotation, JPH::EMotionType::Dynamic, object_layers::kMoving,
        0.7F, Simulation::kIntakeSwingFlightEntityId, kLegalFortyFlightMassKg);
    intake_swing_flight_id_ = track(swing_flight->GetID());

    // A small static anchor at the hinge point -- the hinge's first body,
    // exactly like the needle hoist's fixed head (add_motorized_slider's
    // own comment: "the head is the actual... anchor, the mast beneath it
    // is proof scaffolding"). Sized well under the flight's own 0.18 m
    // corner-sweep radius (this file's clearances note above, echoing
    // the plan's own: "the slab's top-end corners lie 0.18 m off the
    // hinge axis and sweep a 0.18 m circle") -- a cube of half-extent h
    // reaches h*sqrt(2) at its corner, so h=0.08 stays inside that
    // circle by 0.067 m at every angle. AS-001 lost a day to the
    // opposite mistake once already (MOD-DOG-A's own hinge sitting
    // flush with its jamb); this is that same trap, self-inflicted
    // against AS-002's own anchor rather than a neighbour's structure --
    // found by direct observation: at the old 0.30 m half-extent (bigger
    // than the sweep radius on every side) the loaded flight never
    // opened past a fraction of a degree, pinned by its own anchor.
    //
    // Shrinking it to 0.08 only thinned that same pinning, never removed
    // it: the flight's own cross-section is coincident with the hinge
    // pivot at every sweep angle by construction, so ANY nonzero-size
    // anchor box keeps a sliver of solid overlap there always -- found
    // by direct diagnostic tracing (OnContactAdded/Persisted instrumented
    // to log the actual world-space contact point, not just entity IDs)
    // showing a real, continuous PERSISTED contact at the hinge itself
    // for the entire length of a 20+ s window in which the flight was
    // otherwise correctly, fully unloaded and should have been relaxing
    // freely under its own weight. kIntakeSwingAnchorEntityId gives this
    // one body its own identity (unlike every other static piece of
    // MOD-STAIR-A, which stays grouped under kIntakeStairEntityId) so
    // OnContactValidate below can exclude exactly this pair, the same
    // way it already excludes the flight from the handoff deck: the
    // hinge CONSTRAINT is what relates the anchor and the flight, and
    // ordinary contact resolution between them was never meaningful.
    const JPH::BodyID swing_anchor = track(add_box(
        bodies, JPH::Vec3(0.08F, 0.08F, 0.08F), hinge_point, JPH::EMotionType::Static,
        object_layers::kStatic, 0.8F, Simulation::kIntakeSwingAnchorEntityId));
    // Unmotored: gravity alone holds this at its stowed limit (jolt angle
    // 0, this build pose) until MOD-CW-CRADLE's rope tension beats it.
    // Limits are negative-only -- see kIntakeSwingHingeTravelRadians.
    add_hinge(swing_anchor, intake_swing_flight_id_, hinge_point,
              -kIntakeSwingHingeTravelRadians, 0.0F, &intake_swing_hinge_,
              Simulation::kIntakeStairEntityId);

    // ---- MOD-CW-CRADLE: dynamic car on a free vertical slider ------------
    // The hook hangs at the boom tip, so working radius is fixed at the
    // 12 m boom; only the slew bearing chooses where on that circle the
    // cradle sits. Sited on the SAME (east, +X) side as the hinge/flight
    // above, matching AS-002's own geometry table (hinge x=+7.856,
    // cradle x=+8.608): hook_x = -boomLength*sin(angle) for a POSITIVE
    // set_intake_slew_input, verified against the running mechanism, so
    // reaching this +X cradle is a NEGATIVE slew command
    // (kIntakeCwCradleBearingRadians is the magnitude; update_intake's
    // sign is negated at every call site that targets the cradle).
    const float cradle_x =
        kIntakeJibMastX + kIntakeBoomLength * std::sin(kIntakeCwCradleBearingRadians);
    const float cradle_z =
        kIntakeJibMastZ - kIntakeBoomLength * std::cos(kIntakeCwCradleBearingRadians);
    const JPH::RVec3 cradle_build_centre(cradle_x, kIntakeCwCradleBuildCenterY, cradle_z);

    // Offset south of the car's own centreline, clear of the column the
    // pack is lowered through -- found by direct observation: a mast
    // built AT the car's own xz (matching the plan's literal "same xz")
    // stood directly in that column, and the descending pack snagged its
    // top on every attempt. add_slider only needs a static anchor body
    // for the vertical constraint; nothing requires it to share the
    // car's own xz, and the cradle is open in +-Z for exactly this kind
    // of side approach.
    const float guide_mast_z = cradle_z + 2.0F;
    const JPH::BodyID cradle_guide = track(add_box(
        bodies,
        JPH::Vec3(kIntakeCwCradleGuideHalfX, kIntakeCwCradleGuideHalfY,
                  kIntakeCwCradleGuideHalfZ),
        JPH::RVec3(cradle_x, kIntakeCwCradleGuideHalfY, guide_mast_z), JPH::EMotionType::Static,
        object_layers::kStatic, 0.8F, Simulation::kIntakeCwCradleEntityId));

    // Three-sided open frame -- floor and two side rails, open in +-Z --
    // not a closed hopper: a 2.4 m hopper cannot take the 2.2 x 2.3 m pack
    // without pushing the cradle into the bay's front wall.
    JPH::Body *cradle = add_shape_body(
        bodies,
        new JPH::BoxShape(JPH::Vec3(kIntakeCwCradleHalfX, kIntakeCwCradleHalfY,
                                    kIntakeCwCradleHalfZ)),
        cradle_build_centre, JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic,
        object_layers::kMoving, 0.7F, Simulation::kIntakeCwCradleEntityId,
        kIntakeCwCradleTareMassKg);
    intake_cw_cradle_id_ = track(cradle->GetID());
    add_slider(cradle_guide, intake_cw_cradle_id_, kIntakeCwCradleLimitMinMeters,
              kIntakeCwCradleLimitMaxMeters);

    // ---- The rope: one generalised JPH::PulleyConstraint -----------------
    const JPH::RVec3 sheave_a(kIntakeSwingHingeX,
                              mid_landing_surface_y + kIntakeSwingSheaveHeightMeters,
                              kIntakeSwingHingeZ);
    const JPH::RVec3 sheave_b(cradle_x,
                              mid_landing_surface_y + kIntakeSwingSheaveHeightMeters,
                              cradle_z);
    const JPH::RVec3 flight_bracket_point =
        bodies.GetCenterOfMassTransform(intake_swing_flight_id_) *
        JPH::RVec3(kIntakeSwingBracketLocalX, kIntakeSwingBracketLocalY, 0.0);
    const JPH::RVec3 cradle_top_point =
        bodies.GetCenterOfMassTransform(intake_cw_cradle_id_) *
        JPH::RVec3(0.0, kIntakeCwCradleHalfY, 0.0);
    const float flight_side_length = JPH::Vec3(sheave_a - flight_bracket_point).Length();
    const float cradle_side_length_at_build =
        JPH::Vec3(sheave_b - cradle_top_point).Length();
    // Tension-only, rated per the T_crit derivation above (empty cradle
    // 0.37x the stowed demand, loaded 1.21x): never pushes, and its total
    // length is fixed at this as-built (empty-cradle, stowed-flight) sum,
    // which is also this mechanism's one physically consistent rest state
    // when the rope is fully taut.
    intake_cw_pulley_ =
        add_pulley(flight_bracket_point, sheave_a, intake_swing_flight_id_, cradle_top_point,
                  sheave_b, intake_cw_cradle_id_, 1.0F, 0.0F,
                  flight_side_length + cradle_side_length_at_build);

    // ---- MOD-STAIR-A upper flight (static) + mid-landing ------------------
    // An inclined slab like every other flight in this file -- AS-001's own
    // pattern (Quat::sRotation(axisZ, side*pitch)), not the flat, unrotated
    // box this was until now. Climbs toward -X (side = -1, "odd flights
    // back toward -X" per flight_side's own comment): the un-rotated half-
    // length (8.00) times cos(pitch) puts its rotated footprint at
    // x in [-4.506, 9.350], which is exactly the well's own centre-X at
    // the top end and the mid-landing's own centre-X at the foot -- this
    // position was already sited for a rotated slab, only the rotation
    // itself was missing.
    const float upper_flight_pitch =
        std::asin(kLegalFortyFlightRiseMeters / kLegalFortyFlightLength);
    fixed(JPH::Vec3(kLegalFortyFlightHalfLength, kIntakeStairSlabHalfY,
                    kLegalFortyFlightHalfWidth),
          JPH::RVec3(kLegalFortyUpperFlightCenterX,
                     mid_landing_surface_y + kLegalFortyFlightRiseMeters * 0.5F,
                     kLegalFortyUpperFlightCenterZ),
          Simulation::kIntakeStairEntityId,
          JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), -upper_flight_pitch));
    fixed(JPH::Vec3(kLegalFortyMidLandingHalfX, kIntakeStairSlabHalfY,
                    kLegalFortyMidLandingHalfZ),
          JPH::RVec3(kLegalFortyMidLandingCenterX, mid_landing_surface_y - kIntakeStairSlabHalfY,
                     kLegalFortyMidLandingCenterZ),
          Simulation::kIntakeStairEntityId);

    // ---- MOD-HALL-DECK lower landing, with its stair well -----------------
    // Four strips tiling the deck minus a rectangular well, exactly the
    // jamb+lintel pattern the bay's own throat already uses, rotated from
    // a wall opening to a floor opening.
    const float well_min_x = kLegalFortyWellCenterX - kLegalFortyWellHalfX;
    const float well_max_x = kLegalFortyWellCenterX + kLegalFortyWellHalfX;
    const float well_min_z = kLegalFortyWellCenterZ - kLegalFortyWellHalfZ;
    const float well_max_z = kLegalFortyWellCenterZ + kLegalFortyWellHalfZ;
    const float deck_min_x = -kLegalFortyHallDeckHalfX;
    const float deck_max_x = kLegalFortyHallDeckHalfX;
    const float deck_min_z = kLegalFortyHallDeckCenterZ - kLegalFortyHallDeckHalfZ;
    const float deck_max_z = kLegalFortyHallDeckCenterZ + kLegalFortyHallDeckHalfZ;
    const float deck_center_y = hall_deck_surface_y - kLegalFortyHallDeckHalfThickness;
    const auto span = [](const float lo, const float hi) {
        return std::pair<float, float>{(lo + hi) * 0.5F, (hi - lo) * 0.5F};
    };
    const auto [south_cz, south_hz] = span(well_max_z, deck_max_z);
    fixed(JPH::Vec3(kLegalFortyHallDeckHalfX, kLegalFortyHallDeckHalfThickness, south_hz),
          JPH::RVec3(0.0F, deck_center_y, south_cz), Simulation::kIntakeHallDeckEntityId);
    const auto [north_cz, north_hz] = span(deck_min_z, well_min_z);
    fixed(JPH::Vec3(kLegalFortyHallDeckHalfX, kLegalFortyHallDeckHalfThickness, north_hz),
          JPH::RVec3(0.0F, deck_center_y, north_cz), Simulation::kIntakeHallDeckEntityId);
    const auto [west_cx, west_hx] = span(deck_min_x, well_min_x);
    fixed(JPH::Vec3(west_hx, kLegalFortyHallDeckHalfThickness, kLegalFortyWellHalfZ),
          JPH::RVec3(west_cx, deck_center_y, kLegalFortyWellCenterZ),
          Simulation::kIntakeHallDeckEntityId);
    const auto [east_cx, east_hx] = span(well_max_x, deck_max_x);
    fixed(JPH::Vec3(east_hx, kLegalFortyHallDeckHalfThickness, kLegalFortyWellHalfZ),
          JPH::RVec3(east_cx, deck_center_y, kLegalFortyWellCenterZ),
          Simulation::kIntakeHallDeckEntityId);

    // ---- MOD-SKIN-LADDER-S continuation, rungs 16-20 -----------------------
    // Continues AS-001's own rung law north and up to the mid-landing's
    // own height -- see the constants block above for why it stops there
    // rather than climbing on to the hall deck directly.
    for (int rung = kLegalFortySkinRungFirst; rung <= kLegalFortySkinRungLast; ++rung) {
        const float top_y = handoff_surface_y +
                            kIntakeSkinRungRise *
                                static_cast<float>(rung - kIntakeSkinRungCount);
        const float rung_z = kLegalFortySkinRungFirstZ -
                             kLegalFortySkinRungStepZMeters *
                                 static_cast<float>(rung - kLegalFortySkinRungFirst);
        const float rung_x = kIntakeSkinCenterX +
                             (rung > kLegalFortySkinRungFirst ? kLegalFortySkinRungJogX : 0.0F);
        fixed(JPH::Vec3(kIntakeSkinRungHalfX, kIntakeSkinRungHalfY, kIntakeSkinRungHalfZ),
              JPH::RVec3(rung_x, top_y - kIntakeSkinRungHalfY, rung_z),
              Simulation::kIntakeSkinEntityId);
    }

    // ---- the walkway from rung 20 to the mid-landing -----------------------
    {
        const auto [walk_cx, walk_hx] =
            span(kLegalFortySkinWalkwayMinX, kLegalFortySkinWalkwayMaxX);
        fixed(JPH::Vec3(walk_hx, kIntakeStairSlabHalfY, kLegalFortySkinWalkwayHalfZ),
              JPH::RVec3(walk_cx, mid_landing_surface_y - kIntakeStairSlabHalfY,
                         kLegalFortySkinWalkwayCenterZ),
              Simulation::kIntakeStairEntityId);
    }
}

void Simulation::PhysicsWorld::build_hook5_rack(JPH::BodyInterface &bodies) {
    const auto box = [this, &bodies](const float x0, const float x1, const float y0,
                                     const float y1, const float z0, const float z1) {
        const JPH::BodyID id = add_box(
            bodies, JPH::Vec3((x1 - x0) * 0.5F, (y1 - y0) * 0.5F, (z1 - z0) * 0.5F),
            JPH::RVec3((x0 + x1) * 0.5F, (y0 + y1) * 0.5F, (z0 + z1) * 0.5F),
            JPH::EMotionType::Static, object_layers::kStatic, 0.7F,
            Simulation::kHook5CageEntityId);
        machine_bodies_.push_back(id);
        return id;
    };
    const float wall_inner_z = kHook5MinZ + kHook5WallThickness;
    // The west buttress, full height: the one face the belt reaches.
    box(kHook5MinX, kHook5ButtressMaxX, 0.0F, kHook5CageTopY, kHook5MinZ, kHook5MaxZ);
    // North and east walls, up to the roof.
    box(kHook5ButtressMaxX, kHook5MaxX, 0.0F, kHook5WallTopY,
        kHook5MaxZ - kHook5WallThickness, kHook5MaxZ);
    box(kHook5MaxX - kHook5WallThickness, kHook5MaxX, 0.0F, kHook5WallTopY, kHook5MinZ,
        kHook5MaxZ - kHook5WallThickness);
    // The south wall either side of the doorway, and the header over it.
    const JPH::BodyID south_west = box(kHook5ButtressMaxX, kHook5DoorwayMinX, 0.0F,
                                       kHook5WallTopY, kHook5MinZ, wall_inner_z);
    box(kHook5DoorwayMaxX, kHook5MaxX - kHook5WallThickness, 0.0F, kHook5WallTopY,
        kHook5MinZ, wall_inner_z);
    box(kHook5DoorwayMinX, kHook5DoorwayMaxX, kHook5DoorwayTopY, kHook5WallTopY, kHook5MinZ,
        wall_inner_z);
    // The roof: four strips around the hatch.
    box(kHook5ButtressMaxX, kHook5HatchMinX, kHook5WallTopY, kHook5CageTopY, kHook5MinZ,
        kHook5MaxZ);
    box(kHook5HatchMaxX, kHook5MaxX, kHook5WallTopY, kHook5CageTopY, kHook5MinZ, kHook5MaxZ);
    box(kHook5HatchMinX, kHook5HatchMaxX, kHook5WallTopY, kHook5CageTopY, kHook5HatchMaxZ,
        kHook5MaxZ);
    box(kHook5HatchMinX, kHook5HatchMaxX, kHook5WallTopY, kHook5CageTopY, kHook5MinZ,
        kHook5HatchMinZ);
    // The bar's brackets: a ledge under each end, and a stop on its north
    // face that rises only 0.02 m past the bar's centreline -- enough to
    // take the door's push, low enough that a lift of 0.14 m clears it.
    const float bar_bottom = kHook5BarCenterY - kHook5BarHalfSection;
    const float bar_north = kHook5BarCenterZ + kHook5BarHalfSection;
    for (const float bracket_x : {kHook5BracketWestX, kHook5BracketEastX}) {
        box(bracket_x - kHook5BracketHalfX, bracket_x + kHook5BracketHalfX, bar_bottom - 0.08F,
            bar_bottom, wall_inner_z, bar_north + 0.10F);
        box(bracket_x - kHook5BracketHalfX, bracket_x + kHook5BracketHalfX, bar_bottom - 0.06F,
            kHook5BarCenterY + 0.02F, bar_north, bar_north + 0.10F);
    }
    // The rack: a pedestal against the east wall.
    box(10.95F, kHook5MaxX - kHook5WallThickness, 0.0F, kHook5RackTopY,
        kHook5BlockSeatZ - 0.45F, kHook5BlockSeatZ + 0.45F);

    // MOD-HOOK5-DOOR, driven open from build time and never commanded
    // again. About -Y, positive rotation carries the leaf's far end toward
    // +Z: into the cage, where the bar lies.
    JPH::Body *door = add_shape_body(
        bodies,
        new JPH::BoxShape(JPH::Vec3(kHook5DoorHalfLength, kHook5DoorHalfHeight,
                                    kHook5DoorHalfThickness)),
        JPH::RVec3(kHook5DoorHingeX + kHook5DoorHalfLength, kHook5DoorCenterY, kHook5DoorZ),
        JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, object_layers::kMoving, 0.5F,
        Simulation::kHook5DoorEntityId, kHook5DoorMassKg);
    hook5_door_id_ = door->GetID();
    machine_bodies_.push_back(hook5_door_id_);
    add_vertical_hinge(south_west, hook5_door_id_,
                       JPH::RVec3(kHook5DoorHingeX, kHook5DoorCenterY, kHook5DoorZ), 0.0F,
                       kHook5DoorOpenAngle, kHook5DoorDriveTorqueNm, &hook5_door_hinge_,
                       -JPH::Vec3::sAxisY());
    if (hook5_door_hinge_ != nullptr) {
        hook5_door_hinge_->SetTargetAngularVelocity(kHook5DoorDriveSpeed);
    }

    // MOD-HOOK5-BAR and the hook block: plain bodies, at rest where they sit.
    JPH::Body *bar = add_shape_body(
        bodies,
        new JPH::BoxShape(
            JPH::Vec3(kHook5BarHalfLength, kHook5BarHalfSection, kHook5BarHalfSection)),
        JPH::RVec3(kHook5BarCenterX, kHook5BarCenterY, kHook5BarCenterZ),
        JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, object_layers::kMoving, 0.5F,
        Simulation::kHook5BarEntityId, kHook5BarMassKg);
    hook5_bar_id_ = bar->GetID();
    machine_bodies_.push_back(hook5_bar_id_);
    JPH::Body *block = add_shape_body(
        bodies, new JPH::BoxShape(JPH::Vec3(kHook5BlockHalf, kHook5BlockHalfY, kHook5BlockHalf)),
        JPH::RVec3(kHook5BlockSeatX, kHook5BlockSeatY, kHook5BlockSeatZ),
        JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, object_layers::kMoving, 0.6F,
        Simulation::kHook5BlockEntityId, kHook5BlockMassKg);
    hook5_block_id_ = block->GetID();
    machine_bodies_.push_back(hook5_block_id_);
}

void Simulation::PhysicsWorld::build_kernel_sump(JPH::BodyInterface &bodies) {
    const auto track = [this](const JPH::BodyID id) {
        machine_bodies_.push_back(id);
        return id;
    };

    const float deck_y = kSumpPlatformTopY - kSumpDeckHalfThickness;
    track(add_box(bodies,
                  JPH::Vec3(kSumpApproachDeckHalfX, kSumpDeckHalfThickness, kSumpDeckHalfZ),
                  JPH::RVec3(kSumpApproachDeckX, deck_y, kSumpCenterZ),
                  JPH::EMotionType::Static, object_layers::kStatic, 0.9F,
                  Simulation::kStaticDeckEntityId));
    track(add_box(bodies, JPH::Vec3(kSumpFarDeckHalfX, kSumpDeckHalfThickness, kSumpDeckHalfZ),
                  JPH::RVec3(kSumpFarDeckX, deck_y, kSumpCenterZ),
                  JPH::EMotionType::Static, object_layers::kStatic, 0.9F,
                  Simulation::kStaticDeckEntityId));

    // Support legs, purely structural -- under each fixed deck section,
    // clear of the grate span so nothing but the grate itself is ever the
    // question of whether this walkway holds.
    track(add_box(bodies, JPH::Vec3(0.25F, kSumpPlatformTopY * 0.5F, 0.25F),
                  JPH::RVec3(kSumpApproachDeckX, kSumpPlatformTopY * 0.5F, kSumpCenterZ),
                  JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                  Simulation::kStaticDeckEntityId));
    track(add_box(bodies, JPH::Vec3(0.25F, kSumpPlatformTopY * 0.5F, 0.25F),
                  JPH::RVec3(kSumpFarDeckX, kSumpPlatformTopY * 0.5F, kSumpCenterZ),
                  JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                  Simulation::kStaticDeckEntityId));

    JPH::Body *grate = add_shape_body(
        bodies, new JPH::BoxShape(JPH::Vec3(kSumpGrateHalfX, kSumpDeckHalfThickness, kSumpDeckHalfZ)),
        JPH::RVec3(kSumpGrateX, deck_y, kSumpCenterZ), JPH::Quat::sIdentity(),
        JPH::EMotionType::Static, object_layers::kStatic, 0.9F, Simulation::kSumpGrateEntityId,
        0.0F);
    sump_grate_id_ = track(grate->GetID());
    // The sump starts full (existing truth: wet is the default), so the
    // grate starts as a sensor -- see update_sump for why a sensor alone
    // is not the whole mechanism.
    bodies.SetIsSensor(sump_grate_id_, true);
    sump_volume_kg_ = kSumpCapacityKg;
}

} // namespace scraperx::sim
