// The physics world: construction and teardown, the tick, bodies, the world's solids and supports.
// Members of Simulation::PhysicsWorld (physics_world.hpp).

#include "sim/physics_world.hpp"

namespace scraperx::sim::detail {

std::mutex JoltRuntimeLease::mutex_;
std::uint32_t JoltRuntimeLease::lease_count_ = 0;

} // namespace scraperx::sim::detail

namespace scraperx::sim {

namespace {

// The presentation's static dressing as native collision (see
// kWorldSolidEntityId). Boxes are oriented; hulls are world-space point sets.
struct WorldSolidBox final {
    float px, py, pz;
    float qx, qy, qz, qw;
    float hx, hy, hz;
};
struct WorldSolidHull final {
    std::uint32_t first;
    std::uint32_t count;
};
#include "sim/world_solids.inc"

} // namespace

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

    if (scraperx::sim::is_proving_spawn(initial_spawn)) {
        const auto bar_part = [this, &bodies](const JPH::Vec3 half, const JPH::RVec3 at) {
            machine_bodies_.push_back(add_box(bodies, half, at, JPH::EMotionType::Static,
                                              object_layers::kStatic, 0.7F,
                                              Simulation::kMonkeyBarsEntityId));
        };
        for (int bar = 0; bar < 6; ++bar) {
            bar_part(JPH::Vec3(0.04F, 0.04F, 0.62F), JPH::RVec3(static_cast<double>(bar), 3.1, -20.0));
        }
        for (const double side : {-1.0, 1.0}) {
            bar_part(JPH::Vec3(3.3F, 0.12F, 0.12F), JPH::RVec3(2.5, 3.26, -20.0 + side * 0.74));
            for (const double end : {-0.9, 5.9}) {
                bar_part(JPH::Vec3(0.12F, 1.69F, 0.12F), JPH::RVec3(end, 1.69, -20.0 + side * 0.74));
            }
        }
        bar_part(JPH::Vec3(1.0F, 0.25F, 0.5F), JPH::RVec3(6.9, 0.25, -20.0));
        hook_rack_id_ = add_box(bodies, JPH::Vec3(0.15F, 0.6F, 0.15F), JPH::RVec3(-2.0, 0.6, -20.9),
                                JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                                Simulation::kHookRackEntityId);
        machine_bodies_.push_back(hook_rack_id_);
        machine_bodies_.push_back(add_box(bodies, JPH::Vec3(6.0F, 2.0F, 0.25F), JPH::RVec3(-6.0, 2.0, -27.0),
                                          JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                                          Simulation::kRunWallEntityId));
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
    scraperx::sim::bands::build_midstack_service(*kit_, service_);
    // Band 0, the Stack: the ascent from grade.
    scraperx::sim::bands::build_stack(*kit_, stack_);
    // The ground slingshot, in the yard south of the tower.
    slingshot_ = std::make_unique<Slingshot>(physics_system_, *kit_, player_id_);
    // AS-012, the swing off the 220 ring's south face.
    swing_ = std::make_unique<Swing>(physics_system_, *kit_, player_id_);
    // C6, the west band's climb from the 242 ring to the 264 ring.
    ClimbC6::build(*kit_);
    // The owner's vertical machines, on their own motors, where the route
    // has no way up: from the 264 ring, the cascade mast.
    vertical_ = std::make_unique<scraperx::sim::vertical::Route>(*kit_, physics_system_);
    // Phase 3: the trapeze in the yard east of the start. Built last, so
    // every body before it keeps its place in the kit.
    trapeze_ = std::make_unique<Trapeze>(physics_system_, *kit_, player_id_);
    // Parkour step 2: the pipe gallery beside it, built after it for the
    // same reason.
    PipeGallery::build(*kit_);
    // Parkour step 5: the grappling hook, found on the 220 ring's
    // north-west corner, off the line from C's platform to the swing.
    build_hook_rack();
    // The vertical route's late bodies (the pitman's deck counterweight),
    // after every other kit body so older saves' lists still line up.
    vertical_->build_late();
    // C7, the west face's exposed climb from the 264 ring to the
    // mast-pitman walkway: the alternative to riding the mast. Last too.
    ClimbC7::build(*kit_);
    // After C7, so every save written before it still lines up.
    vertical_->build_wheel_reload();
    // Life L2: the yard crew, real bodies on grade (not in the kit, not saved).
    crew_ = std::make_unique<Crew>(physics_system_, object_layers::kMoving);

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
    drop_hang_arms();
    stow_hook(physics_system_.GetBodyInterface());
    vertical_.reset();
    trapeze_.reset();
    swing_.reset();
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
    look_pitch_ = static_cast<float>(commands.look_pitch);

    // Harnessed into the slingshot's pouch or the swing's seat, the body
    // is the machine's: no crouch, carry, rig or traversal begins.
    if (!slingshot_->controls_player() && !swing_->controls_player() && !trapeze_->controls_player()) {
        update_crouch(bodies, commands);
        update_rig(bodies, commands);
        update_carry(bodies, commands, delta_seconds);
        apply_traversal_commands(bodies, commands);
        if (traversal_state_ == TraversalState::Climbing) {
            update_climb(bodies, commands, delta_seconds);
        } else if (traversal_state_ == TraversalState::Hanging) {
            update_shimmy(bodies, commands, delta_seconds);
        } else if (traversal_state_ == TraversalState::Swinging) {
            update_swing(bodies, commands);
        }
    }

    const bool sling_was_controlling = slingshot_->controls_player();
    slingshot_->pre_step(delta_seconds, commands.sling_draw, commands.sling_yaw,
                         commands.sling_elevation, commands.sling_action, commands.sling_drop,
                         grounded_ && support_entity_id_ == Slingshot::kPouchEntity);
    swing_->pre_step(delta_seconds, commands.swing_action, commands.swing_drop);
    trapeze_->pre_step(delta_seconds, commands.trapeze_action, commands.trapeze_drop, commands.crouch_held,
                       !grounded_ && traversal_state_ == TraversalState::None);
    const bool sling_controlling =
        slingshot_->controls_player() || swing_->controls_player() || trapeze_->controls_player();
    if (sling_was_controlling && !slingshot_->controls_player() && slingshot_->state().released) {
        slingshot_flight_ = true;
    }
    if (sling_controlling) {
        release_carry();
        clear_traversal();
        parachute_deployed_ = false;
    }
    if (grounded_ && support_entity_id_ != Slingshot::kPouchEntity && !sling_controlling) {
        slingshot_flight_ = false;
    }

    if (sling_controlling || traversal_state_ != TraversalState::None) {
        jump_buffer_ticks_ = 0;
    }
    if (jump_buffer_ticks_ > 0) {
        --jump_buffer_ticks_;
    }
    bool jump_started = false;
    // A teeter is ordinary locomotion's; whatever else holds the body
    // (harness, hands, a throw) ends it.
    if (sling_controlling || traversal_state_ != TraversalState::None || (slingshot_flight_ && !grounded_)) {
        teetering_ = false;
        line_held_ = false;
        flight_from_jump_ = false;
        clear_support_jolt();
    }
    apply_wind(bodies, commands.time_seconds);
    if (!sling_controlling && traversal_state_ == TraversalState::None) {
        if (slingshot_flight_ && !grounded_) {
            // Thrown: the stick steers with a bounded force (2 m/s/s at
            // full stick) and does not brake the throw to a walking speed
            // as air control would.
            bodies.AddForce(player_id_,
                            JPH::Vec3(static_cast<float>(commands.move_input_x * kPlayerMassKg * 2.0), 0.0F,
                                      static_cast<float>(commands.move_input_z * kPlayerMassKg * 2.0)));
        } else {
            jump_started = apply_locomotion(bodies, commands, delta_seconds);
        }
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
            pre_contact_velocity_ = bodies.GetLinearVelocity(player_id_);
            const float vertical_speed = pre_contact_velocity_.GetY();
            pre_contact_fall_speed_mps_ = std::max(0.0F, -vertical_speed);
            fall_peak_speed_mps_ =
                std::max(fall_peak_speed_mps_, pre_contact_fall_speed_mps_);
        }
    }

