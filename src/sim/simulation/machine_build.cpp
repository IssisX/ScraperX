#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

void Simulation::PhysicsWorld::build_machine(JPH::BodyInterface &bodies) {
    const auto track = [this](const JPH::BodyID id) {
        machine_bodies_.push_back(id);
        return id;
    };

    // Static plant structure.
    const JPH::BodyID machine_pylon = track(add_box(
        bodies, JPH::Vec3(0.55F, 1.45F, 1.4F), JPH::RVec3(kTipperHingeX, 1.45, kTipperZ),
        JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
        Simulation::kMachinePylonEntityId));
    const JPH::BodyID valve_pylon = track(add_box(
        bodies, JPH::Vec3(0.3F, 3.6F, 0.3F), JPH::RVec3(kValveHingeX, 3.6, -92.25),
        JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
        Simulation::kMachinePylonEntityId));
    track(add_box(bodies, JPH::Vec3(0.4F, 6.2F, 0.4F), JPH::RVec3(35.9, 6.2, kScoopZ),
                  JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                  Simulation::kMachinePylonEntityId));
    track(add_box(bodies, JPH::Vec3(1.7F, 2.3F, 1.7F), JPH::RVec3(30.5, 2.3, -101.5),
                  JPH::EMotionType::Static, object_layers::kStatic, 0.75F,
                  Simulation::kVesselShellEntityId));
    track(add_box(bodies, JPH::Vec3(0.9F, 0.625F, 0.8F), JPH::RVec3(27.0, 0.625, -94.05),
                  JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                  Simulation::kMachinePylonEntityId));
    track(add_box(bodies, JPH::Vec3(0.9F, 1.25F, 0.8F), JPH::RVec3(28.3, 1.25, -94.05),
                  JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                  Simulation::kMachinePylonEntityId));
    // The landing starts at the second step's east face (x = 29.2). It used
    // to start 0.7 m west of it, over the step: 1.0 m of headroom on the
    // step's top, a slot a standing capsule walked into and wedged.
    track(add_box(bodies, JPH::Vec3(1.0F, 0.15F, 0.8F), JPH::RVec3(30.2, 3.65, -94.05),
                  JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                  Simulation::kMachinePylonEntityId));

    const JPH::BodyID lift_mast = track(add_box(
        bodies, JPH::Vec3(0.4F, 5.6F, 0.4F), JPH::RVec3(kLiftMastX, 5.6, kLiftZ),
        JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
        Simulation::kLiftMastEntityId));
    track(add_box(bodies, JPH::Vec3(2.6F, 0.14F, 9.0F), JPH::RVec3(kLiftPlatformX, 8.55, -112.0),
                  JPH::EMotionType::Static, object_layers::kStatic, 0.7F,
                  Simulation::kCatwalkEntityId));

    // Return basin: a sloped real surface that walks the ballast back to the
    // hoist mouth under gravity and friction alone, with lane guards so it
    // cannot wander out of the machine.
    track(add_box(bodies, JPH::Vec3(1.2F, 0.14F, 1.5F), JPH::RVec3(31.82, 1.84, kScoopZ),
                  JPH::EMotionType::Static, object_layers::kStatic, 0.30F,
                  Simulation::kCatchBasinEntityId,
                  JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), -0.20F)));
    for (const float guard_z : {kScoopZ - 1.55F, kScoopZ + 1.55F}) {
        track(add_box(bodies, JPH::Vec3(1.4F, 0.45F, 0.12F),
                      JPH::RVec3(31.82, 2.20, guard_z), JPH::EMotionType::Static,
                      object_layers::kStatic, 0.4F, Simulation::kChuteEntityId));
    }

    // Hoist scoop: four kinematic panels driven by one rigid transform, open
    // on its -x face so a forward tilt discharges the ballast.
    scoop_local_[0] = JPH::Vec3(0.0F, -0.03F, 0.0F);
    scoop_local_[1] = JPH::Vec3(1.42F, 0.70F, 0.0F);
    scoop_local_[2] = JPH::Vec3(0.0F, 0.70F, -1.42F);
    scoop_local_[3] = JPH::Vec3(0.0F, 0.70F, 1.42F);
    const JPH::Vec3 scoop_half[4] = {
        JPH::Vec3(1.30F, 0.03F, 1.30F),
        JPH::Vec3(0.12F, 0.70F, 1.30F),
        JPH::Vec3(1.30F, 0.70F, 0.12F),
        JPH::Vec3(1.30F, 0.70F, 0.12F),
    };
    for (int i = 0; i < 4; ++i) {
        scoop_ids_[i] = track(add_box(
            bodies, scoop_half[i],
            JPH::RVec3(kScoopX + scoop_local_[i].GetX(),
                       kScoopBottomY + scoop_local_[i].GetY(),
                       kScoopZ + scoop_local_[i].GetZ()),
            JPH::EMotionType::Kinematic, object_layers::kMoving, 0.25F,
            Simulation::kHoistScoopEntityId));
    }

    // Ballast: a real 380 kg mass. The player can push it, be struck by it, or
    // stand where it lands.
    ballast_id_ = track(add_box(bodies, JPH::Vec3(0.42F, 0.42F, 0.42F),
                                JPH::RVec3(34.6, 1.0, kScoopZ), JPH::EMotionType::Dynamic,
                                object_layers::kMoving, 0.45F, Simulation::kBallastEntityId,
                                JPH::Quat::sIdentity(), kBallastMassKg));

    // Tipper: beam plus an inboard counterweight lump, so it rests with the
    // catch end raised and resets itself once the ballast rolls off.
    JPH::StaticCompoundShapeSettings tipper_settings;
    tipper_settings.AddShape(JPH::Vec3::sZero(), JPH::Quat::sIdentity(),
                             new JPH::BoxShape(JPH::Vec3(3.6F, 0.20F, 1.1F)));
    tipper_settings.AddShape(JPH::Vec3(-3.95F, -0.30F, 0.0F), JPH::Quat::sIdentity(),
                             new JPH::BoxShape(JPH::Vec3(0.44F, 0.44F, 0.44F)));
    JPH::Body *tipper = add_shape_body(
        bodies, tipper_settings.Create().Get(),
        JPH::RVec3(kTipperHingeX, kTipperHingeY, kTipperZ), JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic, object_layers::kMoving, 0.30F,
        Simulation::kTipperEntityId, kTipperMassKg);
    tipper_id_ = track(tipper->GetID());

    // Valve lever: arm plus an outboard counterweight, so gravity shuts the
    // valve and only rope tension opens it.
    JPH::StaticCompoundShapeSettings lever_settings;
    lever_settings.AddShape(JPH::Vec3(-0.80F, 0.0F, 0.0F), JPH::Quat::sIdentity(),
                            new JPH::BoxShape(JPH::Vec3(0.80F, 0.13F, 0.20F)));
    lever_settings.AddShape(JPH::Vec3(0.62F, 0.0F, 0.0F), JPH::Quat::sIdentity(),
                            new JPH::BoxShape(JPH::Vec3(0.34F, 0.34F, 0.34F)));
    JPH::Body *lever = add_shape_body(
        bodies, lever_settings.Create().Get(),
        JPH::RVec3(kValveHingeX, kValveHingeY, -93.0), JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic, object_layers::kMoving, 0.6F,
        Simulation::kValveLeverEntityId, kValveLeverMassKg);
    valve_lever_id_ = track(lever->GetID());

    // WO-010 treadle: the plant's human-scale control, on the catwalk deck.
    // A plate hinged at its inboard end with a counterweight just past the
    // hinge, so it rests level against its stop with the valve shut, and an
    // 85 kg body standing on it swings it down against that counterweight.
    // Pylon top stops below the plate's underside so the hinge is free.
    const JPH::BodyID treadle_pylon = track(add_box(
        bodies, JPH::Vec3(0.24F, 0.155F, 0.34F),
        JPH::RVec3(kTreadleX, kCatwalkDeckY + 0.155, kTreadleZ),
        JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
        Simulation::kMachinePylonEntityId));
    JPH::StaticCompoundShapeSettings treadle_settings;
    treadle_settings.AddShape(
        JPH::Vec3(-kTreadlePlateMeters * 0.5F, 0.0F, 0.0F), JPH::Quat::sIdentity(),
        new JPH::BoxShape(JPH::Vec3(kTreadlePlateMeters * 0.5F, 0.04F, 0.55F)));
    // Counterweight rides above the hinge line: only its x offset sets the
    // restoring torque, so putting it high keeps it clear of the deck.
    treadle_settings.AddShape(JPH::Vec3(0.55F, 0.42F, 0.0F), JPH::Quat::sIdentity(),
                              new JPH::BoxShape(JPH::Vec3(0.32F, 0.32F, 0.32F)));
    JPH::Body *treadle = add_shape_body(
        bodies, treadle_settings.Create().Get(),
        JPH::RVec3(kTreadleX, kTreadleHingeY, kTreadleZ), JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic, object_layers::kMoving, 0.9F,
        Simulation::kTreadleEntityId, kTreadleMassKg);
    treadle_id_ = track(treadle->GetID());
    add_hinge(treadle_pylon, treadle_id_, JPH::RVec3(kTreadleX, kTreadleHingeY, kTreadleZ),
              kTreadleRestAngle, kTreadleDepressedAngle, &treadle_hinge_,
              Simulation::kMachinePylonEntityId);

    // Sheave masts carrying the control cable. Static, and load-bearing only
    // as pulley anchor points.
    track(add_box(bodies, JPH::Vec3(0.10F, 1.0F, 0.10F),
                  JPH::RVec3(kTreadleX - kTreadleCableArm, kCatwalkDeckY + 1.61,
                             kTreadleMastZ),
                  JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                  Simulation::kMachinePylonEntityId));
    track(add_box(bodies, JPH::Vec3(0.10F, 1.2F, 0.10F),
                  JPH::RVec3(kValveHingeX + kValveCableArm, kValveSheaveY - 1.2, kValveMastZ),
                  JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                  Simulation::kMachinePylonEntityId));

    // Lift platform and its counterweight, each on a real vertical slider and
    // joined by a real pulley rope.
    JPH::Body *platform = add_shape_body(
        bodies, new JPH::BoxShape(JPH::Vec3(2.3F, 0.16F, 2.3F)),
        JPH::RVec3(kLiftPlatformX, kLiftPlatformRestY, kLiftZ), JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic, object_layers::kMoving, 0.9F,
        Simulation::kLiftPlatformEntityId, kLiftPlatformMassKg);
    lift_platform_id_ = track(platform->GetID());

    JPH::Body *counterweight = add_shape_body(
        bodies, new JPH::BoxShape(JPH::Vec3(0.5F, 0.9F, 0.5F)),
        JPH::RVec3(kCounterweightX, 7.4, kLiftZ), JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic, object_layers::kMoving, 0.6F,
        Simulation::kCounterweightEntityId, kCounterweightMassKg);
    counterweight_id_ = track(counterweight->GetID());

    add_hinge(machine_pylon, tipper_id_, JPH::RVec3(kTipperHingeX, kTipperHingeY, kTipperZ),
              -0.42F, 0.06F, &tipper_hinge_, Simulation::kMachinePylonEntityId);
    add_hinge(valve_pylon, valve_lever_id_, JPH::RVec3(kValveHingeX, kValveHingeY, -93.0),
              kValveShutAngle, 0.95F, &valve_hinge_, Simulation::kMachinePylonEntityId);

    add_slider(lift_mast, lift_platform_id_, 0.0F, kLiftTravelMeters);
    add_slider(lift_mast, counterweight_id_, -kLiftTravelMeters, 0.0F);
    // -1 max length is unchanged from before add_pulley took explicit
    // points: Jolt auto-derives it from this exact as-built configuration
    // (PulleyConstraint.cpp), so behaviour here is byte-identical.
    (void)add_pulley(JPH::RVec3(kLiftPlatformX, kLiftPlatformRestY + 0.16, kLiftZ),
                    JPH::RVec3(kLiftPlatformX, kSheaveY, kLiftZ), lift_platform_id_,
                    JPH::RVec3(kCounterweightX, 8.3, kLiftZ),
                    JPH::RVec3(kCounterweightX, kSheaveY, kLiftZ), counterweight_id_, 1.0F, 0.0F,
                    -1.0F);
    settle_machine();
    add_rope();
    add_treadle_cable();
}

