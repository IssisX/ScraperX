#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

void Simulation::PhysicsWorld::update_crouch(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept {
    if (traversal_state_ != TraversalState::None) {
        return;
    }
    const bool recovery_tuck = grounded_ && landing_recovery_seconds_ > 0 &&
        (landing_response_ == 1 || landing_response_ == 2);
    const bool wants_up = (!commands.crouch_held && !recovery_tuck) ||
        commands.jump_requested || commands.traversal_requested;
    if (crouched_) {
        if (wants_up) {
            (void)try_stand(bodies);
        }
        return;
    }
    if (!wants_up) {
        const JPH::RVec3 at = bodies.GetPosition(player_id_);
        bodies.SetShape(player_id_, player_crouch_shape_.GetPtr(), false,
                        JPH::EActivation::Activate);
        // On contact the existing stance owner preserves planted soles.
        // In flight the knees tuck around the same COM: no position,
        // velocity, impulse or takeoff work is granted by the shape change.
        if (grounded_)
            bodies.SetPosition(player_id_, at - JPH::Vec3(0.0F, kCrouchDrop, 0.0F),
                               JPH::EActivation::Activate);
        crouched_ = true;
    }
}

[[nodiscard]] bool Simulation::PhysicsWorld::try_stand(JPH::BodyInterface &bodies) noexcept {
    const JPH::RVec3 standing = bodies.GetPosition(player_id_) +
        (grounded_ ? JPH::Vec3(0.0F, kCrouchDrop, 0.0F) : JPH::Vec3::sZero());
    const auto clearance_pose = standing + (grounded_ ?
        JPH::Vec3(0.0F, kStandClearanceSkin, 0.0F) : JPH::Vec3::sZero());
    if (!shape_pose_is_clear(player_shape_.GetPtr(), clearance_pose)) {
        return false;
    }
    bodies.SetShape(player_id_, player_shape_.GetPtr(), false, JPH::EActivation::Activate);
    if (grounded_)
        bodies.SetPosition(player_id_, standing, JPH::EActivation::Activate);
    crouched_ = false;
    return true;
}

void Simulation::PhysicsWorld::apply_traversal_commands(JPH::BodyInterface &bodies,
                              const StepCommands &commands) noexcept {
    if (palm_props_active()) return;
    if (traversal_state_ != TraversalState::Hanging || !physical_hands_->compatible_hands() ||
        hands_[0].generation != hang_topout_left_generation_ ||
        hands_[1].generation != hang_topout_right_generation_ ||
        commands.cancel_jump_requested || commands.release_requested ||
        (commands.crouch_held && !commands.jump_requested && !commands.traversal_requested)) {
        hang_topout_ticks_left_ = 0;
    }
    if (traversal_state_ == TraversalState::Lowering && commands.release_requested) {
        // Lowering still owns the hands. Drop cancels that ownership
        // through the same passive, momentum-preserving release as a hang.
        release_hang(bodies);
        return;
    }
    if ((traversal_state_ == TraversalState::Climbing || traversal_state_ == TraversalState::Hanging) &&
        physical_hands_->active() && !physical_hands_->compatible_hands()) {
        // Action may add a separately verified free-hand grip. It never
        // converts the same input transaction into a top-out or jump.
        if (commands.release_requested || commands.jump_requested) {
            if (traversal_state_ == TraversalState::Hanging) release_hang(bodies);
            else let_go_climb(bodies);
        } else if (commands.traversal_requested) {
            const Grip candidate = missing_radial_grip(bodies);
            const unsigned missing = physical_hands_->attached(0) ? 1U : 0U;
            if (candidate.valid && physical_hands_->attach_radial_hand(missing,candidate.body,candidate.point)) {
                hands_[missing].body = candidate.body;
                hands_[missing].local = to_support_local(bodies,candidate.body,candidate.point);
                hands_[missing].valid = true;
                ++hands_[missing].generation;
                second_hand_added_this_tick_ = true;
            } else ++rejected_traversal_count_;
        }
        return;
    }
    // Each accepted action changes its own load topology. Merely requesting
    // a top-out must not unload a radial catch that validation then rejects.
    if (traversal_state_ == TraversalState::Hanging) {
        // Retained legacy regression fixtures do not use the physical
        // transfer reach law. Their explicitly separate path is unchanged.
        const bool physical_topout = !regression_fixtures_ ||
            entity_is_causal_section_support(traversal_entity_id_) ||
            (traversal_entity_id_ == 1901 && physical_hands_->active());
        if (!physical_topout) {
            hang_topout_ticks_left_ = 0;
            if (commands.release_requested) release_hang(bodies);
            else if (commands.jump_requested || commands.traversal_requested)
                begin_mantle_from_hang(bodies);
            return;
        }
        if (commands.release_requested) {
            release_hang(bodies);
        } else if (!commands.cancel_jump_requested) {
            if (commands.jump_requested || commands.traversal_requested) {
                hang_topout_ticks_left_ = kOrdinaryJumpIntentTicks;
                hang_topout_left_generation_ = hands_[0].generation;
                hang_topout_right_generation_ = hands_[1].generation;
            }
            if (hang_topout_ticks_left_ > 0) {
                const auto ledge = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
                const auto shoulder = bodies.GetPosition(player_id_) + JPH::Vec3(0, kClimbHandMid, 0);
                if (JPH::Vec3(ledge - shoulder).Length() <= kTransferShoulderReachM) {
                    // Consume once at real reach. The existing receiver,
                    // clearance and physical handoff checks still decide it.
                    hang_topout_ticks_left_ = 0;
                    begin_mantle_from_hang(bodies);
                } else if (--hang_topout_ticks_left_ == 0) {
                    ++rejected_traversal_count_;
                }
            }
        }
        return;
    }

    if (traversal_state_ == TraversalState::Climbing) {
        if (commands.release_requested) {
            let_go_climb(bodies);
        } else if (physical_hands_->swinging() && (commands.jump_requested || commands.traversal_requested)) {
            let_go_climb(bodies);
        } else if (commands.jump_requested) {
            jump_off_climb(bodies);
        } else if (commands.traversal_requested && !try_top_out(bodies)) {
            ++rejected_traversal_count_;
        }
        return;
    }

    if (traversal_state_ != TraversalState::None) {
        if (commands.traversal_requested || commands.release_requested) {
            ++rejected_traversal_count_;
        }
        return;
    }

    // Still crouched here means update_crouch could not stand the body:
    // every vault, mantle and hang is a standing pose, so none begins.
    if (crouched_) {
        jump_vault_ticks_left_ = 0;
        if (commands.traversal_requested) {
            ++rejected_traversal_count_;
        }
        return;
    }

    // Both hands on a carried body (AS-003 §8.4.3): vault, mantle and hang
    // are all pull-ups, and none begins until it is set down.
    if (carry_constraint_ != nullptr) {
        jump_vault_ticks_left_ = 0;
        if (commands.traversal_requested) {
            ++rejected_traversal_count_;
        }
        return;
    }

    if (jump_vault_ticks_left_ > 0) {
        --jump_vault_ticks_left_;
        if (commands.jump_requested && try_begin_jump_vault(bodies)) {
            jump_vault_ticks_left_ = 0;
            return;
        }
    }

    // Drop with an edge behind: lower over it into a hang.
    if (commands.release_requested && grounded_) {
        if (!try_begin_lowering(bodies)) {
            ++rejected_traversal_count_;
        }
        return;
    }

    // Action: a vault or mantle where one is offered, else a hold faced
    // at hand height is climbed.
    if (commands.traversal_requested && !try_begin_ground_traversal(bodies)) {
        // Action expresses deliberate reach in free flight too. The same
        // actual geometry and finite hands decide whether a catch holds.
        const Grip grip = grip_in_front(bodies);
        if (grip.valid) {
            begin_climb(bodies, grip, true);
        } else {
            ++rejected_traversal_count_;
        }
    }
}

[[nodiscard]] bool Simulation::PhysicsWorld::try_begin_jump_vault(JPH::BodyInterface &bodies) noexcept {
    if (facing_.IsNearZero()) {
        return false;
    }
    const JPH::RVec3 at = bodies.GetPosition(player_id_);
    const JPH::RVec3 takeoff(at.GetX(), jump_takeoff_feet_y_ + kPlayerHalfHeight, at.GetZ());
    const LedgeProbe probe =
        probe_ledge(takeoff, facing_, kVaultMinimumRise, kVaultMaximumRise, false);
    if (!probe.valid) {
        return false;
    }
    if(!regression_fixtures_ || entity_is_causal_section_support(probe.ledge_entity_id))
        return begin_cargo_transfer(bodies,probe,at);
    JPH::RVec3 landing_centre;
    JPH::BodyID landing_body;
    if (!probe_vault_landing(probe, facing_, jump_takeoff_feet_y_, landing_centre,
                             landing_body)) {
        return false;
    }
    begin_vault(bodies, probe, landing_centre, landing_body, at);
    ++jump_vault_count_;
    return true;
}

void Simulation::PhysicsWorld::clear_landing_recovery() noexcept {
    landing_recovery_seconds_ = landing_recovery_duration_ = landing_initial_balance_loss_ = 0;
    landing_applied_force_ = JPH::Vec3::sZero();
    landing_slip_velocity_ = JPH::Vec3::sZero();
    landing_response_ = 0;
}

void Simulation::PhysicsWorld::record_landing(const JPH::BodyInterface &bodies, const SupportSample &support,
                    const bool prepared) noexcept {
    ++landing_count_;
    landing_support_entity_id_ = support.entity_id;
    landing_normal_speed_mps_ = support.approach_normal_speed;
    landing_tangent_speed_mps_ = support.approach_tangent_speed;
    landing_approach_energy_j_ = .5 * kPlayerMassKg * landing_normal_speed_mps_ * landing_normal_speed_mps_;
    landing_tangent_energy_j_ = .5 * kPlayerMassKg * landing_tangent_speed_mps_ * landing_tangent_speed_mps_;
    const JPH::Vec3 normal(float(support.normal.x), float(support.normal.y), float(support.normal.z));
    const JPH::RVec3 player_point(support.player_contact_point.x, support.player_contact_point.y,
                                 support.player_contact_point.z);
    const JPH::Vec3 before(float(support.player_point_velocity.x), float(support.player_point_velocity.y),
                           float(support.player_point_velocity.z));
    const auto after = bodies.GetPointVelocity(player_id_, player_point);
    // This is the body's observed normal momentum change across the
    // contact interval. It includes intervening gravity/other contacts;
    // it is not claimed as an isolated contact impulse or support loss.
    landing_observed_normal_impulse_ns_ = kPlayerMassKg * (after - before).Dot(normal);
    const JPH::RVec3 point(support.contact_point.x, support.contact_point.y, support.contact_point.z);
    const auto relative = bodies.GetLinearVelocity(player_id_) - support_point_velocity(bodies, support.body_id, point);
    landing_slip_velocity_ = relative - normal * relative.Dot(normal);
    landing_recovery_duration_ = std::clamp(.08 + .045 * landing_normal_speed_mps_ +
        .06 * landing_slip_velocity_.Length(), .08, 2.0);
    landing_recovery_seconds_ = landing_recovery_duration_;
    landing_initial_balance_loss_ = std::clamp((landing_approach_energy_j_ +
        .25 * landing_tangent_energy_j_) / 7000.0, 0.0, .90);
    landing_response_ = 0;
    // Deliberately prepared compact recovery, a reduced-order posture.
    // Collision and finite reciprocal traction own forward travel. This
    // does not yet demonstrate multi-contact rolling or spread the normal
    // impact in time, and it grants no impact immunity.
    const auto tangent_velocity = relative-normal*relative.Dot(normal);
    const auto along = tangent_velocity.NormalizedOr(JPH::Vec3::sZero());
    const bool forward = !along.IsNearZero() && along.Dot(facing_) > .7F;
    // Preparation selects compact recovery at the existing brace boundary.
    // The actual impact, lethal policy and actuator laws remain unchanged.
    constexpr double kBraceMinimumImpactSpeedMps = 8.0;
    bool roll_floor = prepared && forward && landing_normal_speed_mps_>=kBraceMinimumImpactSpeedMps &&
        landing_normal_speed_mps_<=kLethalImpactSpeedMps && landing_tangent_speed_mps_>=2.5;
    const auto centre = bodies.GetPosition(player_id_);
    const float roll_runout = tangent_velocity.Length()*float(landing_recovery_duration_)+
        tangent_velocity.LengthSq()/(2*.85F*9.81F);
    const auto standing_centre=centre+JPH::Vec3(0,crouched_?kCrouchDrop:0,0);
    const auto compact_centre=standing_centre-JPH::Vec3(0,kCrouchDrop,0);
    for (float ahead=.5F;roll_floor && ahead<=roll_runout+.5F;ahead+=.5F) {
        if (!step_is_supported(standing_centre+along*ahead) ||
            !shape_pose_is_clear(player_crouch_shape_.GetPtr(),compact_centre+along*ahead+
                JPH::Vec3(0,kStandClearanceSkin,0))) {roll_floor=false;break;}
    }
    if (landing_normal_speed_mps_ >= kBraceMinimumImpactSpeedMps && landing_normal_speed_mps_ <= kLethalImpactSpeedMps &&
        landing_tangent_speed_mps_ >= 2.5 && roll_floor) landing_response_ = 2;
    else if (landing_normal_speed_mps_ >= kBraceMinimumImpactSpeedMps) landing_response_ = 1;
    else if (landing_tangent_speed_mps_ >= 4 && landing_initial_balance_loss_ > .18 && !forward)
        landing_response_ = 3;
}

void Simulation::PhysicsWorld::apply_ground_contact_force(JPH::BodyInterface &bodies, JPH::Vec3 correction,
                                const float dt, const bool oppose_slope) noexcept {
    const auto support_id = support_sample_.body_id;
    if (support_id.IsInvalid() || dt <= 0) return;
    if (soft_foot_policy_.passive && net_for_body(support_id)) {
        // Native contact friction owns neutral soft feet. Do not also
        // queue sampled servo braking, incline compensation or its reaction.
        landing_applied_force_ = JPH::Vec3::sZero();
        landing_force_support_id_ = JPH::BodyID();
        return;
    }
    const JPH::RVec3 point(support_sample_.contact_point.x, support_sample_.contact_point.y,
                          support_sample_.contact_point.z);
    const auto normal = support_traction_normal();
    const auto relative = bodies.GetLinearVelocity(player_id_) - support_point_velocity(bodies, support_id, point);
    const auto slip = relative - normal * relative.Dot(normal);
    landing_slip_velocity_ = slip;
    correction -= normal * correction.Dot(normal);
    // The capsule body coefficient stays zero (including walls). Passive
    // soft feet returned above; powered feet and rigid supports use this
    // reciprocal actuator to oppose gravity along an incline as well as
    // brake existing slip. Otherwise neutral intent has a downhill bias.
    // This is a reciprocal contact force, subject to the SAME finite
    // traction/work caps; it cannot hold an overloaded slope or missing
    // contact and does not prescribe velocity on accelerating supports.
    const auto gravity = physics_system_.GetGravity();
    if (oppose_slope)
        correction -= (gravity - normal * gravity.Dot(normal)) * kPlayerMassKg;
    const float traction_cap = (entity_is_causal_section_support(support_sample_.entity_id) ? .8F : .85F) * kPlayerMassKg * -physics_system_.GetGravity().GetY() *
                               std::max(.3F, normal.GetY());
    if (correction.Length() > traction_cap) correction *= traction_cap / correction.Length();
    bool dynamic_support = false;
    float effective_inverse_mass = 1.0F / kPlayerMassKg;
    if (auto *net = net_for_body(support_id)) {
        effective_inverse_mass += net->material_inverse_mass(support_material(*net, support_id, point));
    } else {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), support_id);
        dynamic_support = lock.Succeeded() && lock.GetBody().IsDynamic();
        if (dynamic_support && !correction.IsNearZero()) {
            const auto &body = lock.GetBody();
            const auto lever = JPH::Vec3(point - body.GetCenterOfMassPosition());
            const auto torque_axis = lever.Cross(correction.Normalized());
            effective_inverse_mass += body.GetMotionProperties()->GetInverseMass() +
                torque_axis.Dot(body.GetInverseInertia().Multiply3x3(torque_axis));
        }
    }
    // Include both bodies' translation and support rotation in the
    // finite 3 kW positive-work prediction. Braking is dissipative work.
    // Positive actuator work cannot borrow credit from braking through zero.
    // This predicts the constant-force pair with frozen contact inertia;
    // solver/contact work remains separately outside this command bound.
    const double impulse = double(correction.Length()) * dt;
    if (impulse > 0) {
        const double u = correction.Normalized().Dot(slip);
        const double inverse = effective_inverse_mass;
        const double budget = 3000.0 * dt;
        const double cap = u < 0 ? -u / inverse + std::sqrt(2.0 * budget / inverse) :
            budget / (.5 * u + .5 * std::hypot(u, std::sqrt(2.0 * inverse * budget)));
        if (cap < impulse) {
            // Leave a small rounding margin before the native float force.
            correction *= float(std::max(0.0, cap / impulse) * .999999);
        }
    }
    bodies.AddForce(player_id_, correction);
    if (auto *net = net_for_body(support_id)) net->force(support_material(*net, support_id, point), -correction, dt);
    else if (dynamic_support) bodies.AddForce(support_id, -correction, point);
    landing_applied_force_ = correction;
    landing_force_support_id_ = support_id;
    if (slingshot_ && !correction.IsNearZero()) slingshot_->note_external_influence();
}

