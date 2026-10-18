#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

namespace {
bool braking_horizontal_motion(double move_x, double move_z,
                               JPH::Vec3Arg relative) noexcept {
    return std::hypot(move_x, move_z) > .001 &&
        move_x * relative.GetX() + move_z * relative.GetZ() < 0;
}
} // namespace

void Simulation::PhysicsWorld::prepare_trailing_plant(const StepCommands &commands) noexcept {
    const bool trailing = foot_push_purpose_ == FootPushPurpose::TrailingPlant;
    const bool commanded = foot_push_purpose_ == FootPushPurpose::CoyotePush;
    const bool wall_kick = foot_push_purpose_ == FootPushPurpose::WallKick;
    const bool wall_stride = foot_push_purpose_ == FootPushPurpose::WallStride;
    if (!trailing && !commanded && !wall_kick && !wall_stride) return;
    const bool cancelled = commands.cancel_jump_requested || commands.release_requested ||
        commands.set_down_requested || commands.sling_drop || commands.crouch_held ||
        traversal_state_ != TraversalState::None || physical_hands_->active() ||
        carry_constraint_ != nullptr || (slingshot_ && slingshot_->controls_player());
    const bool same_contact = foot_contact_.valid &&
        foot_contact_.solved_tick == contact_solve_serial_ &&
        foot_contact_.body == foot_push_contact_.body &&
        foot_contact_.subshape == foot_push_contact_.subshape;
    // Keep the first actual edge plant instead of rolling its anchor/rest
    // target down the rounded capsule every grounded tick. Fresh real contact
    // updates only input age; it never renews the actuator's original lifetime.
    if (trailing && grounded_ && same_contact)
        trailing_foot_contact_tick_ = contact_solve_serial_;
    const bool expired = !grounded_ &&
        contact_solve_serial_ - trailing_foot_contact_tick_ > kCoyotePlantIntentTicks;
    const bool ordinary_jump = grounded_ &&
        (commands.jump_requested || ordinary_jump_ticks_left_ > 0);
    bool returning_on_contact = false;
    if (trailing && grounded_ && same_contact) {
        const auto &bodies = physics_system_.GetBodyInterface();
        const auto anchor = from_support_local(bodies, foot_contact_.body, foot_contact_.local_point);
        const auto relative = bodies.GetLinearVelocity(player_id_) -
            support_point_velocity(bodies, foot_contact_.body, anchor);
        returning_on_contact = braking_horizontal_motion(
            commands.move_input_x, commands.move_input_z, relative);
    }
    // A trailing plant preserves an outgoing step. During deliberate return
    // input its compression can instead lift the capsule off a rounded edge
    // before ordinary traction/stepping recovers. Release only this passive
    // grounded topology; neutral/airborne plants and commanded pushes remain.
    bool stride_cancelled = wall_stride && grounded_;
    if (wall_stride && physical_foot_push_->active() &&
        physical_foot_push_->readback().walking_step && !commands.jump_requested) {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), foot_push_contact_.body);
        if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase()) stride_cancelled = true;
        else {
            const auto &wall = lock.GetBody();
            const auto anchor = wall.GetCenterOfMassTransform()*foot_push_contact_.local_point;
            const auto relative = physics_system_.GetBodyInterface().GetLinearVelocity(player_id_) -
                wall.GetPointVelocity(anchor);
            stride_cancelled = stride_cancelled || !commands.sprint_held ||
                std::hypot(commands.move_input_x, commands.move_input_z) < kSprintMinimumInput ||
                commands.move_input_x*relative.GetX() + commands.move_input_z*relative.GetZ() <= 0;
        }
    }
    if (cancelled || stride_cancelled || (trailing && (expired || ordinary_jump || returning_on_contact ||
                                  (grounded_ && !same_contact)))) {
        physical_foot_push_->clear();
        foot_push_purpose_ = FootPushPurpose::None;
    }
}