void Simulation::PhysicsWorld::build_kernel_jib(JPH::BodyInterface &bodies) {
    const auto track = [this](const JPH::BodyID id) {
        machine_bodies_.push_back(id);
        return id;
    };

    // KX-DECK kernel patch: its own bounded apron, not the Kellerworks
    // yard -- Ascent Atlas section 9 places the kernel at a separate,
    // compressed scale.
    track(add_box(bodies, JPH::Vec3(kKernelDeckHalfExtent, 0.3F, kKernelDeckHalfExtent),
                  JPH::RVec3(kKernelBaseX, -0.3, kKernelBaseZ),
                  JPH::EMotionType::Static, object_layers::kStatic, 0.9F,
                  Simulation::kStaticDeckEntityId));

    const JPH::BodyID jib_mast = track(add_box(
        bodies, JPH::Vec3(0.35F, kJibMastHeight * 0.5F, 0.35F),
        JPH::RVec3(kJibMastX, kJibMastHeight * 0.5F, kJibMastZ),
        JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
        Simulation::kJibMastEntityId));

    JPH::Body *boom = add_shape_body(
        bodies, new JPH::BoxShape(JPH::Vec3(kJibBoomLength * 0.5F, 0.15F, 0.15F)),
        JPH::RVec3(kJibMastX + kJibBoomLength * 0.5F, kJibMastHeight, kJibMastZ),
        JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, object_layers::kMoving, 0.5F,
        Simulation::kJibBoomEntityId, kJibBoomMassKg);
    jib_boom_id_ = track(boom->GetID());

    add_vertical_hinge(jib_mast, jib_boom_id_,
                       JPH::RVec3(kJibMastX, kJibMastHeight, kJibMastZ),
                       -kJibSlewLimitRadians, kJibSlewLimitRadians, kJibSlewMaxTorqueNm,
                       &jib_slew_hinge_);

    // Hook and crate both start near the deck: the crate is where a real
    // load actually sits, and the hook is pre-rigged to it (WO-005 allows
    // this -- "CAP-HOOK5 may be pre-placed on the crate for this WO").
    // Raising is what "picks it up"; nothing snaps into a solved pose.
    const float crate_start_y = kCrateHalfExtent;
    const float link_y = crate_start_y + kCrateHalfExtent;
    const float hook_start_y = link_y + 0.15F;
    JPH::Body *crate = add_shape_body(
        bodies,
        new JPH::BoxShape(JPH::Vec3(kCrateHalfExtent, kCrateHalfExtent, kCrateHalfExtent)),
        JPH::RVec3(kJibMastX + kJibBoomLength, crate_start_y, kJibMastZ),
        JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, object_layers::kMoving, 0.6F,
        Simulation::kCrateEntityId, kCrateMassKg);
    crate_id_ = track(crate->GetID());

    JPH::Body *hook = add_shape_body(
        bodies, new JPH::BoxShape(JPH::Vec3(0.15F, 0.15F, 0.15F)),
        JPH::RVec3(kJibMastX + kJibBoomLength, hook_start_y, kJibMastZ), JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic, object_layers::kMoving, 0.4F,
        Simulation::kJibHookEntityId, kJibHookMassKg);
    jib_hook_id_ = track(hook->GetID());

    add_point_link(jib_hook_id_, crate_id_,
                  JPH::RVec3(kJibMastX + kJibBoomLength, link_y, kJibMastZ));

    // Travel is signed from this starting (lowest) pose, matching every
    // other slider in this file (lift platform, counterweight): 0 here,
    // upward-only, so raising is unambiguously the positive direction.
    const float hoist_travel = (kJibMastHeight - 0.35F) - hook_start_y;
    add_motorized_slider(jib_boom_id_, jib_hook_id_, 0.0F, hoist_travel, kJibMaxLiftForceN,
                         &jib_hoist_slider_);
    jib_hoist_slider_->GetMotorSettings().mSpringSettings = JPH::SpringSettings(
        JPH::ESpringMode::StiffnessAndDamping,
        kJibHoldStiffnessNPerMeter, kJibHoldDampingNsPerMeter);

    // Capacity-proving stand: fixed, no slew, permanently overweight,
    // always commanded to raise. Proves the rated force is real without
    // staging that failure as an unsafe lift on the working jib (WO-005
    // forbidden shortcuts: "unlimited winch force").
    const JPH::BodyID stand_mast = track(add_box(
        bodies, JPH::Vec3(0.3F, kCapacityStandMastHeight * 0.5F, 0.3F),
        JPH::RVec3(kCapacityStandX, kCapacityStandMastHeight * 0.5F, kCapacityStandZ),
        JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
        Simulation::kJibMastEntityId));
    JPH::Body *stand_load = add_shape_body(
        bodies,
        new JPH::BoxShape(JPH::Vec3(kCapacityStandLoadHalfExtent, kCapacityStandLoadHalfExtent,
                                    kCapacityStandLoadHalfExtent)),
        JPH::RVec3(kCapacityStandX, kCapacityStandLoadHalfExtent, kCapacityStandZ),
        JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, object_layers::kMoving, 0.6F,
        Simulation::kCapacityStandEntityId, kCapacityStandLoadMassKg);
    capacity_stand_load_id_ = track(stand_load->GetID());
    JPH::Ref<JPH::SliderConstraint> stand_slider;
    const float stand_travel = kCapacityStandMastHeight - kCapacityStandLoadHalfExtent -
                                kCapacityStandLoadHalfExtent;
    add_motorized_slider(stand_mast, capacity_stand_load_id_, 0.0F, stand_travel,
                         kJibMaxLiftForceN, &stand_slider);
    stand_slider->SetTargetVelocity(kJibHoistMaxRateMetersPerSec);
}