void Simulation::PhysicsWorld::apply_landing_recovery(JPH::BodyInterface &bodies, const StepCommands &commands,
                            const float dt) noexcept {
    const auto support_id = support_sample_.body_id;
    if (support_id.IsInvalid()) return;
    const JPH::RVec3 point(support_sample_.contact_point.x, support_sample_.contact_point.y,
                           support_sample_.contact_point.z);
    const auto normal = support_traction_normal();
    const auto support_velocity = support_point_velocity(bodies, support_id, point);
    const auto relative = bodies.GetLinearVelocity(player_id_) - support_velocity;
    const auto slip = relative - normal * relative.Dot(normal);
    landing_slip_velocity_ = slip;
    const double progress = landing_recovery_duration_ > 0 ?
        std::clamp(landing_recovery_seconds_ / landing_recovery_duration_, 0.0, 1.0) : 0;
    const double balance = 1.0 - landing_initial_balance_loss_ * progress;
    const double speed = kPlayerMaximumRelativeSpeed * (crouched_ ? kCrouchSpeedScale : 1.0) * (.45 + .55 * balance);
    sprinting_ = sprint_is_eligible(commands);
    const double desired_speed = speed * (sprinting_ ? kSprintSpeedScale : 1.0);
    JPH::Vec3 desired(float(commands.move_input_x * desired_speed), 0, float(commands.move_input_z * desired_speed));
    desired -= normal * desired.Dot(normal);
    if (landing_response_ == 2 && landing_recovery_seconds_ > 0) {
        // Carry existing run momentum through compact recovery. Reverse
        // input brakes and lateral input steers through real traction.
        const auto direction=slip.NormalizedOr(JPH::Vec3::sZero());
        const float intent=float(commands.move_input_x)*direction.GetX()+float(commands.move_input_z)*direction.GetZ();
        if (intent >= 0) {
            const auto lateral=desired-direction*desired.Dot(direction);
            desired=slip*(1.0F-std::min(1.0F,dt/.65F))+lateral;
        }
    }
    auto correction = (desired - slip) * (kPlayerMassKg / .18F);
    apply_ground_contact_force(bodies, correction, dt);
    // Recovery belongs to the recorded impact. A blocked walking request
    // is not a new impact and must not renew this finite interval: doing
    // so bypasses ordinary step-up forever while pressing into a riser.
    airborne_inherited_velocity_ = support_velocity;
    balancing_ = true;
}