void Simulation::PhysicsWorld::retain_trailing_plant(const StepCommands &commands, bool jump_started) noexcept {
    if (jump_started || physical_foot_push_->active() || !grounded_ || crouched_ ||
        traversal_state_ != TraversalState::None || physical_hands_->active() || carry_constraint_ ||
        commands.cancel_jump_requested || commands.release_requested || commands.set_down_requested ||
        commands.sling_drop || commands.crouch_held || (slingshot_ && slingshot_->controls_player()) ||
        !foot_contact_.valid || foot_contact_.solved_tick != contact_solve_serial_ ||
        (deformable_plank_ && deformable_plank_->logical_member_for_body(foot_contact_.body))) return;
    auto &bodies = physics_system_.GetBodyInterface();
    const auto anchor = from_support_local(bodies, foot_contact_.body, foot_contact_.local_point);
    const auto at = bodies.GetCenterOfMassPosition(player_id_);
    const JPH::Vec3 delta(at - anchor);
    // Coverage ranks an edge opportunity only. The fresh solved bottom
    // contact above supplies the material anchor; no ray creates a foothold.
    // Waiting for half a capsule radius of lateral offset captures too late,
    // after the capsule has already rolled and accelerated down the edge.
    const JPH::Vec3 horizontal(delta.GetX(), 0, delta.GetZ());
    const auto relative = bodies.GetLinearVelocity(player_id_) -
        support_point_velocity(bodies, foot_contact_.body, anchor);
    if (horizontal.Dot(relative) <= 0 ||
        braking_horizontal_motion(commands.move_input_x, commands.move_input_z, relative) ||
        footing_is_firm(bodies) ||
        delta.GetY() < kPlayerHalfHeight - kPlayerRadius - kStandClearanceSkin ||
        delta.GetY() > kPlayerHalfHeight + kStandClearanceSkin) return;
    {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), foot_contact_.body);
        if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase() || lock.GetBody().IsSensor() ||
            lock.GetBody().GetShape() != foot_contact_.shape_identity ||
            lock.GetBody().GetUserData() != foot_contact_.entity_id) return;
    }
    const auto normal = bodies.GetRotation(foot_contact_.body) * foot_contact_.local_normal;
    if (!physical_foot_push_->begin_passive_plant(foot_contact_.body, anchor, normal, foot_contact_.friction)) return;
    foot_push_contact_ = foot_contact_;
    foot_push_purpose_ = FootPushPurpose::TrailingPlant;
    trailing_foot_contact_tick_ = contact_solve_serial_;
}

bool Simulation::PhysicsWorld::try_begin_coyote_push(JPH::BodyInterface &bodies) noexcept {
    if (grounded_ || ordinary_jump_ticks_left_ == 0 || crouched_ ||
        foot_push_purpose_ != FootPushPurpose::TrailingPlant || !physical_foot_push_->active() ||
        contact_solve_serial_ - trailing_foot_contact_tick_ > kCoyotePlantIntentTicks) return false;
    refresh_rigid_transfer_foot();
    if (!rigid_transfer_foot_valid_ || !physical_foot_push_->activate_planted_push()) return false;
    foot_push_purpose_ = FootPushPurpose::CoyotePush;
    ordinary_jump_ticks_left_ = 0;
    foot_push_started_this_tick_ = true;
    ++foot_push_start_count_;
    const auto anchor = physical_foot_push_->readback().world_anchor;
    airborne_inherited_velocity_ = support_point_velocity(bodies, foot_push_contact_.body, anchor);
    const auto departure = bodies.GetLinearVelocity(player_id_) - airborne_inherited_velocity_;
    air_full_speed_ = std::max(kPlayerMaximumRelativeSpeed,
        JPH::Vec3(departure.GetX(), 0, departure.GetZ()).Length());
    causal_airborne_ = true;
    jump_takeoff_feet_y_ = float(bodies.GetPosition(player_id_).GetY()) - kPlayerHalfHeight;
    jump_vault_ticks_left_ = kJumpVaultWindowTicks;
    clear_landing_recovery();
    return true;
}