void Simulation::PhysicsWorld::build_kernel_needle(JPH::BodyInterface &bodies) {
    const auto track = [this](const JPH::BodyID id) {
        machine_bodies_.push_back(id);
        return id;
    };

    // Each pier is two boxes, not one: a full-height main block, and a
    // shorter notch block under the overlap strip where the beam's end
    // actually lands. Seating the beam flush with the pier top (see
    // kNeedleSeatedY) means its underside has to have somewhere to go
    // that isn't solid pier -- a real pocket is a recess, not a shelf.
    //
    // The notch's boundary against the main block is pulled back by
    // kNeedlePocketMarginMeters, well clear of the beam's own edge --
    // found necessary by direct observation: siting that boundary exactly
    // at the beam's edge left the descending beam stopping ~4 cm short of
    // its seat, against JPH::BoxShape's default rounded convex radius on
    // the main block's corner, not the notch it was actually meant to
    // land on.
    const auto box_from_span = [](const float min_x, const float max_x) {
        return std::pair<float, float>{(min_x + max_x) * 0.5F, (max_x - min_x) * 0.5F};
    };
    const float pier_top_y_half = kNeedlePierHeight * 0.5F;
    const float notch_top_y_half = kNeedlePocketNotchTopY * 0.5F;

    const float approach_outer_x = kNeedlePierApproachX + kNeedlePierHalfExtentX;
    const float approach_inner_x = kNeedlePierApproachX - kNeedlePierHalfExtentX;
    const float approach_notch_boundary_x = kNeedlePocketApproachX - kNeedlePocketMarginMeters;
    const auto [approach_main_x, approach_main_half_x] =
        box_from_span(approach_inner_x, approach_notch_boundary_x);
    const auto [approach_notch_x, approach_notch_half_x] =
        box_from_span(approach_notch_boundary_x, approach_outer_x);

    track(add_box(bodies, JPH::Vec3(approach_main_half_x, pier_top_y_half, kNeedlePierHalfExtentZ),
                  JPH::RVec3(approach_main_x, pier_top_y_half, kNeedleGapCenterZ),
                  JPH::EMotionType::Static, object_layers::kStatic, 0.9F,
                  Simulation::kNeedlePierApproachEntityId));
    needle_pier_approach_id_ = track(add_box(
        bodies, JPH::Vec3(approach_notch_half_x, notch_top_y_half, kNeedlePierHalfExtentZ),
        JPH::RVec3(approach_notch_x, notch_top_y_half, kNeedleGapCenterZ),
        JPH::EMotionType::Static, object_layers::kStatic, 0.9F,
        Simulation::kNeedlePierApproachEntityId));

    const float far_outer_x = kNeedlePierFarX + kNeedlePierHalfExtentX;
    const float far_inner_x = kNeedlePierFarX - kNeedlePierHalfExtentX;
    const float far_notch_boundary_x = kNeedlePocketFarX + kNeedlePocketMarginMeters;
    const auto [far_main_x, far_main_half_x] = box_from_span(far_notch_boundary_x, far_outer_x);
    const auto [far_notch_x, far_notch_half_x] = box_from_span(far_inner_x, far_notch_boundary_x);

    track(add_box(bodies, JPH::Vec3(far_main_half_x, pier_top_y_half, kNeedlePierHalfExtentZ),
                  JPH::RVec3(far_main_x, pier_top_y_half, kNeedleGapCenterZ),
                  JPH::EMotionType::Static, object_layers::kStatic, 0.9F,
                  Simulation::kNeedlePierFarEntityId));
    needle_pier_far_id_ = track(add_box(
        bodies, JPH::Vec3(far_notch_half_x, notch_top_y_half, kNeedlePierHalfExtentZ),
        JPH::RVec3(far_notch_x, notch_top_y_half, kNeedleGapCenterZ),
        JPH::EMotionType::Static, object_layers::kStatic, 0.9F,
        Simulation::kNeedlePierFarEntityId));

    // Mast (visual + mass) and a small fixed head at the top, directly
    // above the gap centreline -- the head is the actual slider anchor,
    // the mast beneath it is proof scaffolding like the capacity stand's,
    // not a load-bearing member of the kernel chain.
    track(add_box(bodies, JPH::Vec3(0.35F, kNeedleHoistMastHeight * 0.5F, 0.35F),
                  JPH::RVec3(kNeedleGapCenterX, kNeedleHoistMastHeight * 0.5F,
                            kNeedleGapCenterZ + kNeedlePierHalfExtentZ + 1.0F),
                  JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                  Simulation::kNeedleHoistMastEntityId));
    const JPH::BodyID needle_head = track(add_box(
        bodies, JPH::Vec3(0.4F, 0.2F, 0.4F),
        JPH::RVec3(kNeedleGapCenterX, kNeedleHoistMastHeight, kNeedleGapCenterZ),
        JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
        Simulation::kNeedleHoistMastEntityId));

    JPH::Body *beam = add_shape_body(
        bodies,
        new JPH::BoxShape(
            JPH::Vec3(kNeedleBeamHalfLength, kNeedleBeamHalfHeight, kNeedleBeamHalfWidth)),
        JPH::RVec3(kNeedleGapCenterX, kNeedleStowedY, kNeedleGapCenterZ), JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic, object_layers::kMoving, 0.7F,
        Simulation::kNeedleBeamEntityId, kNeedleBeamMassKg);
    needle_beam_id_ = track(beam->GetID());

    // Travel is signed from the spawn (stowed, top) pose: 0 here, and
    // downward-only to the hard-limited seat height -- "reaches the
    // bottom of travel" and "reaches the seat" are the same event.
    const float travel_down = kNeedleStowedY - kNeedleSeatedY;
    add_motorized_slider(needle_head, needle_beam_id_, -travel_down, 0.0F, kNeedleMaxLiftForceN,
                         &needle_hoist_slider_);
}