    if (traversal_state_ != TraversalState::None) {
        drive_traversal(bodies, delta_seconds);
        load_hold(bodies);
    }
    hook_pre_step(bodies, commands, delta_seconds);

    vertical_->pre_step(delta_seconds, commands.lift_action, grounded_ ? support_entity_id_ : 0,
                        commands.lift_haul);
    kit_->pre_step(delta_seconds);
    crew_->pre_step(delta_seconds, bodies.GetPosition(player_id_), bodies.GetLinearVelocity(player_id_),
                    grounded_);
    contact_listener_.begin_tick();
    physics_system_.Update(delta_seconds, 1, &temp_allocator_, &job_system_);
    kit_->post_step(delta_seconds);
    slingshot_->post_step(delta_seconds);
    swing_->post_step(delta_seconds);
    trapeze_->post_step(delta_seconds);
    hook_post_step(bodies, delta_seconds);
    crew_->post_step(delta_seconds);

    SupportSample support = contact_listener_.sample();
    struck_closing_mps_ = contact_listener_.struck_closing();
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
            died_this_tick = true;
        } else {
            bool prepared = commands.crouch_held;
            if (precision_target_valid_) {
                const JPH::RVec3 at = bodies.GetPosition(player_id_);
                const float off = static_cast<float>(std::hypot(at.GetX() - precision_target_.GetX(),
                                                                at.GetZ() - precision_target_.GetZ()));
                if (off <= kPlayerRadius + kPrecisionLandingSlack) {
                    prepared = true;   // the legs were set for exactly this landing
                    ++precision_landing_count_;
                }
            }
            record_landing(bodies, prepared);
        }
        precision_target_valid_ = false;
    }
    if (!died_this_tick) {
        advance_recovery(bodies, delta_seconds);
    }
    if (!died_this_tick && grounded_ && traversal_state_ == TraversalState::None &&
        footing_is_firm(bodies)) {
        commit_checkpoint(bodies);
        static_assert(Simulation::kRouteTopEntityId == scraperx::sim::vertical::Route::kDerrickStaticFirst + 2,
                      "the route's top is the derrick's upper receiver");
        if (!summit_reached_ && support_entity_id_ == Simulation::kRouteTopEntityId) {
            summit_reached_ = true;
            summit_seconds_ = next_time_seconds;
            summit_deaths_ = death_count_;
        }
    }
    if (grounded_) {
        fall_peak_speed_mps_ = 0.0F;
        parachute_deployed_ = false;
    }

    check_hang_grip(bodies, delta_seconds);
    if (traversal_state_ != TraversalState::None) {
        resolve_traversal_outcome(bodies);
    }

    update_affordance(bodies);
    update_precision_sight(bodies);
    read_state();
}