bool Simulation::PhysicsWorld::try_begin_wall_kick(JPH::BodyInterface &bodies) noexcept {
    if (grounded_ || ordinary_jump_ticks_left_ == 0 || crouched_ || physical_hands_->active() ||
        carry_constraint_ || traversal_state_ != TraversalState::None ||
        (slingshot_ && slingshot_->controls_player())) return false;
    if (foot_push_purpose_ == FootPushPurpose::WallStride && physical_foot_push_->active()) {
        if (!physical_foot_push_->push_off_existing_step()) return false;
        ordinary_jump_ticks_left_ = 0;
        clear_landing_recovery();
        return true;
    }
    return begin_wall_foot(bodies, false);
}

void Simulation::PhysicsWorld::arm_wall_run() noexcept {
    wall_run_ = {};
    const float gravity = physics_system_.GetGravity().Length();
    if (!(gravity > 0)) return;
    wall_run_.phase = WallRunEpisode::Phase::Armed;
    // Two finite foot efforts may extend the ordinary earned flight window.
    // The original takeoff owns this deadline; replanting never renews it.
    const double seconds = 2.0 * kJumpSpeed / gravity +
        2.0 * PhysicalFootPush::duration_seconds();
    wall_run_.deadline_solve = contact_solve_serial_ +
        std::uint64_t(std::ceil(seconds * Simulation::kTickRateHz));
}

bool Simulation::PhysicsWorld::wall_run_contact_is_distinct() const noexcept {
    const auto &first = wall_run_.first;
    const auto &next = wall_contact_;
    if (!next.valid || next.solved_tick != contact_solve_serial_ ||
        next.body != first.body || next.entity_id != first.entity_id ||
        next.subshape != first.subshape || next.shape_identity != first.shape_identity ||
        next.local_normal.Dot(first.local_normal) < .995F) return false;
    // Only material progress counts. A skin-sized displacement rejects
    // same-point jitter; the actual second start also requires half stroke.
    return (next.local_point-first.local_point).Dot(wall_run_.local_direction) > kLandingSkin;
}

void Simulation::PhysicsWorld::update_wall_run(JPH::BodyInterface &bodies,
                                               const StepCommands &commands) noexcept {
    using Phase = WallRunEpisode::Phase;
    if (wall_run_.phase == Phase::None) return;
    const bool cancelled = grounded_ || commands.jump_requested || commands.release_requested ||
        commands.cancel_jump_requested || commands.set_down_requested || commands.sling_drop ||
        commands.crouch_held || !sprint_is_eligible(commands) || physical_hands_->active() ||
        carry_constraint_ || traversal_state_ != TraversalState::None ||
        (slingshot_ && slingshot_->controls_player()) ||
        contact_solve_serial_ > wall_run_.deadline_solve;
    if (cancelled) { wall_run_ = {}; return; }
    if (wall_run_.phase == Phase::Armed) return;
    if (wall_run_.phase == Phase::SecondPlant) return;
    const auto &first = wall_run_.first;
    JPH::Vec3 normal, direction, support_velocity;
    {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(),first.body);
        if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase() || lock.GetBody().IsSensor() ||
            lock.GetBody().GetShape() != first.shape_identity ||
            lock.GetBody().GetUserData() != first.entity_id) { wall_run_ = {}; return; }
        const auto &wall = lock.GetBody();
        normal = wall.GetRotation()*first.local_normal;
        direction = wall.GetRotation()*wall_run_.local_direction;
        const auto point = wall.GetCenterOfMassTransform()*first.local_point;
        support_velocity = wall.GetPointVelocity(point);
    }
    const auto relative = bodies.GetLinearVelocity(player_id_)-support_velocity;
    const auto intent = JPH::Vec3(float(commands.move_input_x),0,float(commands.move_input_z));
    // A deliberate lean into the wall authorizes the second step. The
    // existing light-brush single-plant path remains an ordinary departure.
    if (-intent.Dot(normal) <= kClimbInputDeadzone ||
        intent.Dot(direction) < kSprintMaximumAngleCos || relative.Dot(direction) <= 0) {
        wall_run_ = {};
        return;
    }
    if (wall_run_.phase == Phase::FirstPlant) {
        const auto &read = physical_foot_push_->readback();
        if (!read.active || read.stop_requested || !read.walking_step) { wall_run_ = {}; return; }
        if (!wall_run_contact_is_distinct() || read.last_load_n <= 0) return;
        const float progress = (wall_contact_.local_point-first.local_point).Dot(wall_run_.local_direction);
        const float next_progress = progress + std::max(0.0F,relative.Dot(direction)) *
            float(Simulation::kFixedStepSeconds);
        // Prediction schedules release only. It cannot create the next foot:
        // a later actual solved material point still needs half a stroke.
        if (next_progress < .5F * PhysicalFootPush::maximum_stroke_m()) return;
        // Validate through the actual actuator without mutating its cached
        // geometry or disguising a traction/reach failure as another step.
        if (!physical_foot_push_->can_retain_contact()) { wall_run_ = {}; return; }
        physical_foot_push_->clear();
        foot_push_purpose_ = FootPushPurpose::None;
        wall_run_.phase = Phase::AwaitingContact;
        wall_run_.retired_solve = contact_solve_serial_;
        return;
    }
    if (contact_solve_serial_ <= wall_run_.retired_solve) return;
    // A real solve without the first row must supply the new contact.
    // A missed wall, changed face or stale point ends the episode.
    if (!wall_run_contact_is_distinct() || physical_foot_push_->active()) { wall_run_ = {}; return; }
    const float progress = (wall_contact_.local_point-first.local_point).Dot(wall_run_.local_direction);
    if (progress < .5F * PhysicalFootPush::maximum_stroke_m()) return;
    const auto consumed = wall_kick_consumed_body_;
    wall_kick_consumed_body_ = JPH::BodyID();
    if (!begin_wall_foot(bodies,true,direction)) {
        wall_kick_consumed_body_ = consumed;
        wall_run_ = {};
        return;
    }
    wall_run_.phase = Phase::SecondPlant;
}