void Simulation::PhysicsWorld::settle_machine() noexcept {
    for (int step = 0; step < 90; ++step) {
        physics_system_.Update(static_cast<float>(Simulation::kFixedStepSeconds), 1,
                               &temp_allocator_, &job_system_);
    }
}

void Simulation::PhysicsWorld::add_hinge(const JPH::BodyID anchor_id,
               const JPH::BodyID moving_id,
               const JPH::RVec3 point,
               const float limit_min,
               const float limit_max,
               JPH::Ref<JPH::HingeConstraint> *out,
               const std::uint64_t) {
    JPH::HingeConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mPoint1 = point;
    settings.mPoint2 = point;
    settings.mHingeAxis1 = JPH::Vec3::sAxisZ();
    settings.mHingeAxis2 = JPH::Vec3::sAxisZ();
    settings.mNormalAxis1 = JPH::Vec3::sAxisX();
    settings.mNormalAxis2 = JPH::Vec3::sAxisX();
    settings.mLimitsMin = limit_min;
    settings.mLimitsMax = limit_max;
    JPH::TwoBodyConstraint *constraint = create_constraint(settings, anchor_id, moving_id);
    if (constraint != nullptr && out != nullptr) {
        *out = static_cast<JPH::HingeConstraint *>(constraint);
    }
}

