#include "sim/simulation/world.hpp"
#include <stdexcept>
#include "sim/rigging_bay.hpp"
#include "sim/mounted_winch_bay.hpp"
#include "sim/workshop_roof_route.hpp"
#include "sim/vault_rail_route.hpp"
#include "sim/loose_gangway_route.hpp"
#include "sim/beam_ramp_route.hpp"

namespace scraperx::sim {
using namespace detail;

Simulation::PhysicsWorld::PhysicsWorld(const InitialSpawn initial_spawn, const WorldContent content)
    : temp_allocator_(8U * 1024U * 1024U),
      job_system_(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 1),
      regression_fixtures_(content == WorldContent::RegressionFixtures) {
    contact_listener_.set_landing_feedback(content == WorldContent::Slingshot);
    // Raised from 1024: the world's solid dressing adds ~1 600 static
    // bodies (world_solids.inc) on top of the ~300 the kernel owns.
    physics_system_.Init(4096,
                         0,
                         2048,
                         1024,
                         broadphase_layer_interface_,
                         object_vs_broadphase_filter_,
                         object_layer_pair_filter_);
    physics_system_.SetContactListener(&contact_listener_);

    auto &bodies = physics_system_.GetBodyInterface();

    // Grade: the yard the player is born on, outdoors, wide enough to walk
    // the tower approach and to carry the whole plant.
    deck_id_ = add_box(bodies,
                       JPH::Vec3(240.0F, 0.5F, 240.0F),
                       JPH::RVec3(0.0, -0.5, -60.0),
                       JPH::EMotionType::Static,
                       object_layers::kStatic,
                       0.6F,
                       Simulation::kStaticDeckEntityId);

    // The neighbouring shaft: 1.6 km of unclimbable mass standing on
    // grade, set well back from the climbable frame. Overhead and close,
    // it read as a black void hanging over the stack; begun 159 m up
    // with nothing under it, it read as a skyscraper floating in the air.
    const float mass_half_height = static_cast<float>(Simulation::kTowerHeightMeters * 0.5);
    tower_id_ = add_box(bodies,
                        JPH::Vec3(46.0F, mass_half_height, 40.0F),
                        JPH::RVec3(-30.0, mass_half_height, -330.0),
                        JPH::EMotionType::Static,
                        object_layers::kStatic,
                        0.8F,
                        Simulation::kTowerEntityId);

    if (regression_fixtures_) {
        translating_support_id_ = add_box(bodies,
                                          JPH::Vec3(2.75F, 0.25F, 2.75F),
                                          JPH::RVec3(0.0, 0.25, 8.0),
                                          JPH::EMotionType::Kinematic,
                                          object_layers::kMoving,
                                          0.8F,
                                          Simulation::kTranslatingSupportEntityId);

        rotating_support_id_ = add_box(bodies,
                                       JPH::Vec3(3.0F, 0.25F, 3.0F),
                                       JPH::RVec3(-8.0, 0.25, 0.0),
                                       JPH::EMotionType::Kinematic,
                                       object_layers::kMoving,
                                       0.8F,
                                       Simulation::kRotatingSupportEntityId);

        vault_rail_id_ = add_box(bodies,
                                 JPH::Vec3(0.22F, 0.475F, 2.5F),
                                 JPH::RVec3(5.0, 0.475, -6.0),
                                 JPH::EMotionType::Static,
                                 object_layers::kStatic,
                                 0.7F,
                                 Simulation::kVaultRailEntityId);

        mantle_ledge_id_ = add_box(bodies,
                                   JPH::Vec3(2.0F, 0.775F, 2.0F),
                                   JPH::RVec3(11.0, 0.775, -6.0),
                                   JPH::EMotionType::Static,
                                   object_layers::kStatic,
                                   0.7F,
                                   Simulation::kMantleLedgeEntityId);

        hang_ledge_id_ = add_box(bodies,
                                 JPH::Vec3(2.5F, 1.8F, 2.5F),
                                 JPH::RVec3(11.0, 1.8, 4.0),
                                 JPH::EMotionType::Static,
                                 object_layers::kStatic,
                                 0.7F,
                                 Simulation::kHangLedgeEntityId);

        moving_ledge_id_ = add_box(bodies,
                                   JPH::Vec3(2.0F, 1.8F, 2.0F),
                                   JPH::RVec3(9.0, 1.8, kMovingLedgeCenterZ),
                                   JPH::EMotionType::Kinematic,
                                   object_layers::kMoving,
                                   0.8F,
                                   Simulation::kMovingLedgeEntityId);

        blocked_ledge_id_ = add_box(bodies,
                                    JPH::Vec3(1.5F, 0.775F, 1.5F),
                                    JPH::RVec3(-6.0, 0.775, -8.0),
                                    JPH::EMotionType::Static,
                                    object_layers::kStatic,
                                    0.7F,
                                    Simulation::kBlockedLedgeEntityId);

        blocked_ledge_canopy_id_ = add_box(bodies,
                                           JPH::Vec3(2.2F, 0.15F, 2.2F),
                                           JPH::RVec3(-6.0, 2.7, -8.0),
                                           JPH::EMotionType::Static,
                                           object_layers::kStatic,
                                           0.7F,
                                           Simulation::kBlockedLedgeCanopyEntityId);

        {
            const auto crawl_part = [this, &bodies](const JPH::Vec3 half, const JPH::RVec3 at) {
                machine_bodies_.push_back(add_box(bodies, half, at, JPH::EMotionType::Static,
                                                  object_layers::kStatic, 0.7F,
                                                  Simulation::kCrawlBeamEntityId));
            };
            const float beam_top = kCrawlBeamUndersideY + 2.0F * kCrawlBeamHalfY;
            crawl_part(JPH::Vec3(kCrawlLaneHalfX, kCrawlBeamHalfY, kCrawlBeamHalfZ),
                       JPH::RVec3(kCrawlLaneCenterX, kCrawlBeamUndersideY + kCrawlBeamHalfY,
                                  kCrawlBeamZ));
            for (const float side : {-1.0F, 1.0F}) {
                crawl_part(JPH::Vec3(kCrawlPostHalf, beam_top * 0.5F, kCrawlPostHalf),
                           JPH::RVec3(kCrawlLaneCenterX + side * (kCrawlLaneHalfX + kCrawlPostHalf),
                                      beam_top * 0.5F, kCrawlBeamZ));
            }
        }

    }

    build_stack(bodies);
    if (regression_fixtures_) {
        build_machine(bodies);
        build_kernel_jib(bodies);
        build_kernel_needle(bodies);
        build_kernel_sump(bodies);
        build_intake_rise(bodies);
        build_water_screw(bodies);
        build_legal_forty(bodies);
    }

    build_world_solids(bodies);

    player_shape_ = new JPH::CapsuleShape(0.55F, kPlayerRadius);
    player_crouch_shape_ =
        new JPH::CapsuleShape(kPlayerCrouchHalfHeight - kPlayerRadius, kPlayerRadius);
    grip_region_shape_ = new JPH::BoxShape(kGripSearchHalf);
    // A climber's moves are swept with a body 5 cm slimmer than the
    // capsule: a body on its holds already touches the rungs it climbs.
    climb_sweep_shape_ =
        new JPH::CapsuleShape(0.55F + kPlayerRadius - kClimbSweepRadius, kClimbSweepRadius);
    JPH::BodyCreationSettings player_settings(player_shape_,
                                              spawn_position(initial_spawn),
                                              JPH::Quat::sIdentity(),
                                              JPH::EMotionType::Dynamic,
                                              object_layers::kMoving);
    player_settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX |
                                   JPH::EAllowedDOFs::TranslationY |
                                   JPH::EAllowedDOFs::TranslationZ;
    player_settings.mAllowSleeping = false;
    player_settings.mFriction = 0.0F;
    player_settings.mLinearDamping = 0.0F;
    player_settings.mUserData = Simulation::kPlayerEntityId;
    player_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    player_settings.mMassPropertiesOverride.mMass = kPlayerMassKg;
    if (content == WorldContent::Slingshot)
        player_settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
    player_id_ = bodies.CreateAndAddBody(player_settings, JPH::EActivation::Activate);
    soft_foot_policy_.player = player_id_;

    // Built last, so every body before it keeps the id it had: the ascent
    // routes proven against them are contact-order sensitive.
    if (regression_fixtures_) { build_hook5_rack(bodies); }

    // AS-006: the mechanism ascent's bands, built last for the same
    // reason: every body before them keeps its id.
    kit_ = std::make_unique<scraperx::sim::kit::Kit>(physics_system_, object_layers::kStatic,
                                                    object_layers::kMoving);
    if (regression_fixtures_) {
        scraperx::sim::bands::build_ground_water_lift(*kit_, ground_water_lift_);
        scraperx::sim::bands::build_counterweight_well(*kit_, well_);

    }

    if (content == WorldContent::PipeBridge || content == WorldContent::Slingshot) {
        if (content == WorldContent::PipeBridge)
            pipe_bridge_ = std::make_unique<PipeBridge>(physics_system_, *kit_);
        else
            slingshot_ = std::make_unique<Slingshot>(physics_system_, *kit_, player_id_, true);
        build_facade_route(*kit_);
        swing_stair_ = std::make_unique<SwingStair>(physics_system_, *kit_);
        upper_ascent_ = std::make_unique<UpperAscent>(physics_system_, *kit_);
        teeter_rise_ = std::make_unique<TeeterRise>(*kit_);
        build_braced_bay(*kit_);
        build_north_service_frame(*kit_);
        // Append only to normal gameplay, preserving retained fixture body order.
        if (content == WorldContent::Slingshot) {
            const auto frame=build_cargo_net_route(*kit_);
            cargo_net_ = std::make_unique<CargoNet>(physics_system_, object_layers::kMoving,kit_->body_id(frame));
            cargo_net_->set_contact_friction_callback(&soft_foot_contact_friction, &soft_foot_policy_);
            build_suspended_ladder(*kit_);
            service_lift_ = std::make_unique<ServiceLift>(physics_system_, *kit_);
            supplied_ascent_ = std::make_unique<SuppliedAscent>(physics_system_, *kit_);
            build_parkour_route(*kit_);
            supplied_ascent_->append_gravity_wheel(physics_system_,*kit_);
            supplied_ascent_->append_gravity_cart(physics_system_,*kit_);
            build_taper_inspection_route(*kit_);
            build_west_brace_bay_route(*kit_);
            build_inspection_junction_route(*kit_);
            deformable_plank_ = std::make_unique<DeformablePlank>(physics_system_, *kit_,
                JPH::RVec3(-8.0F, 406.981F, -128.2F), DeformablePlank::Support::GuidedPinRoller);
            deformable_plank_->enable_strength_failure();
            // New north transfer appends after existing owners, preserving
            // their native body order and checkpoint bindings.
            build_north_transfer_route(*kit_);
            build_pipe_service_route(*kit_);
            contact_listener_.set_reclaim_fragments(supplied_ascent_->reclaim_material_range());
        }
    }

    // Append after retained bodies, preserving every existing contact ID.
    if (cargo_net_) {
        const auto frame = build_catch_net_route(*kit_);
        catch_net_ = std::make_unique<CargoNet>(physics_system_, object_layers::kMoving,
            kit_->body_id(frame), catch_net_config(*kit_));
        catch_net_->set_contact_friction_callback(&soft_foot_contact_friction, &soft_foot_policy_);
    }
    // New inspection crossing appends without renumbering retained bodies.
    if (content == WorldContent::Slingshot) {
        build_split_inspection_route(*kit_);
        (void)build_grapple_practice_route(*kit_);
        forklift_yard_ = build_warehouse_forklift_yard(*kit_);
        forklift_yard_.truck.wheel_layer = object_layers::kMoving;
        warehouse_forklift_ = std::make_unique<WarehouseForklift>(physics_system_, *kit_, forklift_yard_.truck);
        yard_crane_ = std::make_unique<YardCrane>(physics_system_, *kit_, forklift_yard_.truck);
    }
    soft_contact_listener_.set_player(player_id_);
    physics_system_.SetSoftBodyContactListener(&soft_contact_listener_);
    physical_hands_ = std::make_unique<PhysicalHandClimb>(physics_system_, player_id_, cargo_net_.get());
    physical_foot_push_ = std::make_unique<PhysicalFootPush>(physics_system_, player_id_);
    for (auto &palm : physical_palms_)
        palm = std::make_unique<PhysicalHandProp>(physics_system_, player_id_,
            // Keep the standing shoulder-to-crown distance in the compact
            // torso pose. Arm length and every gripping hand law stay fixed.
            kClimbHandMid-kCrouchDrop, kTransferShoulderReachM);
    if (content == WorldContent::Slingshot) {
        grappling_gun_ = std::make_unique<GrapplingGun>(physics_system_, player_id_, object_layers::kMoving);
        // Starting equipment is carried throughout the world. The roof rack
        // remains a practice station, not a prerequisite for drawing the gun.
        grapple_acquired_ = true;
        // Append after all retained physics bodies, including the hook.
        build_workshop_roof_route(*kit_);
        build_vault_rail_route(*kit_);
        build_ground_swing_route(*kit_);
        build_loose_gangway_route(*kit_);
        build_beam_ramp_route(*kit_);
        rigging_ = std::make_unique<RiggingSystem>(physics_system_, *kit_);
        build_rigging_bay(*kit_, *rigging_);
        build_mounted_winch_bay(*kit_, *rigging_);
        traversal_line_ = std::make_unique<PortableTraversalLine>(physics_system_, player_id_,
            object_layers::kMoving, *physical_hands_, rigging_.get());
    }
    if (content == WorldContent::Slingshot) {
        story_actor_ = std::make_unique<PhysicalStoryActor>(physics_system_,
            object_layers::kMoving, Simulation::kStoryActorEntityId);
        if (!story_actor_->spawn(JPH::RVec3(-26.75, 253.9, -143.2)))
            throw std::runtime_error("Unable to create native story actor on barrel exit");
        // The workbench is real static steel, shared by native collision and
        // the existing Kit renderer. Its seam is within the worker's reach.
        std::vector<kit::Part> bench;
        bench.push_back({JPH::Vec3(.6F,.06F,.3F), JPH::Vec3(0,.75F,0),
            JPH::Quat::sIdentity(), kit::Material::Steel});
        for (float x : {-.53F,.53F}) for (float z : {-.23F,.23F})
            bench.push_back({JPH::Vec3(.025F,.345F,.025F), JPH::Vec3(x,.345F,z),
                JPH::Quat::sIdentity(), kit::Material::Steel});
        bench.push_back({JPH::Vec3(.28F,.015F,.09F), JPH::Vec3(0,.825F,.17F),
            JPH::Quat::sIdentity(), kit::Material::Steel});
        (void)kit_->add_body(Simulation::kYardWorkerBenchEntityId, bench,
            YardWorker::home() + JPH::Vec3(0,-.9F,-.85F), JPH::Quat::sIdentity(), 0, .8F);
        yard_worker_ = std::make_unique<YardWorker>(physics_system_,
            object_layers::kMoving, Simulation::kYardWorkerEntityId);
        if (!yard_worker_->spawn())
            throw std::runtime_error("Unable to create native yard worker beside arrival");
    }
    if (slingshot_) physics_system_.AddStepListener(this);
    contact_listener_.set_slingshot(slingshot_.get());
    physics_system_.OptimizeBroadPhase();

    clear_catch_history();
    checkpoint_position_ = JPH::RVec3(0.0, 0.9, 0.0);
    commit_machine_checkpoint(bodies);

    read_state();
}

Simulation::PhysicsWorld::~PhysicsWorld() {
    contact_listener_.set_yard_worker(JPH::BodyID(), 0);
    yard_worker_.reset();
    contact_listener_.set_story_actor(JPH::BodyID(), 0);
    story_actor_.reset();
    if (slingshot_) physics_system_.RemoveStepListener(this);
    grappling_gun_.reset();
    traversal_line_.reset();
    if (cargo_net_) cargo_net_->set_contact_friction_callback(nullptr, nullptr);
    if (catch_net_) catch_net_->set_contact_friction_callback(nullptr, nullptr);
    physical_foot_push_.reset();
    for (auto &palm : physical_palms_) palm.reset();
    physical_hands_.reset();
    physics_system_.SetContactListener(nullptr);
    physics_system_.SetSoftBodyContactListener(nullptr);
    // Dynamically-owned pins (track_for_teardown=false) are never in
    // machine_constraints_, so the loop below cannot reach them -- remove
    // whichever of them are currently seated before it runs.
    if (needle_pin_approach_ != nullptr) {
        physics_system_.RemoveConstraint(needle_pin_approach_);
        needle_pin_approach_ = nullptr;
    }
    if (needle_pin_far_ != nullptr) {
        physics_system_.RemoveConstraint(needle_pin_far_);
        needle_pin_far_ = nullptr;
    }
    if (intake_sling_pin_ != nullptr) {
        physics_system_.RemoveConstraint(intake_sling_pin_);
        intake_sling_pin_ = nullptr;
    }
    if (carry_constraint_ != nullptr) {
        physics_system_.RemoveConstraint(carry_constraint_);
        carry_constraint_ = nullptr;
    }
    catch_net_.reset();
    cargo_net_.reset();
    upper_ascent_.reset();
    swing_stair_.reset();
    pipe_bridge_.reset();
    slingshot_.reset();
    supplied_ascent_.reset();
    contact_listener_.set_reclaim_fragments({0,0});
    service_lift_.reset();
    yard_crane_.reset();
    warehouse_forklift_.reset();
    deformable_plank_.reset();
    rigging_.reset();
    kit_.reset();
    for (JPH::Ref<JPH::TwoBodyConstraint> &constraint : machine_constraints_) {
        if (constraint != nullptr) {
            physics_system_.RemoveConstraint(constraint);
        }
    }
    machine_constraints_.clear();
    tipper_hinge_ = nullptr;
    valve_hinge_ = nullptr;
    treadle_hinge_ = nullptr;
    jib_slew_hinge_ = nullptr;
    jib_hoist_slider_ = nullptr;
    needle_hoist_slider_ = nullptr;
    intake_swing_hinge_ = nullptr;
    hook5_door_hinge_ = nullptr;
    auto &bodies = physics_system_.GetBodyInterface();
    for (auto it = machine_bodies_.rbegin(); it != machine_bodies_.rend(); ++it) {
        remove_and_destroy(bodies, *it);
    }
    machine_bodies_.clear();
    remove_and_destroy(bodies, tower_id_);
    remove_and_destroy(bodies, player_id_);
    remove_and_destroy(bodies, blocked_ledge_canopy_id_);
    remove_and_destroy(bodies, blocked_ledge_id_);
    remove_and_destroy(bodies, moving_ledge_id_);
    remove_and_destroy(bodies, hang_ledge_id_);
    remove_and_destroy(bodies, mantle_ledge_id_);
    remove_and_destroy(bodies, vault_rail_id_);
    remove_and_destroy(bodies, rotating_support_id_);
    remove_and_destroy(bodies, translating_support_id_);
    remove_and_destroy(bodies, deck_id_);
}

void Simulation::PhysicsWorld::remove_and_destroy(JPH::BodyInterface &bodies, const JPH::BodyID body_id) {
    if (body_id.IsInvalid()) { return; }
    bodies.RemoveBody(body_id);
    bodies.DestroyBody(body_id);
}

JPH::Body *Simulation::PhysicsWorld::add_shape_body(JPH::BodyInterface &bodies,
                                 const JPH::Shape *shape,
                                 const JPH::RVec3 position,
                                 const JPH::Quat rotation,
                                 const JPH::EMotionType motion_type,
                                 const JPH::ObjectLayer layer,
                                 const float friction,
                                 const std::uint64_t entity_id,
                                 const float mass_kg) {
    JPH::BodyCreationSettings settings(shape, position, rotation, motion_type, layer);
    settings.mFriction = friction;
    settings.mUserData = entity_id;
    settings.mAllowSleeping = false;
    if (mass_kg > 0.0F) {
        settings.mOverrideMassProperties =
            JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = mass_kg;
    }
    JPH::Body *body = bodies.CreateBody(settings);
    const JPH::EActivation activation = motion_type == JPH::EMotionType::Static
                                            ? JPH::EActivation::DontActivate
                                            : JPH::EActivation::Activate;
    bodies.AddBody(body->GetID(), activation);
    return body;
}

JPH::BodyID Simulation::PhysicsWorld::add_box(JPH::BodyInterface &bodies,
                           const JPH::Vec3 half_extent,
                           const JPH::RVec3 position,
                           const JPH::EMotionType motion_type,
                           const JPH::ObjectLayer layer,
                           const float friction,
                           const std::uint64_t entity_id,
                           const JPH::Quat rotation,
                           const float mass_kg) {
    return add_shape_body(bodies,
                          new JPH::BoxShape(half_extent),
                          position,
                          rotation,
                          motion_type,
                          layer,
                          friction,
                          entity_id,
                          mass_kg)
        ->GetID();
}

[[nodiscard]] JPH::Vec3 Simulation::PhysicsWorld::normalized_horizontal(const double x, const double z) noexcept {
    const double length = std::hypot(x, z);
    if (!(length > 1.0e-6)) {
        return JPH::Vec3::sZero();
    }
    return JPH::Vec3(static_cast<float>(x / length), 0.0F, static_cast<float>(z / length));
}

[[nodiscard]] JPH::BodyID Simulation::PhysicsWorld::body_id_for_entity(const std::uint64_t entity_id) const noexcept {
    if (cargo_net_ && entity_id == cargo_net_->config().entity) return cargo_net_->body();
    if (catch_net_ && entity_id == catch_net_->config().entity) return catch_net_->body();
    switch (entity_id) {
    case Simulation::kStaticDeckEntityId:
        return deck_id_;
    case Simulation::kTranslatingSupportEntityId:
        return translating_support_id_;
    case Simulation::kRotatingSupportEntityId:
        return rotating_support_id_;
    case Simulation::kVaultRailEntityId:
        return vault_rail_id_;
    case Simulation::kMantleLedgeEntityId:
        return mantle_ledge_id_;
    case Simulation::kHangLedgeEntityId:
        return hang_ledge_id_;
    case Simulation::kMovingLedgeEntityId:
        return moving_ledge_id_;
    case Simulation::kBlockedLedgeEntityId:
        return blocked_ledge_id_;
    case Simulation::kBlockedLedgeCanopyEntityId:
        return blocked_ledge_canopy_id_;
    case Simulation::kTowerEntityId:
        return tower_id_;
    case Simulation::kHoistScoopEntityId:
        return scoop_ids_[0];
    case Simulation::kBallastEntityId:
        return ballast_id_;
    case Simulation::kTipperEntityId:
        return tipper_id_;
    case Simulation::kValveLeverEntityId:
        return valve_lever_id_;
    case Simulation::kLiftPlatformEntityId:
        return lift_platform_id_;
    case Simulation::kCounterweightEntityId:
        return counterweight_id_;
    case Simulation::kNeedlePierApproachEntityId:
        return needle_pier_approach_id_;
    case Simulation::kNeedlePierFarEntityId:
        return needle_pier_far_id_;
    case Simulation::kNeedleBeamEntityId:
        return needle_beam_id_;
    case Simulation::kSumpGrateEntityId:
        return sump_grate_id_;
    default:
        // The contact listener already reports Kit support entities.
        // Resolve them to the same Jolt body for locomotion and jump
        // momentum; otherwise a rotating Kit floor reads as stationary
        // here even while its contact point is moving.
        if (kit_ != nullptr && kit::is_kit_entity(entity_id)) {
            const kit::BodyIndex body = kit_->body_for_entity(entity_id);
            if (body.valid()) return kit_->body_id(body);
        }
        return {};
    }
}

} // namespace scraperx::sim