[[nodiscard]] bool Simulation::PhysicsWorld::sprint_is_eligible(const StepCommands &commands) const noexcept {
    const double input = std::hypot(commands.move_input_x, commands.move_input_z);
    return commands.sprint_held && !crouched_ && carry_constraint_ == nullptr &&
        input >= kSprintMinimumInput &&
        commands.move_input_x * facing_.GetX() + commands.move_input_z * facing_.GetZ() >=
            kSprintMaximumAngleCos * input;
}

[[nodiscard]] bool Simulation::PhysicsWorld::push_off_ordinary_support(JPH::BodyInterface &bodies) noexcept {
    const auto support_id = support_sample_.body_id;
    if (!grounded_ || support_id.IsInvalid()) return false;
    const auto contact = JPH::RVec3(support_sample_.contact_point.x,
        support_sample_.contact_point.y, support_sample_.contact_point.z);
    const auto support_velocity = support_point_velocity(bodies, support_id, contact);
    const auto before = bodies.GetLinearVelocity(player_id_);
    bool dynamic = false;
    double inverse_mass = 1.0 / kPlayerMassKg;
    if (auto *net = net_for_body(support_id)) {
        inverse_mass += net->material_inverse_mass(support_material(*net, support_id, contact));
    } else {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), support_id);
        if (!lock.Succeeded()) return false;
        const auto &support = lock.GetBody();
        dynamic = support.IsDynamic();
        if (dynamic) {
            const auto kit_body = kit_->body_for_entity(support.GetUserData());
            const auto lever = JPH::Vec3(contact - support.GetCenterOfMassPosition());
            const auto axis = lever.Cross(JPH::Vec3::sAxisY());
            inverse_mass += kit_body.valid() ? kit_->point_inverse_mass(kit_body, contact,
                JPH::Vec3::sAxisY()) : support.GetMotionProperties()->GetInverseMass() +
                axis.Dot(support.GetInverseInertia().Multiply3x3(axis));
        }
    }
    const double relative_y = before.GetY() - support_velocity.GetY();
    double requested = kPlayerMassKg * std::max(0.0, double(kJumpSpeed) - relative_y);
    constexpr double budget_j = 3000.0;
    // Debit only positive actuator work. Downward braking cannot fund the
    // subsequent upward push, even when the signed energy change is small.
    const double cap = relative_y >= 0 ?
        2 * budget_j / (relative_y + std::sqrt(relative_y * relative_y +
            2 * inverse_mass * budget_j)) :
        -relative_y / inverse_mass + std::sqrt(2 * budget_j / inverse_mass);
    if (requested > cap) requested = cap * 0.999999;
    const JPH::Vec3 impulse(0, float(requested), 0);
    const double delivered = impulse.GetY();
    const double positive_part = relative_y >= 0 ? delivered :
        std::max(0.0, delivered + relative_y / inverse_mass);
    landing_jump_work_j_ += relative_y >= 0 ?
        relative_y * delivered + 0.5 * inverse_mass * delivered * delivered :
        0.5 * inverse_mass * positive_part * positive_part;
    bodies.AddImpulse(player_id_, impulse);
    if (auto *net = net_for_body(support_id)) net->impulse(support_material(*net, support_id, contact), -impulse);
    else if (dynamic) bodies.AddImpulse(support_id, -impulse, contact);
    airborne_inherited_velocity_ = support_velocity;
    const auto relative = before - support_velocity;
    air_full_speed_ = std::max(kPlayerMaximumRelativeSpeed,
        JPH::Vec3(relative.GetX(), 0, relative.GetZ()).Length());
    jump_takeoff_feet_y_ = float(bodies.GetPosition(player_id_).GetY()) - kPlayerHalfHeight;
    jump_vault_ticks_left_ = kJumpVaultWindowTicks;
    clear_landing_recovery();
    return true;
}

