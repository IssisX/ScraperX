#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

[[nodiscard]] CargoNet *Simulation::PhysicsWorld::net_for_body(JPH::BodyID id) const noexcept {
    if (cargo_net_ && id == cargo_net_->body()) return cargo_net_.get();
    if (catch_net_ && id == catch_net_->body()) return catch_net_.get();
    return nullptr;
}

[[nodiscard]] JPH::Vec3 Simulation::PhysicsWorld::support_point_velocity(const JPH::BodyInterface &bodies,
    JPH::BodyID id, JPH::RVec3 point) const noexcept {
    if (const auto *net = net_for_body(id)) return net->material_velocity(support_material(*net, id, point));
    return bodies.GetPointVelocity(id, point);
}

[[nodiscard]] JPH::Vec3 Simulation::PhysicsWorld::support_material(const CargoNet &net, JPH::BodyID id,
    JPH::RVec3 point) const noexcept {
    const JPH::RVec3 contact(support_sample_.contact_point.x, support_sample_.contact_point.y,
        support_sample_.contact_point.z);
    if (support_sample_.grounded && id == support_sample_.body_id &&
        JPH::Vec3(point - contact).LengthSq() < .0001F) return support_sample_.soft_material;
    return net.material_point(point);
}

[[nodiscard]] JPH::Vec3 Simulation::PhysicsWorld::support_traction_normal() const noexcept {
    const auto cap = JPH::Vec3(float(support_sample_.normal.x), float(support_sample_.normal.y),
        float(support_sample_.normal.z));
    const auto *net = net_for_body(support_sample_.body_id);
    if (!net || !support_sample_.grounded) return cap;
    const auto material = support_sample_.soft_material;
    constexpr float epsilon = .01F; // 5 mm at the ordinary .5 m mesh pitch
    const float u0 = std::max(0.0F, material.GetX() - epsilon);
    const float u1 = std::min(float(net->columns() - 1), material.GetX() + epsilon);
    const float v0 = std::max(0.0F, material.GetY() - epsilon);
    const float v1 = std::min(float(net->rows() - 1), material.GetY() + epsilon);
    const auto du = JPH::Vec3(net->world_point(JPH::Vec3(u1, material.GetY(), material.GetZ())) -
        net->world_point(JPH::Vec3(u0, material.GetY(), material.GetZ())));
    const auto dv = JPH::Vec3(net->world_point(JPH::Vec3(material.GetX(), v1, material.GetZ())) -
        net->world_point(JPH::Vec3(material.GetX(), v0, material.GetZ())));
    auto normal = du.Cross(dv);
    if (!std::isfinite(normal.LengthSq()) || normal.LengthSq() <= 1.0e-12F) return cap;
    normal = normal.Normalized();
    if (normal.Dot(cap) < 0) normal = -normal;
    // Only traction uses this frame on the existing material receiver.
    // Actual Jolt contact, cap normal, gap and departure still own stance.
    // A vertical cloth keeps its existing contact basis and hand behavior.
    return normal.GetY() > .25F ? normal : cap;
}

void Simulation::PhysicsWorld::clear_catch_history() noexcept {
    soft_contact_listener_.begin_tick();
    contact_listener_.begin_tick();
    catch_net_state_ = {};
    state_.soft_side_contact_entity = 0;
    catch_contact_previous_ = false;
    catch_episode_active_ = false;
    if (catch_net_) {
        catch_net_state_.installed = true;
        catch_net_state_.columns = catch_net_->columns();
        catch_net_state_.rows = catch_net_->rows();
        refresh_catch_geometry();
    }
}

void Simulation::PhysicsWorld::refresh_catch_geometry() noexcept {
    if (!catch_net_ || catch_net_->vertices().empty()) return;
    double minimum = std::numeric_limits<double>::max();
    for (const auto &v : catch_net_->vertices()) minimum = std::min(minimum, double(v.GetY()));
    catch_net_state_.minimum_y_m = minimum;
    catch_net_state_.sag_m = std::max(0.0, double(catch_net_->config().origin.GetY()) - minimum);
}