// The first surface straight below a point within reach, the player's
// own body ignored: where a bird may stand. Read only.
bool Simulation::PhysicsWorld::surface_below(const JPH::RVec3 from, const float reach, JPH::RVec3 &point) const noexcept {
    if (!(reach > 0.0F)) {
        return false;
    }
    JPH::RayCastResult hit;
    const JPH::Vec3 down(0.0F, -reach, 0.0F);
    if (!cast_ray(from, down, hit)) {
        return false;
    }
    point = JPH::RRayCast(from, down).GetPointOnRay(hit.mFraction);
    return true;
}

// The slingshot's predicted path from the pouch now, swept with the
// player's capsule and cut at the first thing it would hit.
std::vector<Vector3> Simulation::PhysicsWorld::slingshot_prediction() const {
    std::vector<Vector3> result;
    if (!slingshot_->state().seated) {
        return result;
    }
    JPH::IgnoreMultipleBodiesFilter filter;
    filter.IgnoreBody(player_id_);
    filter.IgnoreBody(kit_->body_id(slingshot_->pouch_body()));
    filter.IgnoreBody(kit_->body_id(kit_->body_for_entity(Slingshot::kRailEntity)));
    filter.IgnoreBody(kit_->body_id(kit_->body_for_entity(Slingshot::kLaunchRailEntity)));
    filter.IgnoreBody(kit_->body_id(slingshot_->frame_body()));
    const std::vector<JPH::RVec3> points = slingshot_->prediction();
    if (points.empty()) {
        return result;
    }
    const auto to_vector = [](const JPH::RVec3 p) {
        return Vector3{static_cast<double>(p.GetX()), static_cast<double>(p.GetY()),
                       static_cast<double>(p.GetZ())};
    };
    result.push_back(to_vector(points.front()));
    for (std::size_t n = 1; n < points.size(); ++n) {
        const JPH::RVec3 from = points[n - 1];
        const JPH::Vec3 displacement(points[n] - from);
        JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
        const JPH::RShapeCast sweep(player_shape_.GetPtr(), JPH::Vec3::sReplicate(1.0F),
                                    JPH::RMat44::sTranslation(from), displacement);
        physics_system_.GetNarrowPhaseQuery().CastShape(sweep, JPH::ShapeCastSettings(), from,
                                                        collector, {}, {}, filter);
        if (collector.HadHit()) {
            result.push_back(to_vector(from + displacement * collector.mHit.mFraction));
            break;
        }
        result.push_back(to_vector(points[n]));
    }
    return result;
}