[[nodiscard]] bool Simulation::PhysicsWorld::apply_locomotion(JPH::BodyInterface &bodies,
                                    const StepCommands &commands,
                                    const float delta_seconds) noexcept {
    const bool step_allowed = !commands.cancel_jump_requested && !commands.release_requested &&
        !commands.set_down_requested && !commands.sling_drop;
    JPH::Vec3 player_velocity = bodies.GetLinearVelocity(player_id_);
    JPH::Vec3 requested_velocity = player_velocity;
    JPH::Vec3 ground_force_request = JPH::Vec3::sZero();
    JPH::Vec3 step_probe_velocity = JPH::Vec3::sZero();
    JPH::Vec3 reference_velocity = airborne_inherited_velocity_;
    // Crouch limits walking speed on contact. Applying that walk scale in
    // flight makes forward intent brake a compact departure toward2.475m/s.
    // Air steering retains its existing departure target and finite work law.
    const double speed_scale = grounded_ && crouched_ ? kCrouchSpeedScale : 1.0;
    double move_x = commands.move_input_x * speed_scale;
    double move_z = commands.move_input_z * speed_scale;
    sprinting_ = false;
    balancing_ = false;
    if (ground_sliding_ && grounded_) {
        apply_ground_slide(bodies, commands, delta_seconds);
        return false;
    }
    if (!grounded_) {
        // Every departure uses bounded force steering. Releasing the stick
        // supplies no horizontal braking force; earned jump/support momentum
        // remains until deliberate input or an actual contact changes it.
        if (std::hypot(commands.move_input_x, commands.move_input_z) > 0.001) {
            JPH::Vec3 target = player_velocity;
            approach_relative_horizontal_velocity(target, reference_velocity,
                move_x, move_z, kAirAcceleration, delta_seconds, air_full_speed_);
            JPH::Vec3 force = (target - player_velocity) * (kPlayerMassKg / delta_seconds);
            // Steering retains the departure reference, but there is no
            // longer a contacted receiver sharing this force. Its work
            // must use actual world velocity, not discounted support speed.
            const double inverse_mass = 1.0 / kPlayerMassKg;
            const double budget = 3000.0 * delta_seconds;
            const auto magnitude = [](JPH::Vec3 value) {
                return std::hypot(double(value.GetX()), double(value.GetY()), double(value.GetZ()));
            };
            const auto world_projection = [&](JPH::Vec3 value) {
                return (double(value.GetX()) * player_velocity.GetX() +
                        double(value.GetY()) * player_velocity.GetY() +
                        double(value.GetZ()) * player_velocity.GetZ()) / magnitude(value);
            };
            const double requested_impulse = magnitude(force) * delta_seconds;
            if (requested_impulse > 0) {
                const double u = world_projection(force);
                const double cap = u < 0 ? -u / inverse_mass +
                    std::sqrt(2.0 * budget / inverse_mass) :
                    2.0 * budget / (u + std::hypot(u,
                        std::sqrt(2.0 * inverse_mass * budget)));
                if (cap < requested_impulse)
                    force *= float(std::max(0.0, cap / requested_impulse) * .999999);
                const double impulse = magnitude(force) * delta_seconds;
                const double delivered_u = impulse > 0 ? world_projection(force) : 0;
                const double braking = delivered_u < 0 ?
                    std::min(impulse, -delivered_u / inverse_mass) : 0;
                const double positive = impulse - braking;
                // Frozen isolated-force command receipts, not a ledger of
                // solver, gravity, parachute or collision energy exchange.
                air_control_command_positive_work_j_ += delivered_u < 0 ?
                    .5 * inverse_mass * positive * positive :
                    delivered_u * impulse + .5 * inverse_mass * impulse * impulse;
                air_control_command_absorbed_work_j_ +=
                    -delivered_u * braking - .5 * inverse_mass * braking * braking;
            }
            air_control_force_ = force;
            bodies.AddForce(player_id_, force);
        }
        return false;
    }
    if(grounded_) causal_airborne_=false;

    const bool causal_support=entity_is_causal_section_support(support_entity_id_);
    bool causal_step=false;
    if(grounded_ && !causal_support) {
        const JPH::Vec3 intended(float(commands.move_input_x*kPlayerMaximumRelativeSpeed),0,
                                 float(commands.move_input_z*kPlayerMaximumRelativeSpeed));
        if(intended.Length()>kStepMinimumSpeed) {
            float fraction;JPH::Vec3 normal;JPH::BodyID obstacle;
            causal_step=cast_capsule(bodies.GetPosition(player_id_),
                intended*delta_seconds+intended.Normalized()*kStepLookahead,fraction,normal,&obstacle) &&
                entity_is_causal_section_support(bodies.GetUserData(obstacle));
        }
    }
    if ((slingshot_ || causal_support || causal_step) && grounded_ &&
        (landing_recovery_seconds_ > 0 || causal_support || causal_step) && support_entity_id_ != 0) {
        if (commands.jump_requested && !crouched_) {
            if (!causal_support) return push_off_ordinary_support(bodies);
            if (deformable_plank_ &&
                deformable_plank_->logical_member_for_body(support_sample_.body_id)) {
                // Intent is consumed once. Only an actual supported wood
                // contact starts this finite leg; no free-cell impulse
                // fallback or automatic reattachment on a failed start.
                ordinary_jump_ticks_left_ = 0;
                if (!deformable_plank_->fragment_supported(support_sample_.body_id) ||
                    carry_constraint_ != nullptr || physical_foot_push_->active()) return false;
                float friction = 0;
                {
                    const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), support_sample_.body_id);
                    if (!lock.Succeeded()) return false;
                    friction = lock.GetBody().GetFriction();
                }
                const auto point = support_sample_.contact_point;
                const auto normal = support_sample_.normal;
                // A full 0.30m effort returns ~316J to this narrow board,
                // exceeding its ~171J first-bending elastic capacity.
                // Request a shorter physical stroke, retaining material
                // strength and all actuator ceilings; no takeoff speed is granted.
                if (!physical_foot_push_->begin(support_sample_.body_id,
                    JPH::RVec3(point.x, point.y, point.z),
                    JPH::Vec3(float(normal.x), float(normal.y), float(normal.z)), friction, .15F)) return false;
                foot_push_start_broken_mask_ = deformable_plank_->capture().broken_mask;
                foot_push_purpose_ = FootPushPurpose::PlankJump;
                ++foot_push_start_count_;
                foot_push_started_this_tick_ = true;
                airborne_inherited_velocity_ = current_support_point_velocity(bodies);
                const auto departure = bodies.GetLinearVelocity(player_id_) - airborne_inherited_velocity_;
                air_full_speed_ = std::max(kPlayerMaximumRelativeSpeed,
                    JPH::Vec3(departure.GetX(), 0, departure.GetZ()).Length());
                causal_airborne_ = true;
                jump_takeoff_feet_y_ = float(bodies.GetPosition(player_id_).GetY()) - kPlayerHalfHeight;
                jump_vault_ticks_left_ = kJumpVaultWindowTicks;
                clear_landing_recovery();
                return true;
            }
            const auto support_velocity = current_support_point_velocity(bodies);
            const auto before_velocity = bodies.GetLinearVelocity(player_id_);
            auto impulse = JPH::Vec3(0, kPlayerMassKg * std::max(0.0F,
                support_velocity.GetY() + kJumpSpeed - before_velocity.GetY()), 0);
            const auto support_id = support_sample_.body_id;
            bool dynamic_support = false;
            float inverse_mass = 1.0F / kPlayerMassKg;
            const JPH::RVec3 contact(support_sample_.contact_point.x,
                support_sample_.contact_point.y, support_sample_.contact_point.z);
            if (auto *net = net_for_body(support_id)) {
                inverse_mass += net->material_inverse_mass(support_material(*net, support_id, contact));
            } else {
                const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), support_id);
                dynamic_support = lock.Succeeded() && lock.GetBody().IsDynamic();
                if (dynamic_support) {
                    const auto &support = lock.GetBody();
                    const auto lever = JPH::Vec3(JPH::RVec3(support_sample_.contact_point.x,
                        support_sample_.contact_point.y, support_sample_.contact_point.z) - support.GetCenterOfMassPosition());
                    const auto rotation_axis = lever.Cross(JPH::Vec3::sAxisY());
                    const auto kit_body=kit_->body_for_entity(support.GetUserData());
                    inverse_mass += kit_body.valid() ? kit_->point_inverse_mass(kit_body,
                        JPH::RVec3(support_sample_.contact_point.x,support_sample_.contact_point.y,
                                  support_sample_.contact_point.z),JPH::Vec3::sAxisY()) :
                        support.GetMotionProperties()->GetInverseMass() +
                        rotation_axis.Dot(support.GetInverseInertia().Multiply3x3(rotation_axis));
                }
            }
            const double relative_y = before_velocity.GetY() - support_velocity.GetY();
            if(causal_support) {
                // Finite positive push-off work includes receiver recoil
                // and rotation. Braking downward motion cannot fund the
                // later upward push; retain this branch's existing budget.
                constexpr double budget=.5*double(kPlayerMassKg)*kJumpSpeed*kJumpSpeed;
                const double cap = relative_y >= 0 ?
                    2 * budget / (relative_y + std::sqrt(relative_y * relative_y +
                        2 * inverse_mass * budget)) :
                    -relative_y / inverse_mass + std::sqrt(2 * budget / inverse_mass);
                if (impulse.GetY() > cap) impulse.SetY(float(cap * .999999));
                causal_airborne_=true;
            }
            airborne_inherited_velocity_=support_velocity;
            const auto departure=before_velocity-support_velocity;
            air_full_speed_=std::max(kPlayerMaximumRelativeSpeed,
                JPH::Vec3(departure.GetX(),0,departure.GetZ()).Length());
            bodies.AddImpulse(player_id_, impulse);
            const double delivered = impulse.GetY();
            const double positive_part = relative_y >= 0 ? delivered :
                std::max(0.0, delivered + relative_y / inverse_mass);
            landing_jump_work_j_ += relative_y >= 0 ?
                relative_y * delivered + .5 * inverse_mass * delivered * delivered :
                .5 * inverse_mass * positive_part * positive_part;
            if (auto *net = net_for_body(support_id)) net->impulse(support_material(*net, support_id, contact), -impulse);
            else if (dynamic_support) bodies.AddImpulse(support_id, -impulse, contact);
            jump_takeoff_feet_y_ = float(bodies.GetPosition(player_id_).GetY()) - kPlayerHalfHeight;
            jump_vault_ticks_left_ = kJumpVaultWindowTicks;
            clear_landing_recovery();
            return true;
        }
        // The same reciprocal contact-force owner remains active for ordinary
        // cargo walking after the impact-recovery interval has expired.
        apply_landing_recovery(bodies, commands, delta_seconds);
        if(causal_support || causal_step) causal_airborne_=true;
        if((causal_support || causal_step) && step_allowed) {
            const JPH::Vec3 intended(float(commands.move_input_x*kPlayerMaximumRelativeSpeed),0,
                                     float(commands.move_input_z*kPlayerMaximumRelativeSpeed));
            if(try_step_up(bodies,intended,delta_seconds)) return true;
        }
        const auto relative=bodies.GetLinearVelocity(player_id_)-current_support_point_velocity(bodies);
        air_full_speed_=std::max(kPlayerMaximumRelativeSpeed,JPH::Vec3(relative.GetX(),0,relative.GetZ()).Length());
        return false;
    }

    if (grounded_ && support_entity_id_ != 0) {
        reference_velocity = current_support_point_velocity(bodies);
        airborne_inherited_velocity_ = reference_velocity;
        const Beam beam = beam_underfoot(bodies);
        if (beam.valid) {
            // On a beam: walk along it and deliberately make fine lateral
            // corrections. Neutral input brakes; it never seeks the centre.
            balancing_ = true;
            const double along = move_x * beam.along.GetX() + move_z * beam.along.GetZ();
            const double across = move_x * beam.across.GetX() + move_z * beam.across.GetZ();
            if (std::abs(across) < kBalanceStepOffInput) {
                const double walk = std::clamp(along, -kBalanceSpeedScale, kBalanceSpeedScale);
                const double correction =
                    (across / kBalanceStepOffInput) * kBalanceCorrectionMaxMps /
                    kPlayerMaximumRelativeSpeed;
                move_x = beam.along.GetX() * walk + beam.across.GetX() * correction;
                move_z = beam.along.GetZ() * walk + beam.across.GetZ() * correction;
            }
        } else if (sprint_is_eligible(commands)) {
            sprinting_ = true;
        }
        const float full_speed = sprinting_
                                     ? static_cast<float>(kPlayerMaximumRelativeSpeed * kSprintSpeedScale)
                                     : kPlayerMaximumRelativeSpeed;
        approach_relative_horizontal_velocity(requested_velocity,
                                              reference_velocity,
                                              move_x,
                                              move_z,
                                              kGroundAcceleration,
                                              delta_seconds,
                                              full_speed);
        ground_force_request = (requested_velocity - player_velocity) *
            (kPlayerMassKg / delta_seconds);
        // Probe the shaped walking intent even when contact blocks actual
        // acceleration. This changes geometric recognition, never velocity.
        step_probe_velocity = requested_velocity - reference_velocity;
        if (std::hypot(commands.move_input_x, commands.move_input_z) > 0.001)
            step_probe_velocity = JPH::Vec3(float(move_x * full_speed), 0,
                                           float(move_z * full_speed));
        // The air keeps what the ground gave: a running jump, or a run
        // off an edge, carries its speed.
        const JPH::Vec3 relative = player_velocity - reference_velocity;
        air_full_speed_ = std::max(kPlayerMaximumRelativeSpeed,
                                   JPH::Vec3(relative.GetX(), 0.0F, relative.GetZ()).Length());
    }

    // Crouched here means there was no room to stand, so no room to jump.
    const bool jump_started = commands.jump_requested && grounded_ && !crouched_;
    if (jump_started && !push_off_ordinary_support(bodies)) return false;
    bool step_started = false;
    if (!jump_started && step_allowed && grounded_ && support_entity_id_ != 0) {
        step_started = try_step_up(bodies, step_probe_velocity, delta_seconds,
            std::hypot(commands.move_input_x, commands.move_input_z) > 0.001);
    }
    // Budget against actual velocity after any jump/physical-step impulse:
    // a tangent force on an incline can work on that upward motion too.
    if (grounded_ && support_entity_id_ != 0)
        apply_ground_contact_force(bodies, ground_force_request, delta_seconds);
    return jump_started || step_started;
}