void Simulation::PhysicsWorld::add_slider(const JPH::BodyID anchor_id,
                const JPH::BodyID moving_id,
                const float limit_min,
                const float limit_max) {
    JPH::SliderConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mAutoDetectPoint = true;
    settings.SetSliderAxis(JPH::Vec3::sAxisY());
    settings.mLimitsMin = limit_min;
    settings.mLimitsMax = limit_max;
    (void)create_constraint(settings, anchor_id, moving_id);
}

void Simulation::PhysicsWorld::add_vertical_hinge(const JPH::BodyID anchor_id,
                        const JPH::BodyID moving_id,
                        const JPH::RVec3 point,
                        const float limit_min,
                        const float limit_max,
                        const float max_motor_torque_nm,
                        JPH::Ref<JPH::HingeConstraint> *out,
                        const JPH::Vec3 axis) {
    JPH::HingeConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mPoint1 = point;
    settings.mPoint2 = point;
    settings.mHingeAxis1 = axis;
    settings.mHingeAxis2 = axis;
    settings.mNormalAxis1 = JPH::Vec3::sAxisX();
    settings.mNormalAxis2 = JPH::Vec3::sAxisX();
    settings.mLimitsMin = limit_min;
    settings.mLimitsMax = limit_max;
    settings.mMotorSettings.SetTorqueLimit(max_motor_torque_nm);
    JPH::TwoBodyConstraint *constraint = create_constraint(settings, anchor_id, moving_id);
    if (constraint == nullptr || out == nullptr) {
        return;
    }
    auto *hinge = static_cast<JPH::HingeConstraint *>(constraint);
    hinge->SetMotorState(JPH::EMotorState::Velocity);
    hinge->SetTargetAngularVelocity(0.0F);
    *out = hinge;
}