bool Simulation::PhysicsWorld::try_begin_wall_stride(JPH::BodyInterface &bodies,
                                                     const StepCommands &commands) noexcept {
    if (!wall_stride_available_ || grounded_ || !sprint_is_eligible(commands) ||
        !wall_contact_.valid || wall_contact_.solved_tick != contact_solve_serial_) return false;
    JPH::Vec3 normal, relative;
    {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), wall_contact_.body);
        if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase() || lock.GetBody().IsSensor() ||
            !lock.GetBody().IsRigidBody() || lock.GetBody().GetShape() != wall_contact_.shape_identity ||
            lock.GetBody().GetUserData() != wall_contact_.entity_id) return false;
        const auto &wall = lock.GetBody();
        const auto anchor = wall.GetCenterOfMassTransform()*wall_contact_.local_point;
        normal = wall.GetRotation()*wall_contact_.local_normal;
        relative = bodies.GetLinearVelocity(player_id_)-wall.GetPointVelocity(anchor);
    }
    // Let the earned takeoff rise normally. At its apex the real wall foot can
    // supply a finite step; upward body speed is never replaced or clamped.
    if (relative.GetY() > 0) return false;
    const auto horizontal_normal = JPH::Vec3(normal.GetX(),0,normal.GetZ()).NormalizedOr(JPH::Vec3::sZero());
    const double inward_intent = -(horizontal_normal.GetX()*commands.move_input_x +
        horizontal_normal.GetZ()*commands.move_input_z);
    if (inward_intent >= kSprintMaximumAngleCos) {
        // A committed takeoff into a real wall may spend its one finite
        // plant pushing up and away. The existing radial motor supplies
        // both components; it cannot adhere to the wall or renew the step.
        if (!begin_wall_foot(bodies,false)) return false;
        foot_push_purpose_ = FootPushPurpose::WallStride;
        ++wall_stride_count_;
        return true;
    }
    const JPH::Vec3 horizontal(relative.GetX(),0,relative.GetZ());
    const auto tangent = horizontal-horizontal_normal*horizontal.Dot(horizontal_normal);
    const double speed = tangent.Length();
    if (speed < kPlayerMaximumRelativeSpeed ||
        tangent.GetX()*commands.move_input_x + tangent.GetZ()*commands.move_input_z <
            kSprintMaximumAngleCos*speed) return false;
    return begin_wall_foot(bodies, true, tangent/float(speed));
}