[[nodiscard]] bool Simulation::PhysicsWorld::contact_on_bottom_capsule_cap(const JPH::Vec3 delta,
                                                       const float half) noexcept {
    return delta.GetY() <= -(half - kPlayerRadius) && delta.GetY() >= -half - .20F &&
        delta.GetX()*delta.GetX()+delta.GetZ()*delta.GetZ() <=
            (kPlayerRadius+.15F)*(kPlayerRadius+.15F);
}

[[nodiscard]] bool Simulation::PhysicsWorld::passive_soft_feet_requested(const StepCommands &commands) const noexcept {
    return std::hypot(commands.move_input_x, commands.move_input_z) <= .001 &&
        !commands.jump_requested && !commands.traversal_requested &&
        ordinary_jump_ticks_left_ == 0 && traversal_state_ == TraversalState::None &&
        !physical_hands_->active() && !physical_foot_push_->active() &&
        !foot_push_started_this_tick_ && !cargo_transfer_active_ && cargo_push_seconds_ < 0 &&
        carry_constraint_ == nullptr && !(slingshot_ && slingshot_->controls_player());
}

void Simulation::PhysicsWorld::publish_soft_foot_policy(const StepCommands &commands) noexcept {
    soft_foot_policy_.half_height = crouched_ ? kPlayerCrouchHalfHeight : kPlayerHalfHeight;
    soft_foot_policy_.passive = passive_soft_feet_requested(commands);
}

void Simulation::PhysicsWorld::reset_soft_foot_policy() noexcept {
    soft_foot_policy_.half_height = crouched_ ? kPlayerCrouchHalfHeight : kPlayerHalfHeight;
    soft_foot_policy_.passive = false;
}

[[nodiscard]] float Simulation::PhysicsWorld::soft_foot_contact_friction(void *context, const JPH::BodyID rigid,
    const JPH::Vec3 rigid_local_contact, const JPH::Vec3 world_normal_toward_rigid,
    const float original_friction) noexcept {
    const auto &policy = *static_cast<const SoftFootPolicy *>(context);
    // The capsule has locked rotation and a centred COM. Cached native
    // contact coordinates therefore use the exact observed sole geometry.
    // This grants friction only; support/Jump/checkpoint authority still
    // requires the ordinary post-solve contact observation.
    if (rigid == policy.player && policy.passive &&
        world_normal_toward_rigid.GetY() >= kSupportNormalThreshold &&
        contact_on_bottom_capsule_cap(rigid_local_contact, policy.half_height)) return .8F;
    return original_friction;
}

[[nodiscard]] SupportSample Simulation::PhysicsWorld::observe_soft_support(const JPH::BodyInterface &bodies) const {
    SupportSample best;
    const auto centre = bodies.GetPosition(player_id_);
    const float half = crouched_ ? kPlayerCrouchHalfHeight : kPlayerHalfHeight;
    const auto velocity = bodies.GetLinearVelocity(player_id_);
    double best_distance = std::numeric_limits<double>::max();
    for (const auto &contact : soft_contact_listener_.contacts()) {
        auto *net = net_for_body(contact.soft);
        if (!net || contact.normal.GetY() < kSupportNormalThreshold) continue;
        const auto delta = JPH::Vec3(contact.point - centre);
        // Contact is observed, current and on the bottom spherical cap.
        // A side/underside plane cannot acquire standing or Jump authority.
        if (!contact_on_bottom_capsule_cap(delta, half)) continue;
        const auto material = net->material_point(contact.point);
        const auto support_velocity = net->material_velocity(material);
        const auto relative = velocity - support_velocity;
        if (relative.Dot(contact.normal) > .75F) continue; // actual departure
        const double distance = delta.LengthSq();
        if (distance >= best_distance) continue;
        best_distance = distance;
        best.grounded = true; best.entity_id = contact.entity; best.body_id = contact.soft;
        best.contact_point = {contact.point.GetX(),contact.point.GetY(),contact.point.GetZ()};
        best.player_contact_point = best.contact_point;
        best.soft_material = material;
        best.point_velocity = {support_velocity.GetX(),support_velocity.GetY(),support_velocity.GetZ()};
        best.normal_y = contact.normal.GetY();
        best.normal = {contact.normal.GetX(),contact.normal.GetY(),contact.normal.GetZ()};
        best.player_point_velocity = {velocity.GetX(),velocity.GetY(),velocity.GetZ()};
    }
    return best;
}