[[nodiscard]] bool Simulation::PhysicsWorld::cast_capsule(const JPH::RVec3 from, const JPH::Vec3 displacement,
                                float &fraction, JPH::Vec3 &normal, JPH::BodyID *hit_body,
                                JPH::SubShapeID *hit_shape, JPH::RVec3 *hit_point) const {
    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    const JPH::RShapeCast sweep(active_player_shape(), JPH::Vec3::sReplicate(1.0F),
                                JPH::RMat44::sTranslation(from), displacement);
    const JPH::IgnoreSingleBodyFilter body_filter(player_id_);
    physics_system_.GetNarrowPhaseQuery().CastShape(
        sweep, JPH::ShapeCastSettings(), from, collector, {}, {}, body_filter);
    if (!collector.HadHit()) {
        return false;
    }
    fraction = collector.mHit.mFraction;
    if(hit_body) *hit_body=collector.mHit.mBodyID2;
    if(hit_shape) *hit_shape=collector.mHit.mSubShapeID2;
    if(hit_point) *hit_point=from+collector.mHit.mContactPointOn2;
    normal = -collector.mHit.mPenetrationAxis.NormalizedOr(JPH::Vec3::sZero());
    return true;
}

// Vehicle boarding keeps its existing finite reciprocal push-off. Its
// short approach and rising hand catch are separate from ordinary stair gait.
bool Simulation::PhysicsWorld::try_vehicle_boarding_step(JPH::BodyInterface &bodies, const JPH::Vec3 relative_velocity,
                 const float delta_seconds, const bool deliberate_step) {
    // The still-contacting finite leg already owns this push-off. A
    // separate step impulse must not stack its own vertical work on it.
    const bool passive_plant = physical_foot_push_->active() &&
        foot_push_purpose_ == FootPushPurpose::TrailingPlant;
    if ((physical_foot_push_->active() && !passive_plant) ||
        net_for_body(support_sample_.body_id)) return false;
    const JPH::Vec3 horizontal(relative_velocity.GetX(), 0.0F, relative_velocity.GetZ());
    const float speed = horizontal.Length();
    if (speed < kStepMinimumSpeed) {
        return false;
    }
    const JPH::Vec3 ahead = horizontal / speed * (speed * delta_seconds + kStepLookahead);
    const JPH::RVec3 at = bodies.GetPosition(player_id_);
    float fraction = 1.0F;
    JPH::Vec3 normal = JPH::Vec3::sZero();
    JPH::BodyID obstacle;
    JPH::SubShapeID obstacle_shape;
    if (!cast_capsule(at, ahead, fraction, normal, &obstacle, &obstacle_shape))
        return false;
    bool teeter_member_crossing = false;
    const auto obstacle_entity = bodies.GetUserData(obstacle);
    if (deliberate_step && grounded_ && !support_sample_.body_id.IsInvalid() &&
        (obstacle_entity == 2800 || obstacle_entity == 1800)) {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), obstacle);
        LeafBox member;
        // Identify the actual thin rail or fixed entry-tongue leaf, not
        // either whole compound. Only transverse intent qualifies: a
        // rounded low lip can block the capsule despite its slope normal.
        if (lock.Succeeded() && leaf_box(lock.GetBody(), obstacle_shape, member) &&
            2 * member.half.GetY() <= kStepMaximumHeight) {
            const bool beam_rail = obstacle_entity == 2800 &&
                2 * member.half.GetX() >= kBalanceMinLength &&
                2 * member.half.GetZ() <= kGripMaxSection &&
                std::abs(horizontal.Dot(member.axes[2])) > 0.25F * speed;
            const bool entry_tongue = obstacle_entity == 1800 &&
                2 * member.half.GetZ() >= kBalanceMinLength &&
                2 * member.half.GetX() <= kGripMaxSection &&
                std::abs(horizontal.Dot(member.axes[0])) > 0.25F * speed;
            teeter_member_crossing = (beam_rail || entry_tongue) &&
                horizontal.Dot(normal) < 0;
        }
    }
    if (normal.GetY() >= kSupportNormalThreshold && !teeter_member_crossing)
        return false;
    if (teeter_member_crossing && physical_step_pending_) return false;
    const bool physical=entity_is_causal_section_support(support_entity_id_) ||
        entity_is_causal_section_support(bodies.GetUserData(obstacle)) || teeter_member_crossing ||
        // Boarding a real vehicle step uses the same finite reciprocal leg
        // push as other machinery, never the legacy static-riser pose write.
        support_entity_id_ == WarehouseForkliftYard::kChassisEntity ||
        obstacle_entity == WarehouseForkliftYard::kChassisEntity ||
        (support_entity_id_ >= 1450 && support_entity_id_ <= 1452) ||
        (obstacle_entity >= 1450 && obstacle_entity <= 1452) ||
        support_entity_id_ == SwingStair::kStairEntity ||
        bodies.GetUserData(obstacle) == SwingStair::kStairEntity;
    const JPH::RVec3 raised = at + JPH::Vec3(0.0F, kStepMaximumHeight, 0.0F);
    if (!capsule_pose_is_clear(raised) || cast_capsule(raised, ahead, fraction, normal)) {
        return false;  // No headroom, or the obstacle is taller than a step.
    }
    const JPH::RVec3 over = raised + ahead;
    const JPH::Vec3 down(0.0F, -(kStepMaximumHeight + kStepMinimumHeight), 0.0F);
    if (!cast_capsule(over, down, fraction, normal) ||
        normal.GetY() < kSupportNormalThreshold) {
        return false;  // Nothing walkable to put the feet on.
    }
    const float rise = kStepMaximumHeight + down.GetY() * fraction;
    if (rise < kStepMinimumHeight) {
        return false;
    }
    if (passive_plant) {
        // A remembered opportunity never starts a step. Only a validated
        // physical riser and fresh real support can hand the passive leg to
        // the existing finite step push. Remove its row before that impulse,
        // preserving the mutex for every already-powered foot action.
        if (!physical || !grounded_ || !foot_contact_.valid ||
            foot_contact_.solved_tick != contact_solve_serial_ ||
            foot_contact_.body != support_sample_.body_id) return false;
        physical_foot_push_->clear();
        foot_push_purpose_ = FootPushPurpose::None;
    }
    if(physical) {
        // A swept low riser requests leg work, never a prescribed pose.
        // The actual current foot contact takes the reciprocal impulse;
        // native gravity/contact decide whether the step is achieved.
        if(support_sample_.body_id.IsInvalid()) return false;
        const auto contact = passive_plant ?
            from_support_local(bodies, foot_contact_.body, foot_contact_.local_point) :
            JPH::RVec3(support_sample_.contact_point.x,support_sample_.contact_point.y,
                      support_sample_.contact_point.z);
        const auto support=support_sample_.body_id;
        const auto support_velocity = support_point_velocity(bodies, support, contact);
        const auto relative=bodies.GetLinearVelocity(player_id_)-support_velocity;
        const float takeoff=std::sqrt(2.0F*-physics_system_.GetGravity().GetY()*(rise+0.08F));
        auto impulse=JPH::Vec3(0,kPlayerMassKg*std::max(0.0F,takeoff-relative.GetY()),0);
        float inverse=1.0F/kPlayerMassKg;
        const bool dynamic=bodies.GetMotionType(support)==JPH::EMotionType::Dynamic;
        if(dynamic) {
            const auto body=kit_->body_for_entity(bodies.GetUserData(support));
            if(body.valid()) inverse+=kit_->point_inverse_mass(body,contact,JPH::Vec3::sAxisY());
        }
        const double budget=kPlayerMassKg*-physics_system_.GetGravity().GetY()*(rise+0.08F);
        const double linear=impulse.Dot(relative),quadratic=.5*double(impulse.LengthSq())*inverse;
        if(linear+quadratic>budget)
            impulse*=float(2*budget/(linear+std::sqrt(linear*linear+4*quadratic*budget)));
        bodies.AddImpulse(player_id_,impulse);
        if(dynamic) bodies.AddImpulse(support,-impulse,contact);
        landing_jump_work_j_+=std::max(0.0F,impulse.Dot(relative)+.5F*impulse.LengthSq()*inverse);
        causal_airborne_=true;
        airborne_inherited_velocity_ = support_velocity;
        physical_step_pending_=true;
        physical_step_start_feet_y_=at.GetY()-kPlayerHalfHeight;
        return true;
    }
    return false;
}

