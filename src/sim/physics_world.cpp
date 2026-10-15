#include "sim/physics_internal.hpp"

// World construction, the fixed step, support, and checkpoints.

namespace scraperx::sim {

#include "sim/world_solids.inc"

Simulation::PhysicsWorld::PhysicsWorld(const InitialSpawn initial_spawn)
    : temp_allocator_(8U * 1024U * 1024U),
      job_system_(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 1) {
    // Raised from 1024: the world's solid dressing adds ~1 600 static
    // bodies (world_solids.inc) on top of the tower's and the bands'.
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
    // the tower approach.
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

    // The proving ground's movement fixtures (WO-002, WO-003): only in a
    // world started at one of its spawns, never in the game's.
    if (scraperx::sim::is_proving_spawn(initial_spawn)) {
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
    player_id_ = bodies.CreateAndAddBody(player_settings, JPH::EActivation::Activate);

    // AS-006: the mechanism ascent's bands, built after the player.
    kit_ = std::make_unique<scraperx::sim::kit::Kit>(physics_system_, object_layers::kStatic,
                                                    object_layers::kMoving);
    scraperx::sim::bands::build_counterweight_well(*kit_, well_);
    scraperx::sim::bands::build_wet_isolation(*kit_, wet_);
    scraperx::sim::bands::build_plate_shop(*kit_, shop_);
    scraperx::sim::bands::build_facade_crane(*kit_, crane_);
    scraperx::sim::bands::build_service_skin(*kit_);
    // Band 0, the Stack: the ascent from grade.
    scraperx::sim::bands::build_stack(*kit_, stack_);
    kit_->exclude_player_from(player_id_, Simulation::kPitmanEntityId);
    // The ChatGPT wooden slingshot, seated in the approach yard between
    // grade (z = -25) and the stack. It is a machine on this route, not a
    // replacement for it. Proving-ground spawns keep their old fixtures.
    if (!scraperx::sim::is_proving_spawn(initial_spawn)) {
        slingshot_ = std::make_unique<scraperx::sim::Slingshot>(physics_system_, *kit_, player_id_);
        contact_listener_.set_slingshot(slingshot_.get());
    }

    physics_system_.OptimizeBroadPhase();

    checkpoint_position_ = JPH::RVec3(0.0, 0.9, 0.0);
    commit_machine_checkpoint();

    read_state();
}

Simulation::PhysicsWorld::~PhysicsWorld() {
    physics_system_.SetContactListener(nullptr);
    if (carry_constraint_ != nullptr) {
        physics_system_.RemoveConstraint(carry_constraint_);
        carry_constraint_ = nullptr;
    }
    if (pitman_grip_ != nullptr) {
        physics_system_.RemoveConstraint(pitman_grip_);
        pitman_grip_ = nullptr;
    }
    contact_listener_.set_slingshot(nullptr);
    slingshot_.reset();
    kit_.reset();
    for (JPH::Ref<JPH::TwoBodyConstraint> &constraint : machine_constraints_) {
        if (constraint != nullptr) {
            physics_system_.RemoveConstraint(constraint);
        }
    }
    machine_constraints_.clear();
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

void Simulation::PhysicsWorld::step(const StepCommands &commands,
          const float delta_seconds,
          const double next_time_seconds) noexcept {
    // WO-008: captured before anything this tick can change grounded_, so
    // it means exactly "was the player standing on something one tick ago."
    const bool was_grounded_before_tick = grounded_;

    auto &bodies = physics_system_.GetBodyInterface();
    update_support_motion(bodies, delta_seconds, next_time_seconds);

    // A toggle while airborne only: deploying/retracting on the ground is
    // meaningless and would let a grounded button-mash pre-arm the canopy.
    if (commands.parachute_toggle_requested && !was_grounded_before_tick) {
        parachute_deployed_ = !parachute_deployed_;
    }

    if (regrab_lockout_ticks_ > 0) {
        --regrab_lockout_ticks_;
    }
    facing_ = normalized_horizontal(commands.facing_x, commands.facing_z);

    const bool sling_controls = slingshot_ != nullptr && slingshot_->controls_player();
    if (!sling_controls) {
        update_crouch(bodies, commands);
        update_rig(bodies, commands);
        update_carry(bodies, commands, delta_seconds);
        apply_traversal_commands(bodies, commands);
        if (traversal_state_ == TraversalState::Climbing) {
            update_climb(bodies, commands, delta_seconds);
        } else if (traversal_state_ == TraversalState::Hanging) {
            if (pitman_grip_ == nullptr) {
                update_shimmy(bodies, commands, delta_seconds);
            }
            update_swing(bodies, commands, delta_seconds);
        }
    }

    bool jump_started = false;
    if (!sling_controls && traversal_state_ == TraversalState::None) {
        jump_started = apply_locomotion(bodies, commands, delta_seconds);
        if (!jump_started) {
            try_begin_hang(bodies, commands);
        }
        if (!was_grounded_before_tick) {
            apply_parachute_drag(bodies, delta_seconds);
            // Distinct from fall_peak_speed_mps_ (a running max, telemetry
            // only): this is overwritten every tick, so it always holds
            // exactly the velocity the body carries into this tick's
            // contact resolution -- what "impact speed" has to mean for
            // late deceleration (a well-timed parachute) to matter.
            const float vertical_speed = bodies.GetLinearVelocity(player_id_).GetY();
            pre_contact_fall_speed_mps_ = std::max(0.0F, -vertical_speed);
            fall_peak_speed_mps_ =
                std::max(fall_peak_speed_mps_, pre_contact_fall_speed_mps_);
            const JPH::Vec3 planar = bodies.GetLinearVelocity(player_id_);
            pre_contact_planar_ = JPH::Vec3(planar.GetX(), 0.0F, planar.GetZ());
        }
    }

    if (traversal_state_ != TraversalState::None) {
        drive_traversal(bodies, delta_seconds);
        load_hold(bodies);
    }

    if (slingshot_ != nullptr) {
        const bool on_pouch = grounded_ && support_entity_id_ == scraperx::sim::Slingshot::kPouchEntity;
        slingshot_->pre_step(delta_seconds, commands.sling_draw, commands.sling_yaw,
                             commands.sling_elevation, commands.sling_action, commands.sling_drop,
                             on_pouch);
    }

    const JPH::Vec3 rider_velocity_before = bodies.GetLinearVelocity(player_id_);
    kit_->pre_step(delta_seconds);
    contact_listener_.begin_tick();
    const int collision_steps =
        slingshot_ != nullptr &&
                (slingshot_->controls_player() || slingshot_->state().released ||
                 slingshot_->state().recovering || slingshot_->state().drawing)
            ? 4
            : 1;
    physics_system_.Update(delta_seconds, collision_steps, &temp_allocator_, &job_system_);
    kit_->post_step(delta_seconds);
    if (slingshot_ != nullptr) {
        slingshot_->post_step(delta_seconds);
    }

    SupportSample support = contact_listener_.sample();
    if (jump_started || traversal_state_ != TraversalState::None) {
        support = {};
    }
    support_sample_ = support;
    grounded_ = support.grounded;
    support_entity_id_ = support.entity_id;

    // WO-008: a genuine (non-traversal, non-jump) landing is the one
    // moment fall consequence is resolved. Traversal-completion landings
    // never reach here with a real fall velocity -- complete_traversal
    // always sets a support-relative landing velocity first -- so no
    // separate traversal exemption is needed.
    bool died_this_tick = false;
    if (grounded_ && !was_grounded_before_tick && traversal_state_ == TraversalState::None &&
        !jump_started) {
        last_impact_speed_mps_ = pre_contact_fall_speed_mps_;
        if (last_impact_speed_mps_ > kLethalImpactSpeedMps) {
            restore_from_checkpoint(bodies);
            if (slingshot_ != nullptr) {
                slingshot_->cancel_player_interaction();
            }
            died_this_tick = true;
        } else if (last_impact_speed_mps_ >= kRollKeepImpact) {
            // The landing ate the run. Put the horizontal speed back.
            JPH::Vec3 landed = bodies.GetLinearVelocity(player_id_);
            landed.SetX(pre_contact_planar_.GetX());
            landed.SetZ(pre_contact_planar_.GetZ());
            bodies.SetLinearVelocity(player_id_, landed);
        }
    }
    if (!died_this_tick && grounded_ && traversal_state_ == TraversalState::None &&
        footing_is_firm(bodies)) {
        commit_checkpoint(bodies);
    }
    if (grounded_) {
        fall_peak_speed_mps_ = 0.0F;
        parachute_deployed_ = false;
    }

    if (traversal_state_ != TraversalState::None) {
        resolve_traversal_outcome(bodies);
    }

    update_affordance(bodies);
    // Gravity alone is not a felt load. A checkpoint restore is a
    // discontinuity, not an acceleration.
    rider_specific_acceleration_ = died_this_tick || delta_seconds <= 0.0F
                                       ? JPH::Vec3::sZero()
                                       : (bodies.GetLinearVelocity(player_id_) - rider_velocity_before) /
                                                 delta_seconds -
                                             physics_system_.GetGravity();
    read_state();
}

void Simulation::PhysicsWorld::build_world_solids(JPH::BodyInterface &bodies) {
    for (const WorldSolidBox &box : kWorldSolidBoxes) {
        const JPH::Vec3 half(box.hx, box.hy, box.hz);
        const JPH::RVec3 position(box.px, box.py, box.pz);
        const JPH::Quat rotation = JPH::Quat(box.qx, box.qy, box.qz, box.qw).Normalized();
        const JPH::AABox bounds = JPH::AABox(-half, half).Transformed(
            JPH::Mat44::sRotationTranslation(rotation, JPH::Vec3(position)));
        if (mirrors_owned_body(bounds)) {
            ++world_solid_mirrors_;
            continue;
        }
        const float convex_radius = std::min(JPH::cDefaultConvexRadius, half.ReduceMin());
        add_shape_body(bodies, new JPH::BoxShape(half, convex_radius), position, rotation,
                       JPH::EMotionType::Static, object_layers::kStatic, kWorldSolidFriction,
                       Simulation::kWorldSolidEntityId, 0.0F);
        ++world_solid_bodies_;
    }
    JPH::Array<JPH::Vec3> points;
    for (const WorldSolidHull &hull : kWorldSolidHulls) {
        JPH::DVec3 sum = JPH::DVec3::sZero();
        for (std::uint32_t index = 0; index < hull.count; ++index) {
            const float *point = &kWorldSolidHullPoints[(hull.first + index) * 3U];
            sum += JPH::DVec3(point[0], point[1], point[2]);
        }
        const JPH::DVec3 centre = sum / static_cast<double>(std::max<std::uint32_t>(hull.count, 1U));
        points.clear();
        for (std::uint32_t index = 0; index < hull.count; ++index) {
            const float *point = &kWorldSolidHullPoints[(hull.first + index) * 3U];
            points.push_back(JPH::Vec3(static_cast<float>(point[0] - centre.GetX()),
                                       static_cast<float>(point[1] - centre.GetY()),
                                       static_cast<float>(point[2] - centre.GetZ())));
        }
        const JPH::ConvexHullShapeSettings settings(points.data(), static_cast<int>(points.size()));
        const JPH::ShapeSettings::ShapeResult result = settings.Create();
        if (result.HasError()) {
            ++world_solid_rejected_;
            continue;
        }
        add_shape_body(bodies, result.Get().GetPtr(),
                       JPH::RVec3(centre.GetX(), centre.GetY(), centre.GetZ()),
                       JPH::Quat::sIdentity(), JPH::EMotionType::Static, object_layers::kStatic,
                       kWorldSolidFriction, Simulation::kWorldSolidEntityId, 0.0F);
        ++world_solid_bodies_;
    }
}

bool Simulation::PhysicsWorld::mirrors_owned_body(const JPH::AABox &bounds) const {
    JPH::AllHitCollisionCollector<JPH::CollideShapeBodyCollector> collector;
    physics_system_.GetBroadPhaseQuery().CollideAABox(bounds, collector);
    for (const JPH::BodyID id : collector.mHits) {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), id);
        if (!lock.Succeeded() ||
            lock.GetBody().GetUserData() == Simulation::kWorldSolidEntityId) {
            continue;
        }
        const JPH::AABox owned = lock.GetBody().GetWorldSpaceBounds();
        if ((owned.mMin - bounds.mMin).Abs().ReduceMax() < kWorldSolidMirrorTolerance &&
            (owned.mMax - bounds.mMax).Abs().ReduceMax() < kWorldSolidMirrorTolerance) {
            return true;
        }
    }
    return false;
}

void Simulation::PhysicsWorld::build_stack(JPH::BodyInterface &bodies) {
    const auto frame = [&](const JPH::Vec3 half_extent, const JPH::RVec3 position) {
        machine_bodies_.push_back(add_box(bodies, half_extent, position,
                                          JPH::EMotionType::Static, object_layers::kStatic,
                                          0.85F, Simulation::kTowerEntityId));
    };

    const float band_center = kStackHalfExtent - kStackDeckBandDepth * 0.5F;
    const float inner_half = kStackHalfExtent - kStackDeckBandDepth;
    for (int level = 1; level <= kStackLevelCount; ++level) {
        const float slab_y =
            static_cast<float>(level) * kStackLevelHeight - kStackDeckHalfThickness;
        // Deck ring: two full-width bands and two inner bands.
        for (const float side : {1.0F, -1.0F}) {
            frame(JPH::Vec3(kStackHalfExtent, kStackDeckHalfThickness,
                            kStackDeckBandDepth * 0.5F),
                  JPH::RVec3(kStackCenterX, slab_y, kStackCenterZ + side * band_center));
        }
        for (const float side : {1.0F, -1.0F}) {
            if (level == 7 && side > 0.0F) {
                // Level 7 East band has an equipment hatch / ladder well for C2 from Z = -148.70F to -145.00F:
                const float s_min_z = -145.00F;
                const float s_max_z = -133.00F;
                const float s_half_z = 0.5F * (s_max_z - s_min_z);
                const float s_center_z = 0.5F * (s_min_z + s_max_z);
                frame(JPH::Vec3(kStackDeckBandDepth * 0.5F, kStackDeckHalfThickness, s_half_z),
                      JPH::RVec3(kStackCenterX + side * band_center, slab_y, s_center_z));

                const float n_min_z = -167.00F;
                const float n_max_z = -148.70F;
                const float n_half_z = 0.5F * (n_max_z - n_min_z);
                const float n_center_z = 0.5F * (n_min_z + n_max_z);
                frame(JPH::Vec3(kStackDeckBandDepth * 0.5F, kStackDeckHalfThickness, n_half_z),
                      JPH::RVec3(kStackCenterX + side * band_center, slab_y, n_center_z));
            } else {
                frame(JPH::Vec3(kStackDeckBandDepth * 0.5F, kStackDeckHalfThickness, inner_half),
                      JPH::RVec3(kStackCenterX + side * band_center, slab_y, kStackCenterZ));
            }
        }
    }

    // Columns: corners and edge mid-spans, one run per storey.
    for (int level = 0; level < kStackLevelCount; ++level) {
        const float base_y = static_cast<float>(level) * kStackLevelHeight;
        const float column_half = kStackLevelHeight * 0.5F;
        for (const float sx : {1.0F, -1.0F}) {
            for (const float sz : {1.0F, -1.0F}) {
                frame(JPH::Vec3(kStackColumnHalf, column_half, kStackColumnHalf),
                      JPH::RVec3(kStackCenterX + sx * kStackHalfExtent, base_y + column_half,
                                 kStackCenterZ + sz * kStackHalfExtent));
            }
            frame(JPH::Vec3(kStackColumnHalf, column_half, kStackColumnHalf),
                  JPH::RVec3(kStackCenterX + sx * kStackHalfExtent, base_y + column_half,
                             kStackCenterZ));
            frame(JPH::Vec3(kStackColumnHalf, column_half, kStackColumnHalf),
                  JPH::RVec3(kStackCenterX, base_y + column_half,
                             kStackCenterZ + sx * kStackHalfExtent));
        }
    }
}

JPH::TwoBodyConstraint *Simulation::PhysicsWorld::create_constraint(
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

JPH::Vec3 Simulation::PhysicsWorld::current_support_point_velocity(
    const JPH::BodyInterface &bodies) const noexcept {
    JPH::BodyID support_id = body_id_for_entity(support_entity_id_);
    // Kit machines are not in that table. A body standing on one still
    // has to take the speed of the point under its feet, or the floor
    // leaves and the body stays.
    if (support_id.IsInvalid() && kit_ != nullptr &&
        scraperx::sim::kit::is_dynamic_entity(support_entity_id_)) {
        const scraperx::sim::kit::BodyIndex body = kit_->body_for_entity(support_entity_id_);
        if (body.valid()) {
            support_id = kit_->body_id(body);
        }
    }
    if (support_id.IsInvalid()) {
        return JPH::Vec3::sZero();
    }
    return bodies.GetPointVelocity(
        support_id,
        JPH::RVec3(support_sample_.contact_point.x,
                   support_sample_.contact_point.y,
                   support_sample_.contact_point.z));
}

void Simulation::PhysicsWorld::update_support_motion(JPH::BodyInterface &bodies,
                           const float delta_seconds,
                           const double next_time_seconds) noexcept {
    const double translating_x =
        kTranslatingSupportAmplitudeMeters *
        std::sin(kTranslatingSupportAngularFrequency * next_time_seconds);
    bodies.MoveKinematic(translating_support_id_,
                         JPH::RVec3(translating_x, 0.25, 8.0),
                         JPH::Quat::sIdentity(),
                         delta_seconds);

    rotating_support_yaw_radians_ =
        std::fmod(kRotatingSupportAngularSpeed * next_time_seconds, 2.0 * kPi);
    bodies.MoveKinematic(
        rotating_support_id_,
        JPH::RVec3(-8.0, 0.25, 0.0),
        JPH::Quat::sRotation(JPH::Vec3(0.0F, 1.0F, 0.0F),
                             static_cast<float>(rotating_support_yaw_radians_)),
        delta_seconds);

    const double moving_ledge_z =
        kMovingLedgeCenterZ +
        kMovingLedgeAmplitudeMeters *
            std::sin(kMovingLedgeAngularFrequency * next_time_seconds);
    bodies.MoveKinematic(moving_ledge_id_,
                         JPH::RVec3(9.0, 1.8, moving_ledge_z),
                         JPH::Quat::sIdentity(),
                         delta_seconds);

}

bool Simulation::PhysicsWorld::footing_is_firm(const JPH::BodyInterface &bodies) const noexcept {
    const float reach =
        (crouched_ ? kPlayerCrouchHalfHeight : kPlayerHalfHeight) + kCheckpointFootingSlack;
    const JPH::RRayCast ray(bodies.GetPosition(player_id_), JPH::Vec3(0.0F, -reach, 0.0F));
    JPH::RayCastResult hit;
    const JPH::IgnoreSingleBodyFilter player_filter(player_id_);
    const JPH::IgnoreSingleBodyFilterChained filter(carried_id_, player_filter);
    if (!physics_system_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, filter)) {
        return false;
    }
    return surface_normal(hit.mBodyID, hit.mSubShapeID2, ray.GetPointOnRay(hit.mFraction))
               .GetY() >= kSupportNormalThreshold;
}

void Simulation::PhysicsWorld::commit_machine_checkpoint() noexcept {
    checkpoint_.carrying_entity = carried_entity_;
    kit_->capture(checkpoint_.kit);
}

void Simulation::PhysicsWorld::commit_checkpoint(const JPH::BodyInterface &bodies) noexcept {
    checkpoint_position_ = bodies.GetPosition(player_id_);
    checkpoint_crouched_ = crouched_;
    commit_machine_checkpoint();
    ++checkpoint_commit_count_;
}

void Simulation::PhysicsWorld::restore_from_checkpoint(JPH::BodyInterface &bodies) noexcept {
    // The capsule the checkpoint was committed in: a crouched commit's
    // centre is a crouched centre, and may sit under a low ceiling.
    crouched_ = checkpoint_crouched_;
    bodies.SetShape(player_id_, active_player_shape(), false, JPH::EActivation::Activate);
    bodies.SetPositionAndRotation(player_id_, checkpoint_position_, JPH::Quat::sIdentity(),
                                  JPH::EActivation::Activate);
    bodies.SetLinearAndAngularVelocity(player_id_, JPH::Vec3::sZero(), JPH::Vec3::sZero());
    rider_specific_acceleration_ = JPH::Vec3::sZero();

    // A kit body in the hands may be one the restore takes out of the
    // world (a shackle hooked at the commit): let go of it first.
    if (carry_constraint_ != nullptr &&
        scraperx::sim::kit::is_kit_entity(carried_entity_)) {
        release_carry();
    }
    kit_->restore(checkpoint_.kit);
    if (pitman_grip_ != nullptr) {
        physics_system_.RemoveConstraint(pitman_grip_);
        pitman_grip_ = nullptr;
        bodies.SetGravityFactor(player_id_, 1.0F);
        traversal_state_ = TraversalState::None;
    }
    restore_carry_topology(checkpoint_.carrying_entity);
    // The body comes back at rest, so what it holds does too. Restored
    // with the walking speed it was committed at, the load swung out of
    // the still hands and dragged the body back off the edge it had just
    // been restored onto (observed at a deck's edge).
    if (carry_constraint_ != nullptr) {
        bodies.SetLinearAndAngularVelocity(carried_id_, JPH::Vec3::sZero(),
                                           JPH::Vec3::sZero());
    }

    grounded_ = false;
    jump_vault_ticks_left_ = 0;
    support_entity_id_ = 0;
    support_sample_ = {};
    airborne_inherited_velocity_ = JPH::Vec3::sZero();
    air_full_speed_ = kPlayerMaximumRelativeSpeed;
    sprinting_ = false;
    balancing_ = false;
    fall_peak_speed_mps_ = 0.0F;
    pre_contact_fall_speed_mps_ = 0.0F;
    parachute_deployed_ = false;
    ++death_count_;
}

void Simulation::PhysicsWorld::read_machine_state() noexcept {
    state_.carrying_entity_id = carried_entity_;
    state_.carry_target_entity_id = carry_target_entity_;

    state_.rig_action = rig_action_;
    state_.rig_target_entity_id = rig_target_entity_;
    switch (kit_->carry_kind(carry_target_entity_)) {
    case scraperx::sim::kit::CarryKind::Shackle:
        state_.carry_target_kind = 1;
        break;
    case scraperx::sim::kit::CarryKind::Handle:
        state_.carry_target_kind = 2;
        break;
    default:
        state_.carry_target_kind = 0;
        break;
    }
    const auto &kit = *kit_;
    state_.well_a_cage_travel = kit.guide_travel(well_.a_cage_guide);
    state_.well_a_skip_travel = kit.guide_travel(well_.a_skip_guide);
    state_.well_a_cage_peak_speed = kit.guide_peak_speed(well_.a_cage_guide);
    state_.well_a_catch_latched = kit.catch_latched(well_.a_catch);
    state_.well_a_rope_end_entity_id = kit.rope_end_entity(well_.a_rope);
    state_.well_a_rope_tension_n = kit.rope_tension(well_.a_rope);
    state_.well_a_lever_angle = kit.lever_angle(well_.a_lever);
    state_.well_b_cage_travel = kit.guide_travel(well_.b_cage_guide);
    state_.well_b_cage_peak_speed = kit.guide_peak_speed(well_.b_cage_guide);
    state_.well_b_boom_angle = kit.lever_angle(well_.b_boom_hinge);
    state_.well_b_catch_latched = kit.catch_latched(well_.b_catch);
    state_.well_b_rope_end_entity_id = kit.rope_end_entity(well_.b_rope);
    state_.well_b_rope_let_go = kit.rope_parted(well_.b_rope);
    state_.well_b_rope_tension_n = kit.rope_tension(well_.b_rope);
    state_.well_c_platform_travel = kit.guide_travel(well_.c_platform_guide);
    state_.well_c_platform_peak_speed = kit.guide_peak_speed(well_.c_platform_guide);
    state_.well_c_dumpster_travel = kit.guide_travel(well_.c_dumpster_guide);
    state_.well_c_catch_latched = kit.catch_latched(well_.c_catch);
    state_.well_c_hopper_kg = kit.bin_contents(well_.c_hopper);
    state_.well_c_dumpster_kg = kit.bin_contents(well_.c_dumpster_bin);
    state_.well_a_cage_rubble_kg = kit.bin_contents(well_.a_cage_bin);
    state_.well_rubble_spilled_kg = kit.spilled();
    state_.well_c_rebar_angle = kit.lever_angle(well_.c_rebar);
    state_.well_c_latch_angle = kit.lever_angle(well_.c_latch);
}

void Simulation::PhysicsWorld::read_state() noexcept {
    const auto &bodies = physics_system_.GetBodyInterface();

    const JPH::RVec3 player_position = bodies.GetPosition(player_id_);
    const JPH::Vec3 player_velocity = bodies.GetLinearVelocity(player_id_);
    state_.player_position = to_vector3(player_position);
    state_.player_linear_velocity =
        {player_velocity.GetX(), player_velocity.GetY(), player_velocity.GetZ()};
    state_.player_grounded = grounded_;
    state_.player_crouched = crouched_;
    state_.support_entity_id = support_entity_id_;
    state_.support_contact_point = support_sample_.contact_point;
    state_.support_point_linear_velocity = support_sample_.point_velocity;

    const JPH::RVec3 translating_position = bodies.GetPosition(translating_support_id_);
    const JPH::Vec3 translating_velocity = bodies.GetLinearVelocity(translating_support_id_);
    state_.translating_support_position = to_vector3(translating_position);
    state_.translating_support_linear_velocity =
        {translating_velocity.GetX(), translating_velocity.GetY(), translating_velocity.GetZ()};

    const JPH::RVec3 rotating_position = bodies.GetPosition(rotating_support_id_);
    const JPH::Vec3 rotating_angular_velocity = bodies.GetAngularVelocity(rotating_support_id_);
    state_.rotating_support_position = to_vector3(rotating_position);
    state_.rotating_support_yaw_radians = rotating_support_yaw_radians_;
    state_.rotating_support_angular_velocity =
        {rotating_angular_velocity.GetX(),
         rotating_angular_velocity.GetY(),
         rotating_angular_velocity.GetZ()};

    const JPH::RVec3 moving_ledge_position = bodies.GetPosition(moving_ledge_id_);
    const JPH::Vec3 moving_ledge_velocity = bodies.GetLinearVelocity(moving_ledge_id_);
    state_.moving_ledge_position = to_vector3(moving_ledge_position);
    state_.moving_ledge_linear_velocity =
        {moving_ledge_velocity.GetX(), moving_ledge_velocity.GetY(), moving_ledge_velocity.GetZ()};

    state_.traversal_state = traversal_state_;
    state_.traversal_support_entity_id = traversal_entity_id_;
    state_.traversal_progress = traversal_progress_;
    if (traversal_state_ == TraversalState::None) {
        state_.traversal_ledge_point = {};
        state_.traversal_target_point = {};
    } else {
        state_.traversal_ledge_point =
            to_vector3(from_support_local(bodies, traversal_body_, traversal_local_ledge_));
        state_.traversal_target_point = to_vector3(
            from_support_local(bodies, traversal_target_body_, traversal_local_target_));
    }

    // Step 2: the hands' points, the structure's direction, the legs,
    // and what a grounded body is offered.
    state_.traversal_left_hand = {};
    state_.traversal_right_hand = {};
    state_.traversal_normal = {};
    if (traversal_state_ != TraversalState::None) {
        state_.traversal_normal = {traversal_normal_.GetX(), 0.0, traversal_normal_.GetZ()};
    }
    if (traversal_state_ == TraversalState::Climbing && hands_[0].valid && hands_[1].valid) {
        state_.traversal_left_hand = to_vector3(hand_point(bodies, 0));
        state_.traversal_right_hand = to_vector3(hand_point(bodies, 1));
    } else if ((traversal_state_ == TraversalState::Hanging ||
                traversal_state_ == TraversalState::Lowering) &&
               !traversal_normal_.IsNearZero()) {
        const JPH::RVec3 lip = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
        const JPH::Vec3 span = traversal_right() * kHangHandSpan;
        state_.traversal_left_hand = to_vector3(lip - span);
        state_.traversal_right_hand = to_vector3(lip + span);
    }
    state_.player_sprinting = sprinting_;
    state_.player_balancing = balancing_;
    state_.grip_available = grip_affordance_.valid;
    state_.grip_entity_id = grip_affordance_.valid ? grip_affordance_.entity_id : 0;
    state_.grip_point = grip_affordance_.valid ? to_vector3(grip_affordance_.point) : Vector3{};
    state_.edge_drop_available = edge_affordance_.valid;
    state_.edge_drop_point =
        edge_affordance_.valid ? to_vector3(edge_affordance_.ledge) : Vector3{};
    state_.climb_count = climb_count_;

    state_.ledge_available = affordance_.valid;
    state_.ledge_entity_id = affordance_.valid ? affordance_.ledge_entity_id : 0;
    state_.ledge_point = affordance_.valid ? to_vector3(affordance_.ledge_point) : Vector3{};
    state_.ledge_rise_meters = affordance_.valid ? affordance_.rise : 0.0;

    read_machine_state();

    state_.accepted_traversal_count = accepted_traversal_count_;
    state_.world_solid_bodies = world_solid_bodies_;
    state_.step_up_count = step_up_count_;
    state_.jump_vault_count = jump_vault_count_;
    state_.world_solid_mirrors = world_solid_mirrors_;
    state_.world_solid_rejected = world_solid_rejected_;
    state_.rejected_traversal_count = rejected_traversal_count_;
    state_.aborted_traversal_count = aborted_traversal_count_;

    state_.fall_state = !grounded_
        ? (parachute_deployed_ ? FallState::Parachuting : FallState::Airborne)
        : FallState::Grounded;
    state_.fall_peak_speed_mps = fall_peak_speed_mps_;
    state_.last_impact_speed_mps = last_impact_speed_mps_;
    state_.parachute_deployed = parachute_deployed_;
    state_.checkpoint_position = to_vector3(checkpoint_position_);
    state_.checkpoint_commit_count = checkpoint_commit_count_;
    state_.death_count = death_count_;
}

} // namespace scraperx::sim