bool Simulation::PhysicsWorld::begin_wall_foot(JPH::BodyInterface &bodies, const bool stride,
                                              const JPH::Vec3 direction) noexcept {
    if (grounded_ || crouched_ || physical_foot_push_->active() || physical_hands_->active() ||
        carry_constraint_ || traversal_state_ != TraversalState::None ||
        (slingshot_ && slingshot_->controls_player()) || !wall_contact_.valid ||
        wall_contact_.solved_tick != contact_solve_serial_ ||
        wall_contact_.body == wall_kick_consumed_body_) return false;
    {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(),wall_contact_.body);
        if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase() || lock.GetBody().IsSensor() ||
            !lock.GetBody().IsRigidBody() || lock.GetBody().GetShape() != wall_contact_.shape_identity ||
            lock.GetBody().GetUserData() != wall_contact_.entity_id) return false;
    }
    const auto anchor = from_support_local(bodies,wall_contact_.body,wall_contact_.local_point);
    const auto normal = bodies.GetRotation(wall_contact_.body)*wall_contact_.local_normal;
    const auto delta = JPH::Vec3(bodies.GetCenterOfMassPosition(player_id_)-anchor);
    const float height = delta.Dot(normal);
    const float tangent = (delta-normal*height).Length();
    const float contact_depth = kPlayerRadius+
        (kPlayerHalfHeight-kPlayerRadius)*std::abs(normal.GetY())+kLandingSkin;
    if (std::abs(normal.GetY()) > .2F || delta.GetY() < .05F || height <= 0 ||
        height > contact_depth || tangent > PhysicalFootPush::lateral_reach_m() ||
        tangent > wall_contact_.friction*height) return false;
    if (stride) {
        if (!physical_foot_push_->begin_walking_step(wall_contact_.body,anchor,normal,
                wall_contact_.friction,PhysicalFootPush::maximum_stroke_m(),direction,
                PhysicalFootPush::maximum_stroke_m())) return false;
    } else {
        if (!physical_foot_push_->begin_passive_plant(wall_contact_.body,anchor,normal,
                                                    wall_contact_.friction)) return false;
        if (!physical_foot_push_->activate_planted_push()) {
            physical_foot_push_->clear();
            return false;
        }
    }
    foot_push_contact_ = wall_contact_;
    foot_push_contact_.world_point = anchor;
    foot_push_contact_.world_normal = normal;
    foot_push_purpose_ = stride ? FootPushPurpose::WallStride : FootPushPurpose::WallKick;
    if (stride && wall_run_.phase == WallRunEpisode::Phase::Armed) {
        wall_run_.phase = WallRunEpisode::Phase::FirstPlant;
        wall_run_.first = wall_contact_;
        wall_run_.local_direction = bodies.GetRotation(wall_contact_.body).Conjugated()*direction;
    } else if (!stride) wall_run_ = {};
    wall_kick_consumed_body_ = wall_contact_.body;
    wall_stride_available_ = false;
    if (stride) ++wall_stride_count_;
    ordinary_jump_ticks_left_ = 0;
    foot_push_started_this_tick_ = true;
    ++foot_push_start_count_;
    refresh_rigid_transfer_foot();
    airborne_inherited_velocity_ = support_point_velocity(bodies,wall_contact_.body,anchor);
    const auto departure = bodies.GetLinearVelocity(player_id_)-airborne_inherited_velocity_;
    air_full_speed_ = std::max(kPlayerMaximumRelativeSpeed,
        JPH::Vec3(departure.GetX(),0,departure.GetZ()).Length());
    causal_airborne_ = true;
    jump_takeoff_feet_y_ = float(bodies.GetPosition(player_id_).GetY())-kPlayerHalfHeight;
    jump_vault_ticks_left_ = kJumpVaultWindowTicks;
    clear_landing_recovery();
    return true;
}
} // namespace scraperx::sim