bool Simulation::PhysicsWorld::try_step_up(JPH::BodyInterface &bodies, const JPH::Vec3 relative_velocity,
                 const float delta_seconds, const bool deliberate_step) {
    if (support_entity_id_ == WarehouseForkliftYard::kChassisEntity ||
        (support_entity_id_ >= 1450 && support_entity_id_ <= 1452))
        return try_vehicle_boarding_step(bodies,relative_velocity,delta_seconds,deliberate_step);
    // One finite leg owns the foot at a time. Only an unpowered trailing
    // plant may hand its fresh contact to a new walking stroke.
    const bool passive_plant = physical_foot_push_->active() &&
        foot_push_purpose_ == FootPushPurpose::TrailingPlant;
    if (crouched_ || (physical_foot_push_->active() && !passive_plant) ||
        net_for_body(support_sample_.body_id)) return false;
    const JPH::Vec3 horizontal(relative_velocity.GetX(), 0.0F, relative_velocity.GetZ());
    const float speed = horizontal.Length();
    if (speed < kStepMinimumSpeed) {
        return false;
    }
    const float leg_acceleration = PhysicalFootPush::force_bound_n()/kPlayerMassKg+
        physics_system_.GetGravity().GetY();
    const float lift_lead_seconds = leg_acceleration > 0 ?
        std::sqrt(2*kStepMaximumHeight/leg_acceleration) : delta_seconds;
    const JPH::Vec3 ahead = horizontal/speed*
        (speed*std::max(delta_seconds,lift_lead_seconds)+kStepLookahead);
    const JPH::RVec3 at = bodies.GetPosition(player_id_);
    float fraction = 1.0F;
    JPH::Vec3 normal = JPH::Vec3::sZero();
    JPH::BodyID obstacle;
    JPH::SubShapeID obstacle_shape;
    // Reject resting/support hits before closest-hit pruning. A zero-time
    // floor overlap must not hide a later riser of the same compound body.
    class StepObstacleCollector final : public JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> {
    public:
        StepObstacleCollector(JPH::Vec3 movement, JPH::BodyID support, JPH::SubShapeID support_shape)
            : movement_(movement), support_(support), support_shape_(support_shape) {}
        void AddHit(const JPH::ShapeCastResult &hit) override {
            const auto n = -hit.mPenetrationAxis.NormalizedOr(JPH::Vec3::sZero());
            if (n.Dot(movement_) >= 0) return;
            const double height = hit.mContactPointOn2.GetY()+kPlayerHalfHeight;
            if (n.GetY() >= kSupportNormalThreshold &&
                ((hit.mBodyID2 == support_ && hit.mSubShapeID2 == support_shape_) ||
                 height < kStepMinimumHeight)) return;
            JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector>::AddHit(hit);
        }
    private:
        JPH::Vec3 movement_;
        JPH::BodyID support_;
        JPH::SubShapeID support_shape_;
    } obstacle_hits(ahead,foot_contact_.body,foot_contact_.subshape);
    const JPH::RShapeCast step_sweep(active_player_shape(), JPH::Vec3::sReplicate(1.0F),
        JPH::RMat44::sTranslation(at),ahead);
    const JPH::IgnoreSingleBodyFilter player_filter(player_id_);
    physics_system_.GetNarrowPhaseQuery().CastShape(step_sweep,JPH::ShapeCastSettings(),at,
        obstacle_hits,{},{},player_filter);
    if (!obstacle_hits.HadHit()) return false;
    const auto &obstacle_hit = obstacle_hits.mHit;
    fraction = obstacle_hit.mFraction;
    normal = -obstacle_hit.mPenetrationAxis.NormalizedOr(JPH::Vec3::sZero());
    obstacle = obstacle_hit.mBodyID2;
    obstacle_shape = obstacle_hit.mSubShapeID2;
    // Inspect only the first riser plus its lip. A longer anticipatory
    // query must not reject that step because it also reaches later treads.
    const JPH::Vec3 crossing = horizontal/speed*
        std::min(ahead.Length(),std::max(0.0F,fraction)*ahead.Length()+kStepLookahead);
    bool teeter_member_crossing = false;
    const auto obstacle_entity = bodies.GetUserData(obstacle);
    if (deliberate_step && grounded_ && !support_sample_.body_id.IsInvalid() &&
        (obstacle_entity == 2800 || obstacle_entity == 1800)) {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), obstacle);
        LeafBox member;
        // Identify the actual thin rail or fixed entry-tongue leaf, not
        // either whole compound. Only transverse intent qualifies: a
        // rounded low lip can block the capsule despite its slope normal.
        if (lock.Succeeded() && leaf_box(lock.GetBody(), obstacle_shape, member) &&
            2 * member.half.GetY() <= kStepMaximumHeight) {
            const bool beam_rail = obstacle_entity == 2800 &&
                2 * member.half.GetX() >= kBalanceMinLength &&
                2 * member.half.GetZ() <= kGripMaxSection &&
                std::abs(horizontal.Dot(member.axes[2])) > 0.25F * speed;
            const bool entry_tongue = obstacle_entity == 1800 &&
                2 * member.half.GetZ() >= kBalanceMinLength &&
                2 * member.half.GetX() <= kGripMaxSection &&
                std::abs(horizontal.Dot(member.axes[0])) > 0.25F * speed;
            teeter_member_crossing = (beam_rail || entry_tongue) &&
                horizontal.Dot(normal) < 0;
        }
    }
    if (teeter_member_crossing && physical_step_pending_) return false;
    const JPH::RVec3 raised = at + JPH::Vec3(0.0F, kStepMaximumHeight, 0.0F);
    if (!capsule_pose_is_clear(raised) || cast_capsule(raised, crossing, fraction, normal)) {
        return false;  // No headroom, or the obstacle is taller than a step.
    }
    const JPH::RVec3 over = raised + crossing;
    const JPH::Vec3 down(0.0F, -(kStepMaximumHeight + kStepMinimumHeight), 0.0F);
    JPH::BodyID receiver;
    JPH::SubShapeID receiver_shape;
    JPH::RVec3 receiver_point;
    if (!cast_capsule(over, down, fraction, normal, &receiver, &receiver_shape, &receiver_point) ||
        normal.GetY() < kSupportNormalThreshold) {
        return false;  // Nothing walkable to put the feet on.
    }
    const float rise = float(receiver_point.GetY() - (at.GetY()-kPlayerHalfHeight));
    if (rise < kStepMinimumHeight || rise > kStepMaximumHeight+kLandingSkin) {
        return false;
    }
    // The prospective upper face is only an effort/clearance request. The
    // current completed solve supplies the actual foot and reciprocal load.
    if (!grounded_ || !foot_contact_.valid ||
        foot_contact_.solved_tick != contact_solve_serial_ ||
        foot_contact_.body != support_sample_.body_id) return false;
    if (passive_plant) {
        physical_foot_push_->clear();
        foot_push_purpose_ = FootPushPurpose::None;
    }
    const auto contact = from_support_local(bodies, foot_contact_.body, foot_contact_.local_point);
    auto plant_normal = bodies.GetRotation(foot_contact_.body)*foot_contact_.local_normal;
    {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(),foot_contact_.body);
        if (!lock.Succeeded()) return false;
        // The actual sharp lip also belongs to its top face. Choosing that
        // admissible surface normal never moves or manufactures the foot.
        (void)walking_top_face_normal(lock.GetBody(),foot_contact_.subshape,contact,plant_normal);
    }
    if (!physical_foot_push_->begin_walking_step(foot_contact_.body, contact,
            plant_normal, foot_contact_.friction, rise+kLandingSkin, horizontal/speed,
            std::max(kStepLookahead,JPH::Vec3(receiver_point-at).Dot(horizontal/speed)))) return false;
    foot_push_contact_ = foot_contact_;
    foot_push_purpose_ = FootPushPurpose::WalkingStep;
    foot_push_started_this_tick_ = true;
    ++foot_push_start_count_;
    step_receiver_ = receiver;
    step_receiver_subshape_ = receiver_shape;
    step_receiver_local_point_ = to_support_local(bodies,receiver,receiver_point);
    step_direction_ = horizontal/speed;
    refresh_rigid_transfer_foot();
    airborne_inherited_velocity_ = support_point_velocity(bodies,foot_contact_.body,contact);
    physical_step_pending_ = true;
    physical_step_start_feet_y_ = at.GetY()-kPlayerHalfHeight;
    return true;
}

void Simulation::PhysicsWorld::apply_parachute_drag(JPH::BodyInterface &bodies, const float delta_seconds) noexcept {
    if (!parachute_deployed_) {
        return;
    }
    JPH::Vec3 velocity = bodies.GetLinearVelocity(player_id_);
    const float speed = velocity.Length();
    if (speed > 1.0e-4F) {
        if (slingshot_) slingshot_->note_external_influence();
        const JPH::Vec3 drag_acceleration =
            -(velocity / speed) * (kParachuteDragCoefficient * speed * speed);
        velocity += drag_acceleration * delta_seconds;
        bodies.SetLinearVelocity(player_id_, velocity);
    }
}

} // namespace scraperx::sim