void Simulation::PhysicsWorld::remove_and_destroy(JPH::BodyInterface &bodies, const JPH::BodyID body_id) {
    // A proving-ground fixture is never built in the game's world.
    if (body_id.IsInvalid()) {
        return;
    }
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

JPH::Vec3 Simulation::PhysicsWorld::normalized_horizontal(const double x, const double z) noexcept {
    const double length = std::hypot(x, z);
    if (!(length > 1.0e-6)) {
        return JPH::Vec3::sZero();
    }
    return JPH::Vec3(static_cast<float>(x / length), 0.0F, static_cast<float>(z / length));
}

JPH::BodyID Simulation::PhysicsWorld::body_id_for_entity(const std::uint64_t entity_id) const noexcept {
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
    default:
        // A kit body: the rider stands on what it does (plan rule 4), so
        // a deck carried sideways carries them with it.
        if (scraperx::sim::kit::is_kit_entity(entity_id)) {
            const auto body = kit_->body_for_entity(entity_id);
            if (body.valid()) {
                return kit_->body_id(body);
            }
        }
        return {};
    }
}

// Everything the player can see is something the player can hit. The
// table is generated from the Godot builders (godot/presentation/
// solid_export.gd), so the world these tests walk is the world that is
// drawn. Built last, so a drawn mirror of a body already owned here is
// recognised by its bounds and skipped rather than doubled -- a second
// coincident body would split ledge probes and support identity.
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

// The stack: the tower's lower section, as real static collision. A
// perimeter deck ring per level round an open 34 m shaft, and corner and
// mid-span columns carrying each deck. There is no stair and no ramp: a
// level is gained by a machine or a climb.
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

// track_for_teardown=false hands ownership entirely to the caller (the
// carry, created and removed at runtime): Jolt's ConstraintManager::Remove
// asserts on an already-
// invalidated constraint index, so a constraint that might be removed
// before the destructor runs must never also sit in machine_constraints_,
// which is unconditionally removed there once. Exactly one owner removes
// it, on every path.
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
    const JPH::BodyID support_id = body_id_for_entity(support_entity_id_);
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

} // namespace scraperx::sim