void Simulation::PhysicsWorld::measure_catch_load(JPH::Vec3 velocity_before, const float dt,
    const JPH::BodyInterface &bodies, const SupportSample &soft_support) noexcept {
    state_.soft_side_contact_entity = 0;
    const auto centre = bodies.GetPosition(player_id_);
    const float half = crouched_ ? kPlayerCrouchHalfHeight : kPlayerHalfHeight;
    for (const auto &contact : soft_contact_listener_.contacts()) {
        if (net_for_body(contact.soft) && std::abs(contact.normal.GetY()) < .5F &&
            std::abs(contact.point.GetY() - centre.GetY()) <= half - kPlayerRadius &&
            (state_.soft_side_contact_entity == 0 || contact.entity < state_.soft_side_contact_entity))
            state_.soft_side_contact_entity = contact.entity;
    }
    if (!catch_net_) return;
    catch_net_state_.installed = true;
    catch_net_state_.columns = catch_net_->columns(); catch_net_state_.rows = catch_net_->rows();
    refresh_catch_geometry();
    const bool current = soft_support.grounded && soft_support.entity_id == kCatchNetEntity;
    catch_net_state_.current_contact = current;
    catch_net_state_.side_contact = state_.soft_side_contact_entity == kCatchNetEntity;
    if (current && !catch_contact_previous_) {
        ++catch_net_state_.contact_serial;
        // Cached tick incoming vertical speed: an approach estimate, not
        // the unavailable per-vertex pre-solve relative velocity.
        catch_net_state_.approach_speed_mps = std::max(0.0F, -velocity_before.GetY());
    }
    catch_contact_previous_ = current;
    const bool touched = soft_contact_listener_.catch_touched();
    const auto specific = (bodies.GetLinearVelocity(player_id_) - velocity_before) / dt - physics_system_.GetGravity();
    catch_net_state_.total_specific_load_mps2 = touched ? specific.Length() : 0;
    if (touched) {
        if (!catch_episode_active_) {
            catch_net_state_.peak_total_specific_load_mps2 = 0;
            catch_net_state_.excess_load_dose_g_seconds = 0;
            catch_net_state_.overloaded = false;
            catch_episode_active_ = true;
        }
        const double load_g = specific.Length() / 9.81;
        catch_net_state_.peak_total_specific_load_mps2 = std::max(
            catch_net_state_.peak_total_specific_load_mps2, double(specific.Length()));
        // CHOSEN reduced-order gameplay injury policy, not a clinical
        // criterion or isolated net force. Integrate excess TOTAL load
        // above 8g, units g*s, through one catch/rebound episode. Finite
        // 35g peak / 6g*s dose calibrated against native player traces.
        catch_net_state_.excess_load_dose_g_seconds += std::max(0.0, load_g - 8.0) * dt;
        catch_net_state_.overloaded = catch_net_state_.overloaded || load_g > 35.0 ||
            catch_net_state_.excess_load_dose_g_seconds > 6.0;
    }
}

[[nodiscard]] JPH::Vec3 Simulation::PhysicsWorld::current_support_point_velocity(
    const JPH::BodyInterface &bodies) const noexcept {
    const JPH::BodyID support_id = body_id_for_entity(support_entity_id_);
    if (support_id.IsInvalid()) {
        return JPH::Vec3::sZero();
    }
    return support_point_velocity(bodies,
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

    // AS-001 MOD-INTAKE-BELT: an 18 m translating slat deck. Kinematic and
    // in the moving-support set, so riding it inherits its velocity under
    // the WO-002 support-point law rather than being scenery you slide on.
    const double belt_z =
        kIntakeBeltCenterZ +
        0.5 * kIntakeBeltStrokeMeters *
            std::sin(kIntakeBeltAngularFrequency * next_time_seconds);
    bodies.MoveKinematic(
        intake_belt_id_,
        JPH::RVec3(kIntakeBeltX, kIntakeBeltTopY - kIntakeBeltHalfY, belt_z),
        JPH::Quat::sIdentity(),
        delta_seconds);
}

} // namespace scraperx::sim