void Simulation::PhysicsWorld::add_motorized_slider(const JPH::BodyID anchor_id,
                          const JPH::BodyID moving_id,
                          const float limit_min,
                          const float limit_max,
                          const float max_motor_force_n,
                          JPH::Ref<JPH::SliderConstraint> *out) {
    JPH::SliderConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mAutoDetectPoint = true;
    settings.SetSliderAxis(JPH::Vec3::sAxisY());
    settings.mLimitsMin = limit_min;
    settings.mLimitsMax = limit_max;
    settings.mMotorSettings.SetForceLimit(max_motor_force_n);
    JPH::TwoBodyConstraint *constraint = create_constraint(settings, anchor_id, moving_id);
    if (constraint == nullptr || out == nullptr) {
        return;
    }
    auto *slider = static_cast<JPH::SliderConstraint *>(constraint);
    slider->SetMotorState(JPH::EMotorState::Velocity);
    slider->SetTargetVelocity(0.0F);
    *out = slider;
}

void Simulation::PhysicsWorld::add_point_link(const JPH::BodyID first_id,
                    const JPH::BodyID second_id,
                    const JPH::RVec3 point) {
    JPH::PointConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mPoint1 = point;
    settings.mPoint2 = point;
    (void)create_constraint(settings, first_id, second_id);
}

