// What the player sees and keeps: the precision sight, affordances, checkpoints, saves, the snapshot.
// Members of Simulation::PhysicsWorld (physics_world.hpp).

#include "sim/physics_world.hpp"

namespace scraperx::sim {

// M3: the landing the eyes are on, if any (see kPrecisionMinM). The look
// is cast from the eye to the ground; what it meets is the sighted point
// when it lies in the jump's reach, is level enough to stand on, and a
// standing capsule fits there.
void Simulation::PhysicsWorld::update_precision_sight(const JPH::BodyInterface &bodies) noexcept {
    precision_sighted_ = false;
    if (!grounded_ || crouched_ || sliding_ || traversal_state_ != TraversalState::None ||
        carry_constraint_ != nullptr || facing_.IsNearZero() || look_pitch_ > -0.05F) {
        return;
    }
    const JPH::RVec3 centre = bodies.GetPosition(player_id_);
    const JPH::RVec3 eye(centre.GetX(), centre.GetY() + kPlayerHalfHeight - 0.15F, centre.GetZ());
    const float cos_p = std::cos(look_pitch_);
    const JPH::Vec3 look(facing_.GetX() * cos_p, std::sin(look_pitch_), facing_.GetZ() * cos_p);
    JPH::RayCastResult hit;
    if (!cast_ray(eye, look * kPrecisionLookMaxM, hit)) {
        return;
    }
    const JPH::RVec3 point = JPH::RRayCast(eye, look * kPrecisionLookMaxM).GetPointOnRay(hit.mFraction);
    if (surface_normal(hit.mBodyID, hit.mSubShapeID2, point).GetY() < kLedgeTopNormalThreshold) {
        return;
    }
    const float feet_y = static_cast<float>(centre.GetY()) - kPlayerHalfHeight;
    const float rise = static_cast<float>(point.GetY()) - feet_y;
    const float ahead = static_cast<float>(std::hypot(point.GetX() - centre.GetX(), point.GetZ() - centre.GetZ()));
    if (ahead < kPrecisionMinM || ahead > kPrecisionMaxM || rise > kPrecisionRiseMaxM ||
        rise < -kPrecisionDropMaxM) {
        return;
    }
    const JPH::RVec3 stand(point.GetX(), point.GetY() + kPlayerHalfHeight + kLandingSkin, point.GetZ());
    if (!capsule_pose_is_clear(stand)) {
        return;
    }
    precision_sighted_ = true;
    precision_point_ = point;
}

void Simulation::PhysicsWorld::update_affordance(const JPH::BodyInterface &bodies) noexcept {
    affordance_ = {};
    affordance_vault_ = false;
    grip_affordance_ = {};
    edge_affordance_ = {};
    carry_target_entity_ = 0;
    (void)carry_candidate(bodies, carry_target_entity_);
    find_rig_action(bodies);
    if (traversal_state_ == TraversalState::None && !crouched_ &&
        carry_constraint_ == nullptr && !facing_.IsNearZero() && grounded_) {
        grip_affordance_ = grip_in_front(bodies);
        edge_affordance_ = probe_edge_drop(bodies);
    }
    // The probes measure rises from standing feet and test standing
    // landing poses; a crouched body is offered none (a request stands
    // it first, see update_crouch). Hands full, no ledge is offered at all.
    if (traversal_state_ != TraversalState::None || facing_.IsNearZero() || crouched_ ||
        carry_constraint_ != nullptr) {
        return;
    }

    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    if (grounded_) {
        const LedgeProbe vault_probe =
            probe_ledge(origin, facing_, kVaultMinimumRise, kVaultMaximumRise, false, false, vault_reach(bodies));
        if (vault_probe.valid) {
            JPH::RVec3 landing_centre;
            JPH::BodyID landing_body;
            if (probe_vault_landing(vault_probe,
                                    facing_,
                                    origin.GetY() - kPlayerHalfHeight,
                                    landing_centre,
                                    landing_body)) {
                affordance_ = vault_probe;
                affordance_vault_ = true;
                return;
            }
        }
        affordance_ = probe_ledge(origin, facing_, kMantleMinimumRise, kMantleMaximumRise, true);
        return;
    }

    affordance_ = probe_ledge(origin,
                              facing_,
                              kPlayerHalfHeight + kHangMinimumRiseAboveCentre,
                              kPlayerHalfHeight + kHangMaximumRiseAboveCentre,
                              true);
}

// What request_up would do from here, in the order a runner means it.
// Hands on something: what they do with it. In the air: a kick off the wall
// being run, the trapeze's bar in reach, else a jump held for the landing
// (or the vault the takeoff could have been). Crouched: stand; a slide is
// jumped out of at its speed. On the ground: the landing the eyes are on;
// a run's springboard off a top close ahead; a vault over what is ahead; a
// mantle up on to it; the hold ahead taken; else a jump. So a box deep
// enough to plant a foot on is springboarded at a run, and a rail too thin
// for one is vaulted; a ledge in reach is mantled, not jumped at.
void Simulation::PhysicsWorld::update_up_move(const JPH::BodyInterface &bodies) noexcept {
    up_move_ = UpMove::None;
    if (trapeze_->controls_player()) {
        up_move_ = UpMove::LetGo;
        return;
    }
    if (slingshot_->controls_player() || swing_->controls_player()) {
        return;
    }
    // At a machine's controls the stick and the buttons are the machine's;
    // thrown off the slingshot, the throw is the body's way.
    if (yard_crane_->operating() || (slingshot_flight_ && !grounded_)) {
        return;
    }
    for (const std::unique_ptr<Forklift> &truck : forklifts_) {
        if (truck->driving()) {
            return;
        }
    }
    switch (traversal_state_) {
    case TraversalState::None:
        break;
    case TraversalState::Hanging:
        up_move_ = UpMove::ClimbUp;
        return;
    case TraversalState::Swinging:
        up_move_ = UpMove::Launch;
        return;
    case TraversalState::Climbing:
        up_move_ = top_out_ahead(bodies).valid ? UpMove::TopOut : UpMove::JumpOff;
        return;
    case TraversalState::Mantling:
    case TraversalState::Vaulting:
    case TraversalState::Lowering:
        return;
    }
    if (!grounded_ && coyote_ticks_ == 0) {
        const Trapeze::State &bar = trapeze_->state();
        // The run's hold counts down at the top of the next tick, before its
        // kick is tried: offered only where it will still be running then.
        if (wall_run_seconds_left_ > kFixedStepSeconds && wall_kicks_this_flight_ < kWallKicksMax) {
            up_move_ = UpMove::Kick;
        } else if (bar.catch_available || bar.catch_near) {
            up_move_ = UpMove::Catch;
        } else {
            up_move_ = UpMove::Jump;
        }
        return;
    }
    if (!grounded_) {
        // Walked (or slid, or crouch-walked) off an edge, still in the coyote
        // ticks: a jump, standing the body first as update_crouch does.
        up_move_ = UpMove::Jump;
        return;
    }
    if (crouched_) {
        // Up from a crouch stands, and from a slide jumps, where the body has
        // room to stand (try_stand's own test); under a low beam, nothing.
        const JPH::RVec3 standing = bodies.GetPosition(player_id_) + JPH::Vec3(0.0F, kCrouchDrop, 0.0F);
        if (shape_pose_is_clear(player_shape_.GetPtr(), standing + JPH::Vec3(0.0F, kStandClearanceSkin, 0.0F))) {
            up_move_ = sliding_ ? UpMove::Jump : UpMove::Stand;
        }
        return;
    }
    if (holding_throwable()) {
        // With a piece of loose stock in the hands, up throws it.
        up_move_ = UpMove::Throw;
        return;
    }
    if (precision_sighted_) {
        up_move_ = UpMove::Precision;
        return;
    }
    const JPH::Vec3 reference =
        support_entity_id_ != 0 ? current_support_point_velocity(bodies) : airborne_inherited_velocity_;
    if (springboard_ahead(bodies, bodies.GetLinearVelocity(player_id_) - reference).valid) {
        up_move_ = UpMove::Springboard;
        return;
    }
    if (affordance_.valid) {
        up_move_ = affordance_vault_ ? UpMove::Vault : UpMove::Mantle;
        return;
    }
    up_move_ = grip_affordance_.valid ? UpMove::Climb : UpMove::Jump;
}

// Machine half of a checkpoint (TDD 14.1: "machine/control state"): the
// carry and every kit body. Kinematic bodies are pure functions of the
// tick counter and need no restoring.
void Simulation::PhysicsWorld::commit_machine_checkpoint() noexcept {
    checkpoint_.carrying_entity = carried_entity_;
    kit_->capture(checkpoint_.kit);
    checkpoint_.slingshot = slingshot_->state();
    checkpoint_.swing = swing_->state();
    checkpoint_.vertical = vertical_->state();
    checkpoint_.matron = matron_->save();
    checkpoint_.bay = bay_->capture();
}

// WO-008 automatic commit (GDD 9.1): every tick the player is grounded on
// firm footing (kCheckpointFootingNormalY) and not mid-traversal, so the
// checkpoint is always "wherever the player was last standing." No dwell
// timer, no player-facing save action.
void Simulation::PhysicsWorld::commit_checkpoint(const JPH::BodyInterface &bodies) noexcept {
    checkpoint_position_ = bodies.GetPosition(player_id_);
    checkpoint_crouched_ = crouched_;
    commit_machine_checkpoint();
    ++checkpoint_commit_count_;
}

// WO-008 death restore (Governing Laws 9, 21): the one sanctioned
// exception to "no hidden teleportation," explicitly named by Law 21
// itself. Restores player and every captured machine body, then clears
// this tick's now-stale contact/traversal-adjacent state so the next
// tick re-establishes ground truth from a fresh contact pass rather than
// publishing a snapshot that mixes a teleported position with a contact
// sample that referred to the pre-restore position.
void Simulation::PhysicsWorld::restore_from_checkpoint(JPH::BodyInterface &bodies) noexcept {
    // Out of the slingshot's harness, and the hands off whatever they hold
    // (a ledge, a climb's holds, a bar, a zipline's trolley), before the body
    // moves: the checkpoint is a standing one. The Matron's hands too, before
    // any stock is put back.
    matron_->drop_stock();
    slingshot_->detach();
    swing_->detach();
    trapeze_->detach();
    if (traversal_state_ != TraversalState::None) {
        bodies.SetGravityFactor(player_id_, 1.0F);
        clear_traversal();
    }
    // Back on the feet before the hands reach for anything: put back beside a
    // ladder with the stick still held (as it was at the death), the first
    // tick, not yet grounded, would catch it and hang the body there.
    regrab_lockout_ticks_ = kReleaseRegrabLockoutTicks;
    slingshot_flight_ = false;
    // The capsule the checkpoint was committed in: a crouched commit's
    // centre is a crouched centre, and may sit under a low ceiling.
    crouched_ = checkpoint_crouched_;
    bodies.SetShape(player_id_, active_player_shape(), false, JPH::EActivation::Activate);
    bodies.SetPositionAndRotation(player_id_, checkpoint_position_, JPH::Quat::sIdentity(),
                                  JPH::EActivation::Activate);
    bodies.SetLinearAndAngularVelocity(player_id_, JPH::Vec3::sZero(), JPH::Vec3::sZero());

    // A kit body in the hands may be one the restore takes out of the
    // world (a shackle hooked at the commit): let go of it first.
    if (carry_constraint_ != nullptr &&
        scraperx::sim::kit::is_kit_entity(carried_entity_)) {
        release_carry();
    }
    kit_->restore(checkpoint_.kit);
    slingshot_->restore(checkpoint_.slingshot);
    swing_->restore(checkpoint_.swing);
    trapeze_->restore();
    vertical_->restore(checkpoint_.vertical);
    matron_->restore(checkpoint_.matron);
    // After her restore, which lets go of the district: its drives as saved.
    bay_->restore(checkpoint_.bay);
    impacts_->resync();
    up_move_ = UpMove::None;   // chosen afresh from the restored body by the next tick
    wall_run_seconds_left_ = 0.0;
    for (const std::unique_ptr<Forklift> &truck : forklifts_) {
        truck->restore();
    }
    yard_crane_->restore();
    zipline_->restore();
    zip_riding_ = false;
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
    sliding_ = false;
    stow_hook(bodies);   // the hook comes back with the body; it is kept
    sync_hook_from_rack();
    clear_recovery();
    clear_support_jolt();
    limp_seconds_left_ = 0.0;   // a whole body: no break carried over
    arm_broken_seconds_left_ = 0.0;
    teetering_ = false;
    flight_from_jump_ = false;
    springboard_takeoff_ = false;
    jump_vault_ticks_left_ = 0;
    jump_buffer_ticks_ = 0;
    support_entity_id_ = 0;
    support_sample_ = {};
    airborne_inherited_velocity_ = JPH::Vec3::sZero();
    air_full_speed_ = kPlayerMaximumRelativeSpeed;
    sprinting_ = false;
    balancing_ = false;
    beam_xcom_ = 0.0F;
    beam_lean_ = JPH::Vec3::sZero();
    line_held_ = false;
    fall_peak_speed_mps_ = 0.0F;
    pre_contact_fall_speed_mps_ = 0.0F;
    parachute_deployed_ = false;
    ++death_count_;
}

// The kit's bodies built with the world, by entity: not the fabricated
// objects after them, which a save carries on their own (write_fabricated).
std::vector<std::uint64_t> Simulation::PhysicsWorld::body_entities() const {
    std::vector<std::uint64_t> out(kit_built_count());
    for (std::uint32_t index = 0; index < out.size(); ++index) {
        out[index] = kit_->body_entity(kit::BodyIndex{index});
    }
    return out;
}

// The committed checkpoint as bytes (see SaveWriter).
void Simulation::PhysicsWorld::write_save(SaveWriter &w) const {
    w.pod(checkpoint_position_);
    w.pod(static_cast<std::uint8_t>(checkpoint_crouched_ ? 1 : 0));
    w.pod(checkpoint_commit_count_);
    w.pod(death_count_);
    w.pod(checkpoint_.carrying_entity);
    // The world's bodies by entity, in order: a load takes the states
    // only when this world begins with the same bodies. The fabricated
    // objects' come last, on their own (write_fabricated).
    w.list(body_entities());
    const auto &k = checkpoint_.kit;
    const std::size_t built = std::min(kit_built_count(), k.bodies.size());
    w.list(std::vector<kit::Kit::BodyState>(k.bodies.begin(), k.bodies.begin() + static_cast<std::ptrdiff_t>(built)));
    w.list(k.rope_anchor);
    w.list(k.dog_floor);
    w.list(k.bin_contents);
    w.bits(k.rope_parted);
    w.bits(k.catch_latched);
    w.bits(k.latch_set);
    w.list(k.piles);
    w.list(k.pool_water);
    w.list(k.cell_air);
    w.list(k.reel_paid);
    w.pod(k.drained);
    w.pod(checkpoint_.slingshot);
    w.pod(checkpoint_.swing);
    w.pod(checkpoint_.bay);
    const auto &v = checkpoint_.vertical;
    w.pod(static_cast<std::uint32_t>(v.controls.size()));
    for (const auto &control : v.controls) {
        w.pod(control.command);
        w.list(control.target_speeds);
    }
    w.list(v.targets);
    // Last, so a save made before the grips (which ends here) still loads:
    // what each grip holds and its rating.
    w.list(k.grip_load);
    w.list(k.grip_rating);
    // After them, the fabricated objects, when there are any (a save with
    // none ends here, as it always has).
    write_fabricated(w);
}

// Reads a saved checkpoint, then restores the world from it as a death
// would (without counting one). Refused, and nothing changed, unless
// every list matches this world's own.
bool Simulation::PhysicsWorld::read_save(SaveReader &r, JPH::BodyInterface &bodies) {
    MachineCheckpoint current{};
    kit_->capture(current.kit);
    current.vertical = vertical_->state();
    MachineCheckpoint loaded{};
    JPH::RVec3 position{};
    std::uint8_t crouched = 0;
    std::uint64_t commits = 0, deaths = 0;
    auto &k = loaded.kit;
    const auto &c = current.kit;
    // The saved world's bodies, by entity, must be this world's first
    // ones in the same order: then every saved state is its body's. A
    // world grown since (bodies built last) starts the new ones as built;
    // one with a body removed or moved is refused.
    std::vector<std::uint64_t> entities;
    const std::vector<std::uint64_t> here = body_entities();
    const std::size_t built = here.size();
    bool ok = r.pod(position) && r.pod(crouched) && crouched <= 1 && r.pod(commits) && r.pod(deaths) &&
              r.pod(loaded.carrying_entity) && r.list_up_to(entities, here.size()) &&
              std::equal(entities.begin(), entities.end(), here.begin()) &&
              r.list_up_to(k.bodies, built) && k.bodies.size() == entities.size() &&
              r.list(k.rope_anchor, c.rope_anchor.size()) && r.list(k.dog_floor, c.dog_floor.size()) &&
              r.list_up_to(k.bin_contents, c.bin_contents.size()) && r.bits(k.rope_parted, c.rope_parted.size()) &&
              r.bits(k.catch_latched, c.catch_latched.size()) && r.bits(k.latch_set, c.latch_set.size()) &&
              r.list_up_to(k.piles, 256) && r.list(k.pool_water, c.pool_water.size()) &&
              r.list(k.cell_air, c.cell_air.size()) && r.list(k.reel_paid, c.reel_paid.size()) &&
              r.pod(k.drained) && r.pod(loaded.slingshot) && r.pod(loaded.swing) && r.pod(loaded.bay) &&
              std::isfinite(loaded.bay.restart_s) && loaded.bay.restart_s >= 0.0F && loaded.bay.supplied <= 1 &&
              loaded.bay.leaf_running <= 1 && loaded.bay.fan_running <= 1;
    std::uint32_t controls = 0;
    ok = ok && r.pod(controls) && controls == current.vertical.controls.size();
    if (ok) {
        loaded.vertical.controls.resize(controls);
        for (std::size_t i = 0; ok && i < controls; ++i) {
            ok = r.pod(loaded.vertical.controls[i].command) &&
                 r.list(loaded.vertical.controls[i].target_speeds,
                        current.vertical.controls[i].target_speeds.size());
        }
    }
    ok = ok && r.list(loaded.vertical.targets, current.vertical.targets.size());
    // A save made before the grips ends here: it holds nothing. One made with
    // fewer (before the spreader) holds nothing in the grips built since.
    if (ok && !r.done()) {
        ok = r.list_up_to(k.grip_load, c.grip_load.size()) &&
             r.list_up_to(k.grip_rating, c.grip_rating.size()) && k.grip_rating.size() == k.grip_load.size() &&
             std::all_of(k.grip_rating.begin(), k.grip_rating.end(),
                         [](const float rating) { return std::isfinite(rating) && rating >= 0.0F; });
    }
    // After the grips, the fabricated objects: a save of a world with none
    // (every save made before them) ends before.
    std::vector<SavedFabricated> saved;
    if (ok && !r.done()) {
        ok = read_fabricated(r, saved);
    }
    ok = ok && r.done();
    // A grip holds a body the save has: one built with its world, or one of
    // its fabricated objects (the kit's last).
    const std::size_t saved_built = entities.size();
    ok = ok && std::all_of(k.grip_load.begin(), k.grip_load.end(), [&](const std::uint32_t load) {
             return load == kit::kNone || load < saved_built + saved.size();
         });
    // The world's fabricated objects must be the save's first ones, the same
    // entity made from the same hull file: the save's further ones are made
    // now, if the physics has room for them; the world's further ones stay
    // as they are, as bodies built since do. Any other is refused.
    for (std::size_t i = 0; ok && i < std::min(fabricated_.size(), saved.size()); ++i) {
        ok = fabricated_[i].entity == saved[i].object.entity && fabricated_[i].data == saved[i].object.data;
    }
    const std::size_t to_make = saved.size() > fabricated_.size() ? saved.size() - fabricated_.size() : 0;
    ok = ok && physics_system_.GetNumBodies() + to_make <= physics_system_.GetMaxBodies();
    // Bodies added to the world since the save was written (always built
    // last, after every body before them) start as built.
    for (std::size_t index = k.bodies.size(); ok && index < built; ++index) {
        k.bodies.push_back(c.bodies[index]);
    }
    // Then the fabricated objects: the save's as it has them, the world's
    // further ones as they are.
    for (std::size_t i = 0; ok && i < std::max(fabricated_.size(), saved.size()); ++i) {
        k.bodies.push_back(i < saved.size() ? saved[i].state : c.bodies[built + i]);
    }
    // A grip on a fabricated object holds it where this world's kit has it,
    // after any bodies built since the save.
    for (std::uint32_t &load : k.grip_load) {
        if (ok && load != kit::kNone && load >= saved_built) {
            load = static_cast<std::uint32_t>(load - saved_built + built);
        }
    }
    for (std::size_t index = k.bin_contents.size(); ok && index < c.bin_contents.size(); ++index) {
        k.bin_contents.push_back(c.bin_contents[index]);
    }
    if (!ok || !std::isfinite(position.GetX()) || !std::isfinite(position.GetY()) ||
        !std::isfinite(position.GetZ())) {
        return false;
    }
    // Taken: the save's fabricated objects the world lacks are made, where
    // its checkpoint has them (the restore below sets them moving as saved).
    for (std::size_t i = fabricated_.size(); i < saved.size(); ++i) {
        Fabricated object = std::move(saved[i].object);
        object.body = build_fabricated(object, saved[i].state.position, saved[i].state.rotation);
        if (!object.body.valid()) {
            // Not for want of room, which was checked: the world keeps a
            // checkpoint of every body it has, and the save is refused.
            kit::Kit::Checkpoint now;
            kit_->capture(now);
            checkpoint_.kit.bodies.resize(std::min(checkpoint_.kit.bodies.size(), kit_built_count() + fabricated_.size()));
            for (std::size_t b = checkpoint_.kit.bodies.size(); b < now.bodies.size(); ++b) {
                checkpoint_.kit.bodies.push_back(now.bodies[b]);
            }
            return false;
        }
        fabricated_.push_back(std::move(object));
    }
    checkpoint_position_ = position;
    checkpoint_crouched_ = crouched == 1;
    checkpoint_ = std::move(loaded);
    has_hook_ = false;   // had in the save only if its gun is off the rack there
    corpses_->clear();   // corpses are not saved: a loaded world has none
    restore_from_checkpoint(bodies);
    checkpoint_commit_count_ = commits;
    death_count_ = deaths;
    read_state();
    return true;
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
    state_.player_tucked = tucked_;
    state_.tuck_count = tuck_count_;
    state_.springboard_count = springboard_count_;
    state_.throw_count = throw_count_;
    state_.player_springboarding = traversal_springboard_;
    state_.player_sliding = sliding_;
    state_.slide_count = slide_count_;
    state_.wall_step_count = wall_step_count_;
    state_.wall_kick_count = wall_kick_count_;
    state_.wall_climb_count = wall_climb_count_;
    state_.corpse_count = static_cast<std::uint32_t>(corpses_->count());
    state_.corpses_spawned = corpses_->spawned();
    state_.death_point = to_vector3(death_point_);
    state_.death_speed_mps = death_speed_mps_;
    state_.death_side_on = death_side_on_;
    state_.hit_count = hit_count_;
    state_.hit_speed_mps = hit_speed_mps_;
    state_.zipline_riding = zip_riding_;
    state_.zipline_line = zipline_->state().line;
    state_.zipline_ride_count = zipline_->state().rides;
    state_.zipline_speed_mps = zipline_->state().speed_mps;
    state_.zipline_along_m = zipline_->state().along_m;
    state_.zipline_braking = zipline_->state().braking;
    state_.zipline_hand_brake = zipline_->state().hand_brake;
    state_.zipline_hands_load_n = zipline_->state().hands_load_n;
    state_.zipline_torn_count = zip_torn_count_;
    state_.player_wall_running = wall_running();
    state_.player_wall_normal = wall_running() ? to_vector3(wall_normal_) : Vector3{};
    state_.player_wall_climbing = wall_climbed_this_flight_ && wall_running();
    state_.has_hook = has_hook_;
    state_.hook_take_available = hook_take_in_reach(bodies);
    state_.hook_state = static_cast<std::uint8_t>(hook_state_);
    state_.hook_position = hook_state_ == HookState::Stowed ? Vector3{} : to_vector3(hook_end(bodies));
    {
        double laid = 0.0;
        for (std::uint32_t bend = 0; bend < hook_bend_count_; ++bend) {
            laid += hook_bends_[bend].laid_m;
        }
        state_.hook_line_length = hook_line_ != nullptr ? hook_line_->GetMaxLength() + laid : 0.0;
        // The line as it lies: chest, the bends nearest-first, the hook.
        static_assert(kHookBendsMax + 2 <= Snapshot::kHookLinePointsMax, "the line's points fit the snapshot");
        std::uint32_t points = 0;
        if (hook_state_ != HookState::Stowed) {
            state_.hook_line_points[points++] = to_vector3(hook_chest(bodies));
            for (std::uint32_t bend = hook_bend_count_; bend > 0; --bend) {
                state_.hook_line_points[points++] = to_vector3(hook_bend_point(bodies, bend - 1));
            }
            state_.hook_line_points[points++] = to_vector3(hook_end(bodies));
        }
        state_.hook_line_point_count = points;
        state_.hook_bend_count = hook_bend_count_;
        state_.hook_wrap_count = hook_wrap_count_;
        state_.hook_unwrap_count = hook_unwrap_count_;
        state_.hook_bends_refused = hook_bends_refused_;
    }
    state_.hook_tension_n = hook_tension_n_;
    state_.hook_fire_count = hook_fire_count_;
    state_.hook_bite_count = hook_bite_count_;
    state_.hook_tear_count = hook_tear_count_;
    static_assert(std::tuple_size<decltype(Snapshot::workers)>::value == Crew::kMaxWorkers,
                  "the snapshot carries every worker the crew can hold");
    state_.worker_count = static_cast<std::uint8_t>(std::min<std::size_t>(crew_->count(), state_.workers.size()));
    for (std::size_t index = 0; index < state_.worker_count; ++index) {
        const Crew::View &view = crew_->view(index);
        WorkerView &out = state_.workers[index];
        out.position = Vector3{view.x, view.y, view.z};
        out.yaw = view.yaw;
        out.state = static_cast<std::uint8_t>(view.state);
        out.speed = view.speed;
        out.stride = view.stride;
        out.grounded = view.grounded;
    }
    state_.worker_yields = crew_->yields();
    state_.worker_startles = crew_->startles();
    state_.worker_stumbles = crew_->stumbles();
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
                traversal_state_ == TraversalState::Lowering ||
                traversal_state_ == TraversalState::Swinging) &&
               !traversal_normal_.IsNearZero()) {
        const JPH::RVec3 lip = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
        const JPH::Vec3 span = traversal_right() * kHangHandSpan;
        state_.traversal_left_hand = to_vector3(lip - span);
        state_.traversal_right_hand = to_vector3(lip + span);
    }
    state_.player_sprinting = sprinting_;
    state_.player_balancing = balancing_;
    state_.beam_xcom = beam_xcom_;
    state_.support_jolt_peak_m = support_jolt_peak_m_;
    state_.support_stumble_count = support_stumble_count_;
    state_.jump_reaction_count = jump_reactions_;
    state_.wind_mps = wind_mps_;
    state_.wind_force_n = wind_force_n_;
    state_.wind_peak_force_n = wind_peak_force_n_;
    state_.shove_count = shove_count_;
    state_.beam_lean = {beam_lean_.GetX(), 0.0, beam_lean_.GetZ()};
    state_.grip_available = grip_affordance_.valid;
    state_.grip_entity_id = grip_affordance_.valid ? grip_affordance_.entity_id : 0;
    state_.grip_point = grip_affordance_.valid ? to_vector3(grip_affordance_.point) : Vector3{};
    state_.edge_drop_available = edge_affordance_.valid;
    state_.edge_drop_point =
        edge_affordance_.valid ? to_vector3(edge_affordance_.ledge) : Vector3{};
    state_.climb_count = climb_count_;
    state_.player_climb_sliding = climb_sliding_;
    state_.climb_slide_count = climb_slide_count_;
    state_.swing_catch_count = swing_catch_count_;
    state_.swing_reach_count = swing_reach_count_;
    state_.swing_launch_count = swing_launch_count_;
    state_.hang_slip_count = hang_slip_count_;

    state_.ledge_available = affordance_.valid;
    state_.ledge_entity_id = affordance_.valid ? affordance_.ledge_entity_id : 0;
    state_.ledge_point = affordance_.valid ? to_vector3(affordance_.ledge_point) : Vector3{};
    state_.ledge_rise_meters = affordance_.valid ? affordance_.rise : 0.0;
    state_.up_move = up_move_;
    state_.up_resolved = up_resolved_;
    state_.up_count = up_count_;

    read_machine_state();

    state_.accepted_traversal_count = accepted_traversal_count_;
    state_.world_solid_bodies = world_solid_bodies_;
    state_.step_up_count = step_up_count_;
    state_.step_up_meters = step_up_meters_;
    state_.step_down_count = step_down_count_;
    state_.jump_vault_count = jump_vault_count_;
    state_.precision_sighted = precision_sighted_;
    state_.precision_point = precision_sighted_ ? to_vector3(precision_point_) : Vector3{};
    state_.precision_jump_count = precision_jump_count_;
    state_.precision_landing_count = precision_landing_count_;
    state_.world_solid_mirrors = world_solid_mirrors_;
    state_.world_solid_rejected = world_solid_rejected_;
    state_.rejected_traversal_count = rejected_traversal_count_;
    state_.aborted_traversal_count = aborted_traversal_count_;

    state_.fall_state = !grounded_
        ? (parachute_deployed_ ? FallState::Parachuting : FallState::Airborne)
        : FallState::Grounded;
    state_.fall_peak_speed_mps = fall_peak_speed_mps_;
    state_.last_impact_speed_mps = last_impact_speed_mps_;
    state_.landing_response = static_cast<std::uint8_t>(landing_response_);
    state_.landing_normal_speed_mps = landing_normal_speed_mps_;
    state_.landing_tangent_speed_mps = landing_tangent_speed_mps_;
    state_.landing_balance_loss = landing_loss_;
    state_.recovery_balance = recovery_balance();
    state_.roll_progress = rolling() ? 1.0 - roll_seconds_left_ / kRollSeconds : 0.0;
    state_.roll_travel_m = roll_travel_m_;
    state_.landing_count = landing_count_;
    state_.roll_count = roll_count_;
    state_.roll_refused_count = roll_refused_count_;
    state_.roll_blocked_count = roll_blocked_count_;
    state_.stumble_count = stumble_count_;
    state_.limping = limping();
    state_.limp_seconds_left = limp_seconds_left_;
    state_.ankle_break_count = ankle_break_count_;
    state_.arm_broken = arm_broken();
    state_.arm_broken_seconds_left = arm_broken_seconds_left_;
    state_.arm_break_count = arm_break_count_;
    state_.broken_arm_side = broken_arm_side_;
    state_.in_water = in_water_;
    state_.swimming = swimming_;
    state_.head_underwater = head_underwater_;
    state_.water_submersion = water_submersion_;
    state_.water_surface_y = water_surface_y_;
    state_.water_pool = in_water_ ? water_pool_ : -1;
    state_.water_entry_count = water_entry_count_;
    state_.water_exit_count = water_exit_count_;
    state_.water_entry_speed_mps = water_entry_speed_mps_;
    state_.swim_kick_count = swim_kick_count_;
    state_.player_teetering = teetering_;
    state_.summit_reached = summit_reached_;
    state_.summit_seconds = summit_seconds_;
    state_.summit_deaths = summit_deaths_;
    state_.teeter_count = teeter_count_;
    state_.parachute_deployed = parachute_deployed_;
    state_.checkpoint_position = to_vector3(checkpoint_position_);
    state_.checkpoint_commit_count = checkpoint_commit_count_;
    state_.death_count = death_count_;
}

} // namespace scraperx::sim