void Simulation::PhysicsWorld::add_rope() {
    const auto &bodies = physics_system_.GetBodyInterface();
    const JPH::RVec3 tipper_point =
        bodies.GetCenterOfMassTransform(tipper_id_) * JPH::RVec3(3.0, -0.2, 0.0);
    const JPH::RVec3 lever_point =
        bodies.GetCenterOfMassTransform(valve_lever_id_) * JPH::RVec3(-1.60, 0.0, 0.0);
    rope_rest_length_ = JPH::Vec3(lever_point - tipper_point).Length();

    JPH::DistanceConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mPoint1 = tipper_point;
    settings.mPoint2 = lever_point;
    settings.mMinDistance = 0.0F;
    settings.mMaxDistance = rope_rest_length_ + kRopeSlackMeters;
    (void)create_constraint(settings, tipper_id_, valve_lever_id_);
}

void Simulation::PhysicsWorld::add_treadle_cable() {
    JPH::PulleyConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mBodyPoint1 =
        JPH::RVec3(kTreadleX - kTreadleCableArm, kTreadleHingeY, kTreadleZ);
    settings.mFixedPoint1 = JPH::RVec3(kTreadleX - kTreadleCableArm,
                                       kTreadleHingeY + kTreadleSheaveRise, kTreadleMastZ);
    settings.mBodyPoint2 = JPH::RVec3(kValveHingeX + kValveCableArm, kValveHingeY + 0.2, -93.0);
    settings.mFixedPoint2 = JPH::RVec3(kValveHingeX + kValveCableArm, kValveSheaveY, kValveMastZ);
    settings.mRatio = 1.0F;
    settings.mMinLength = 0.0F;
    settings.mMaxLength = -1.0F;
    (void)create_constraint(settings, treadle_id_, valve_lever_id_);
}

JPH::PulleyConstraint *Simulation::PhysicsWorld::add_pulley(const JPH::RVec3 body_point_1,
                                  const JPH::RVec3 fixed_point_1,
                                  const JPH::BodyID body_1,
                                  const JPH::RVec3 body_point_2,
                                  const JPH::RVec3 fixed_point_2,
                                  const JPH::BodyID body_2,
                                  const float ratio,
                                  const float min_length,
                                  const float max_length) {
    JPH::PulleyConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mBodyPoint1 = body_point_1;
    settings.mFixedPoint1 = fixed_point_1;
    settings.mBodyPoint2 = body_point_2;
    settings.mFixedPoint2 = fixed_point_2;
    settings.mRatio = ratio;
    settings.mMinLength = min_length;
    settings.mMaxLength = max_length;
    return static_cast<JPH::PulleyConstraint *>(create_constraint(settings, body_1, body_2));
}

[[nodiscard]] JPH::TwoBodyConstraint *Simulation::PhysicsWorld::create_constraint(
    const JPH::TwoBodyConstraintSettings &settings,
    const JPH::BodyID first,
    const JPH::BodyID second,
    const bool track_for_teardown) {
    // Jolt stripes body mutexes across a fixed-size array, so two distinct
    // bodies can share one. Taking two separate BodyLockWrite locks then
    // deadlocks on a non-recursive mutex ("Resource deadlock avoided").
    // BodyLockMultiWrite sorts and dedupes the mutexes, which is exactly
    // what it exists for. Every constraint pair built before this simply
    // happened not to collide.
    const JPH::BodyID ids[2] = {first, second};
    JPH::BodyLockMultiWrite lock(physics_system_.GetBodyLockInterface(), ids, 2);
    JPH::Body *first_body = lock.GetBody(0);
    JPH::Body *second_body = lock.GetBody(1);
    if (first_body == nullptr || second_body == nullptr) {
        return nullptr;
    }
    JPH::TwoBodyConstraint *constraint = settings.Create(*first_body, *second_body);
    if (constraint == nullptr) {
        return nullptr;
    }
    physics_system_.AddConstraint(constraint);
    if (track_for_teardown) {
        machine_constraints_.emplace_back(constraint);
    }
    return constraint;
}

} // namespace scraperx::sim
