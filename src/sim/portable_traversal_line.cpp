#include "sim/portable_traversal_line.hpp"
#include "sim/finite_line_reel.hpp"
#include "sim/rigging_system.hpp"
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace scraperx::sim {
namespace {
using namespace JPH;
constexpr float stand_off_m = .025F;
constexpr float initial_slack_m = .025F;
constexpr float unloaded_n = 60; // Existing Kit unhook policy.
constexpr float recovery_speed_mps = 2;
constexpr float fitting_half_x_m = .14F, fitting_half_y_m = .02F, fitting_half_z_m = .025F;
double fitting_kinetic_energy(const Body &body) {
    const Vec3 angular = body.GetRotation().Conjugated() * body.GetAngularVelocity();
    const Vec3 inertia = PortableTraversalLine::fitting_mass_kg / 3 * Vec3(
        fitting_half_y_m * fitting_half_y_m + fitting_half_z_m * fitting_half_z_m,
        fitting_half_x_m * fitting_half_x_m + fitting_half_z_m * fitting_half_z_m,
        fitting_half_x_m * fitting_half_x_m + fitting_half_y_m * fitting_half_y_m);
    return .5 * PortableTraversalLine::fitting_mass_kg * double(body.GetLinearVelocity().LengthSq()) +
        .5 * double((inertia * angular).Dot(angular));
}
template<class V> bool finite(const V &v) {
    return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) && std::isfinite(v.GetZ());
}
struct Filter final : BodyFilter {
    BodyID player, fitting, ignored;
    bool ShouldCollide(const BodyID &id) const override {
        return id != player && id != fitting && id != ignored;
    }
    bool ShouldCollideLocked(const Body &body) const override { return !body.IsSensor(); }
};
bool beam_leaf(const Shape &shape, SubShapeID subshape) {
    // Mutable compounds require an explicit geometry-generation provider.
    // Admit immutable Box/StaticCompound mounting families in this slice.
    if (shape.GetSubType() != EShapeSubType::Box && shape.GetSubType() != EShapeSubType::StaticCompound)
        return false;
    SubShapeID remainder;
    const Shape *leaf = shape.GetLeafShape(subshape, remainder);
    if (!leaf || leaf->GetSubType() != EShapeSubType::Box) return false;
    const Vec3 half = static_cast<const BoxShape *>(leaf)->GetHalfExtent();
    const float longest = std::max({half.GetX(), half.GetY(), half.GetZ()});
    const float shortest = std::min({half.GetX(), half.GetY(), half.GetZ()});
    const float middle = half.GetX() + half.GetY() + half.GetZ() - longest - shortest;
    // First mounting family: a beam/rail whose section fits the carried strap.
    // Geometry only selects a real reduced-order clamp; the rope retains its
    // finite 3000N strength and loads the actual supporting body's local point.
    return longest >= .5F && shortest >= .04F && middle <= .5F && longest >= 2 * middle;
}
}

PortableTraversalLine::PortableTraversalLine(JPH::PhysicsSystem &world, JPH::BodyID player,
        JPH::ObjectLayer layer, PhysicalHandClimb &hands, const RiggingSystem *eyes)
    : world_(world), player_(player), hands_(hands), eyes_(eyes) {
    BodyCreationSettings settings(new BoxShape(Vec3(fitting_half_x_m, fitting_half_y_m, fitting_half_z_m)), RVec3::sZero(),
        Quat::sIdentity(), EMotionType::Dynamic, layer);
    settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = fitting_mass_kg;
    settings.mMotionQuality = EMotionQuality::LinearCast;
    auto *body = world_.GetBodyInterface().CreateBody(settings);
    if (!body) throw std::runtime_error("Portable traversal line: expected capacity for one 0.25kg grip fitting");
    fitting_ = body->GetID();
}
PortableTraversalLine::~PortableTraversalLine() {
    detach_for_restore();
    world_.GetBodyInterface().DestroyBody(fitting_);
}
bool PortableTraversalLine::reject(Failure reason) {
    failure_ = reason; ++event_serial_; return false;
}
JPH::Body *PortableTraversalLine::support(const Anchor &a) const {
    auto *body = world_.GetBodyLockInterfaceNoLock().TryGetBody(a.body);
    return body && body->IsInBroadPhase() && !body->IsSensor() &&
        body->GetShape() == a.shape_identity.GetPtr() &&
        (registered_eye(a) || beam_leaf(*body->GetShape(), a.subshape))
        ? body : nullptr;
}
bool PortableTraversalLine::registered_eye(const Anchor &a) const {
    if (!eyes_) return false;
    for (std::size_t i = 0; i < eyes_->eye_count(); ++i) {
        const auto *eye = eyes_->available_eye(i);
        if (eye && eye->kind == RiggingSystem::Kind::End && eye->identity == a.body &&
            eye->rating_n >= GrappleRopeSettings::strength_n &&
            (eye->local - a.point).LengthSq() < 1.e-10F) return true;
    }
    return false;
}
JPH::RVec3 PortableTraversalLine::point(const Anchor &a) const {
    const auto *body = support(a);
    return body ? body->GetWorldTransform() * a.point : RVec3::sZero();
}
bool PortableTraversalLine::clear(JPH::RVec3 from, JPH::RVec3 to, JPH::BodyID ignored) const {
    const Vec3 delta(to - from);
    if (delta.LengthSq() < 1.e-8F) return true;
    Filter filter; filter.player = player_; filter.fitting = fitting_; filter.ignored = ignored;
    RayCastResult hit;
    return !world_.GetNarrowPhaseQuery().CastRay(RRayCast(from, delta), hit, {}, {}, filter);
}
bool PortableTraversalLine::gripping() const {
    for (unsigned i = 0; i < 2; ++i) {
        if (hands_.attached_to(i, fitting_)) return true;
    }
    return false;
}
bool PortableTraversalLine::unloaded() const {
    if (rope_ && rope_->tension() > unloaded_n) return false;
    if (!gripping()) return true;
    // A fresh catch has not solved yet, so zero cached force cannot authorize
    // stowing. Once settled, real hands may hold the unloaded fitting while
    // packing; hand attachment alone is not evidence of supporting the player.
    if (!held_solved_) return false;
    float hand_load = 0;
    for (unsigned i = 0; i < 2; ++i) {
        const auto grip = hands_.grip_readback(i);
        if (grip.attached && grip.support == fitting_) hand_load += grip.player_force.Length();
    }
    return hand_load <= unloaded_n;
}
JPH::RVec3 PortableTraversalLine::deployment_position() const {
    // Reach forward from the shoulder, independently of looking up/down. A
    // downward camera aim must not place real hardware inside the capsule.
    const auto forward = Vec3(aim_.GetX(), 0, aim_.GetZ()).NormalizedOr(-Vec3::sAxisZ());
    return hand_ + forward * .45F;
}
JPH::Quat PortableTraversalLine::deployment_rotation() const {
    const auto forward = Vec3(aim_.GetX(), 0, aim_.GetZ()).NormalizedOr(-Vec3::sAxisZ());
    return Quat::sFromTo(Vec3::sAxisX(), forward.Cross(Vec3::sAxisY()));
}
bool PortableTraversalLine::deployment_clear() const {
    const auto *fitting = world_.GetBodyLockInterfaceNoLock().TryGetBody(fitting_);
    CollideShapeSettings clearance;
    AnyHitCollisionCollector<CollideShapeCollector> overlap;
    Filter filter; filter.fitting = fitting_;
    const auto at = deployment_position();
    world_.GetNarrowPhaseQuery().CollideShape(fitting->GetShape(), Vec3::sReplicate(1),
        RMat44::sRotationTranslation(deployment_rotation(), at), clearance, at, overlap, {}, {}, filter);
    return !overlap.HadHit();
}
void PortableTraversalLine::equip(bool value) {
    if (!value) release();
    equipped_ = value; allowed_ = false; target_ = 0; reel_input_ = 0;
}
void PortableTraversalLine::aim(JPH::RVec3 hand, JPH::Vec3 direction, bool allowed) {
    hand_ = hand;
    allowed_ = equipped_ && allowed && finite(hand) && finite(direction) && direction.LengthSq() > 1.e-8F;
    target_ = 0;
    if (!allowed_ || installed_) return;
    aim_ = direction.Normalized();
    if (!deployment_clear()) return;
    Vec3 tangent = aim_.Cross(Vec3::sAxisY()).NormalizedOr(Vec3::sAxisX());
    Vec3 vertical = tangent.Cross(aim_).Normalized();
    float best = -1;
    Anchor chosen;
    Filter filter; filter.player = player_; filter.fitting = fitting_;
    // Small assisted cone, each ray keeps the first actual solid hit. A wall
    // never becomes transparent merely because a beam lies behind it.
    for (unsigned i = 0; i < 9; ++i) {
        const float x = float(int(i % 3) - 1) * .08F;
        const float y = float(int(i / 3) - 1) * .08F;
        const Vec3 ray_direction = (aim_ + tangent * x + vertical * y).Normalized();
        const RRayCast ray(hand_, ray_direction * install_reach_m);
        RayCastResult hit;
        if (!world_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, filter)) continue;
        auto *body = world_.GetBodyLockInterfaceNoLock().TryGetBody(hit.mBodyID);
        if (!body || !beam_leaf(*body->GetShape(), hit.mSubShapeID2)) continue;
        const auto surface = ray.GetPointOnRay(hit.mFraction);
        const auto normal = body->GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, surface);
        const auto at = surface + normal * stand_off_m;
        const Vec3 delta(at - hand_);
        const float distance = delta.Length();
        if (distance < .08F || distance > install_reach_m || !clear(hand_, at, BodyID()) ||
            !clear(deployment_position(), at, BodyID())) continue;
        const float score = delta.Dot(aim_) / distance - .04F * distance;
        if (score <= best) continue;
        best = score;
        chosen.body = body->GetID(); chosen.subshape = hit.mSubShapeID2;
        chosen.shape_identity = body->GetShape();
        chosen.point = Vec3(body->GetWorldTransform().Inversed() * at);
        chosen.normal = body->GetRotation().Conjugated() * normal;
    }
    // Reuse rated world eyes without another mounting registry. Clear rays
    // still include the supporting compound, so another leaf cannot turn
    // transparent merely because the endpoint belongs to that body.
    if (eyes_) for (std::size_t i = 0; i < eyes_->eye_count(); ++i) {
        const auto *eye = eyes_->available_eye(i);
        if (!eye || eye->kind != RiggingSystem::Kind::End ||
            eye->rating_n < GrappleRopeSettings::strength_n) continue;
        const auto *body = world_.GetBodyLockInterfaceNoLock().TryGetBody(eye->identity);
        const auto at = body->GetWorldTransform() * eye->local;
        const Vec3 delta(at - hand_);
        const float distance = delta.Length();
        if (distance < .08F || distance > install_reach_m || delta.Dot(aim_) / distance < .8F ||
            !clear(hand_, at, BodyID()) || !clear(deployment_position(), at, BodyID())) continue;
        const float score = delta.Dot(aim_) / distance - .04F * distance;
        if (score <= best) continue;
        best = score; chosen = {}; chosen.body = eye->identity;
        chosen.shape_identity = body->GetShape(); chosen.point = eye->local;
    }
    if (best < 0) return;
    // Preserve a displayed token through harmless aim jitter on one fitting
    // footprint. Install revalidates its current geometry, reach and clearance.
    if (candidate_.body != chosen.body || candidate_.subshape != chosen.subshape ||
        candidate_.shape_identity != chosen.shape_identity ||
        (candidate_.point - chosen.point).LengthSq() > .04F * .04F) {
        candidate_ = chosen; ++epoch_;
    }
    target_ = epoch_;
}
bool PortableTraversalLine::connect() {
    auto *anchor = support(anchor_);
    auto *fitting = world_.GetBodyLockInterfaceNoLock().TryGetBody(fitting_);
    if (!anchor || !fitting || !fitting->IsInBroadPhase()) return false;
    path_ = PortableLinePath::evaluate(world_, path_input(), path_state_);
    if (!path_.valid) return false;
    rope_ = new FiniteCableConstraint(paid_length_m_);
    if (rope_->set_geometry(path_.participants.data(), path_.participant_count, path_.length_m) !=
        FiniteCableConstraint::Admission::Accepted) { rope_ = nullptr; return false; }
    world_.AddConstraint(rope_);
    world_.GetBodyInterface().ActivateBody(fitting_);
    if (anchor->IsDynamic()) world_.GetBodyInterface().ActivateBody(anchor_.body);
    return true;
}
PortableLinePath::Input PortableTraversalLine::path_input() const {
    PortableLinePath::Input input;
    input.player = player_; input.fitting = fitting_; input.anchor_body = anchor_.body;
    input.anchor_shape_identity = anchor_.shape_identity;
    input.anchor_shape_local_point = anchor_.point;
    const auto *fitting = world_.GetBodyLockInterfaceNoLock().TryGetBody(fitting_);
    if (fitting) input.endpoint_a = fitting->GetCenterOfMassPosition();
    return input;
}
void PortableTraversalLine::disconnect() {
    if (rope_) { paid_length_m_ = rope_->rest(); world_.RemoveConstraint(rope_); rope_ = nullptr; }
}
void PortableTraversalLine::remove_fitting() {
    if (world_.GetBodyInterface().IsAdded(fitting_)) world_.GetBodyInterface().RemoveBody(fitting_);
}
bool PortableTraversalLine::install(std::uint64_t shown) {
    if (!allowed_) return reject(Failure::UnsafeContext);
    if (installed_) return reject(Failure::AlreadyInstalled);
    if (!shown || shown != target_) return reject(Failure::StaleTarget);
    auto *anchor = support(candidate_);
    if (!anchor) return reject(Failure::SupportChanged);
    const auto at = point(candidate_);
    if (Vec3(at - hand_).Length() > install_reach_m) return reject(Failure::OutOfReach);
    const auto fitting_position = deployment_position();
    const float distance = Vec3(at - fitting_position).Length();
    if (!clear(hand_, at, BodyID()) || !clear(fitting_position, at, BodyID()))
        return reject(Failure::Obstructed);
    // Deployment must fit the actual hardware, including the player's capsule.
    // A free ray is insufficient evidence for a box-sized grip fitting.
    if (!deployment_clear()) return reject(Failure::Obstructed);
    // Equipment allocation occurs here, outside the solve. No player pose or
    // velocity changes. Paid length is never shorter than accepted geometry.
    auto &bodies = world_.GetBodyInterface();
    bodies.SetPositionAndRotation(fitting_, fitting_position, deployment_rotation(), EActivation::DontActivate);
    const auto *player = world_.GetBodyLockInterfaceNoLock().TryGetBody(player_);
    bodies.SetLinearAndAngularVelocity(fitting_, player ? player->GetPointVelocity(fitting_position) : Vec3::sZero(), Vec3::sZero());
    bodies.AddBody(fitting_, EActivation::Activate);
    anchor_ = candidate_; path_state_ = {};
    paid_length_m_ = std::max(minimum_length_m, distance + initial_slack_m);
    if (!connect()) { remove_fitting(); return reject(Failure::SupportChanged); }
    installed_ = true; broken_ = false; target_ = 0; ++generation_; ++event_serial_;
    failure_ = Failure::None;
    return true;
}
bool PortableTraversalLine::grab() {
    if (!allowed_) return reject(Failure::UnsafeContext);
    if (!installed_ || broken_ || !rope_) return reject(Failure::NoLine);
    if (hands_.active() && !gripping()) return reject(Failure::HandsBusy);
    auto &bodies = world_.GetBodyInterface();
    const auto transform = bodies.GetWorldTransform(fitting_);
    if ((transform.GetTranslation() - hand_).LengthSq() > install_reach_m * install_reach_m)
        return reject(Failure::OutOfReach);
    if (!clear(hand_, transform.GetTranslation(), BodyID())) return reject(Failure::Obstructed);
    bool attached = false;
    for (unsigned i = 0; i < 2; ++i) {
        if (hands_.attached(i)) continue;
        attached |= hands_.attach(i, fitting_, transform * Vec3(i ? .10F : -.10F, 0, 0));
    }
    if (!attached && !gripping()) return reject(Failure::OutOfReach);
    if (attached) held_solved_ = false;
    hands_.set_swing_profile(); failure_ = Failure::None; ++event_serial_;
    return true;
}
void PortableTraversalLine::release() {
    const bool held = gripping();
    for (unsigned i = 0; i < 2; ++i) {
        const auto grip = hands_.grip_readback(i);
        if (grip.attached && grip.support == fitting_) hands_.detach(i);
    }
    reel_input_ = 0;
    held_solved_ = false;
    if (held) ++event_serial_;
}
bool PortableTraversalLine::reachable_equipment() const {
    if (!installed_) return false;
    const auto grip = world_.GetBodyInterface().GetPosition(fitting_);
    return ((grip - hand_).LengthSq() <= install_reach_m * install_reach_m && clear(hand_, grip, BodyID())) ||
        (support(anchor_) && (point(anchor_) - hand_).LengthSq() <= install_reach_m * install_reach_m &&
            clear(hand_, point(anchor_), BodyID()));
}
bool PortableTraversalLine::recover() {
    if (!allowed_) return reject(Failure::UnsafeContext);
    if (!installed_) return reject(Failure::NoLine);
    if (!unloaded()) return reject(Failure::Loaded);
    if (!reachable_equipment()) return reject(Failure::OutOfReach);
    auto *fitting = world_.GetBodyLockInterfaceNoLock().TryGetBody(fitting_);
    const auto velocity = fitting->GetLinearVelocity();
    const double kinetic = fitting_kinetic_energy(*fitting);
    if (velocity.LengthSq() > recovery_speed_mps * recovery_speed_mps || kinetic > .5)
        return reject(Failure::MovingTooFast);
    // Explicit finite inventory recovery abstraction, separately reported from
    // reel work. Only the light unloaded fitting is removed, never its support.
    recovery_absorbed_j_ += kinetic + (rope_ ? FiniteLineReel::potential(
        path_.valid ? path_.length_m : Vec3(point(anchor_) - fitting->GetCenterOfMassPosition()).Length(),
        rope_->rest()) : 0);
    release(); disconnect(); remove_fitting(); installed_ = broken_ = false;
    anchor_ = {}; path_state_ = {}; path_ = {};
    ++generation_; ++epoch_; ++event_serial_; failure_ = Failure::None;
    return true;
}
void PortableTraversalLine::set_reel_input(float value) {
    reel_input_ = std::isfinite(value) ? std::clamp(value, -1.0F, 1.0F) : 0;
}
void PortableTraversalLine::collision_step(float dt) {
    if (!rope_ || !installed_ || broken_) return;
    const auto *anchor = support(anchor_);
    const auto *fitting = world_.GetBodyLockInterfaceNoLock().TryGetBody(fitting_);
    if (!anchor || !fitting) { (void)rope_->set_geometry(nullptr, 0, 0); return; }
    const float solved_tension = rope_->tension();
    path_ = PortableLinePath::evaluate(world_, path_input(), path_state_);
    if (!path_.valid || rope_->set_geometry(path_.participants.data(), path_.participant_count, path_.length_m) !=
        FiniteCableConstraint::Admission::Accepted) {
        // Model limits are explicit readback. Never substitute a straight row,
        // remembered world pin or fabricated break across unknown geometry.
        (void)rope_->set_geometry(nullptr, 0, 0);
        return;
    }
    // Reeling is a held-end human action. An unattended installed line is
    // passive; no remote powered shortening is smuggled in by changing tools.
    const float input = equipped_ && gripping() ? reel_input_ : 0;
    const auto result = FiniteLineReel::step(rope_->rest(), float(path_.length_m), solved_tension, dt, input,
        minimum_length_m, maximum_length_m,
        [&](float rest, float step) { return rope_->prospective_tension(rest, step); });
    rope_->set_rest(result.rest_length_m); rope_->set_drive_limited(result.drive_limited);
    paid_length_m_ = result.rest_length_m;
    reel_work_j_ += result.work_j; spool_absorbed_j_ += result.absorbed_j;
}
void PortableTraversalLine::post_step() {
    if (!installed_ || broken_) return;
    held_solved_ = gripping();
    if (!support(anchor_)) {
        release(); disconnect(); broken_ = true; reject(Failure::SupportChanged);
    } else if (rope_ && rope_->broken()) {
        release(); disconnect(); broken_ = true; reject(Failure::StrengthExceeded);
    } else {
        // Refresh only the visible solved geometry. Rebinding the row here
        // would erase the actual tension receipt from the preceding solve.
        auto visual_state = path_state_;
        path_ = PortableLinePath::evaluate(world_, path_input(), visual_state);
        if (!path_.valid && failure_ != Failure::Obstructed) reject(Failure::Obstructed);
        else if (path_.valid && failure_ == Failure::Obstructed) failure_ = Failure::None;
    }
}
PortableTraversalLine::Readback PortableTraversalLine::readback() const {
    Readback out;
    out.equipped = equipped_; out.installed = installed_; out.broken = broken_; out.gripping = gripping();
    out.available_spools = installed_ ? 0 : 1;
    out.target = target_; out.target_valid = target_ && support(candidate_);
    out.generation = generation_; out.event_serial = event_serial_; out.failure = failure_;
    out.support = anchor_.body; out.fitting = installed_ ? fitting_ : BodyID();
    if (out.target_valid) out.target_point = point(candidate_);
    if (installed_) {
        out.anchor_point = point(anchor_); out.grip_point = world_.GetBodyInterface().GetPosition(fitting_);
        out.paid_length_m = rope_ ? rope_->rest() : paid_length_m_;
        out.tension_n = rope_ ? rope_->tension() : 0;
        out.path_valid = path_.valid; out.path_reason = unsigned(path_.reason);
        out.path_length_m = path_.length_m; out.path_token = path_.topology_token;
        out.contact_count = path_.has_contact ? 1U : 0U;
        out.path_points = path_.visual_points; out.path_point_count = path_.visual_point_count;
        out.can_grab = allowed_ && !broken_ && !out.gripping && !hands_.active() &&
            (out.grip_point - hand_).LengthSq() <= install_reach_m * install_reach_m &&
            clear(hand_, out.grip_point, BodyID());
        out.can_recover = allowed_ && unloaded() && reachable_equipment() &&
            world_.GetBodyInterface().GetLinearVelocity(fitting_).LengthSq() <= recovery_speed_mps * recovery_speed_mps &&
            fitting_kinetic_energy(*world_.GetBodyLockInterfaceNoLock().TryGetBody(fitting_)) <= .5;
    }
    out.reel_work_j = reel_work_j_; out.spool_absorbed_j = spool_absorbed_j_;
    out.recovery_absorbed_j = recovery_absorbed_j_;
    return out;
}
PortableTraversalLine::Checkpoint PortableTraversalLine::capture() const {
    Checkpoint out;
    out.installed = installed_; out.broken = broken_; out.anchor = anchor_; out.path = path_state_;
    out.paid_length_m = rope_ ? rope_->rest() : paid_length_m_;
    out.reel_work_j = reel_work_j_; out.spool_absorbed_j = spool_absorbed_j_;
    out.recovery_absorbed_j = recovery_absorbed_j_;
    if (installed_) {
        const auto &bodies = world_.GetBodyInterface();
        out.fitting_position = bodies.GetPosition(fitting_); out.fitting_rotation = bodies.GetRotation(fitting_);
        out.linear_velocity = bodies.GetLinearVelocity(fitting_); out.angular_velocity = bodies.GetAngularVelocity(fitting_);
    }
    return out;
}
void PortableTraversalLine::detach_for_restore() {
    release(); disconnect(); remove_fitting(); installed_ = broken_ = false;
    target_ = 0; anchor_ = {}; path_state_ = {}; path_ = {}; paid_length_m_ = 0;
    allowed_ = equipped_ = false; ++epoch_; ++generation_;
}
bool PortableTraversalLine::restore(const Checkpoint &state) {
    detach_for_restore();
    if (!state.installed) {
        reel_work_j_ = state.reel_work_j; spool_absorbed_j_ = state.spool_absorbed_j;
        recovery_absorbed_j_ = state.recovery_absorbed_j; failure_ = Failure::None;
        return true;
    }
    if (!support(state.anchor) || !std::isfinite(state.paid_length_m) ||
        state.paid_length_m < minimum_length_m || state.paid_length_m > maximum_length_m ||
        !finite(state.fitting_position) || !finite(state.linear_velocity) || !finite(state.angular_velocity) ||
        !std::isfinite(state.fitting_rotation.GetX()) || !std::isfinite(state.fitting_rotation.GetY()) ||
        !std::isfinite(state.fitting_rotation.GetZ()) || !std::isfinite(state.fitting_rotation.GetW()) ||
        std::abs(state.fitting_rotation.LengthSq() - 1) > .001F)
        return reject(Failure::StaleTarget);
    anchor_ = state.anchor; path_state_ = state.path; paid_length_m_ = state.paid_length_m;
    auto &bodies = world_.GetBodyInterface();
    bodies.SetPositionAndRotation(fitting_, state.fitting_position, state.fitting_rotation, EActivation::DontActivate);
    bodies.SetLinearAndAngularVelocity(fitting_, state.linear_velocity, state.angular_velocity);
    bodies.AddBody(fitting_, EActivation::Activate);
    installed_ = true; broken_ = state.broken;
    if (!broken_ && !connect()) { remove_fitting(); installed_ = false; return reject(Failure::SupportChanged); }
    reel_work_j_ = state.reel_work_j; spool_absorbed_j_ = state.spool_absorbed_j;
    recovery_absorbed_j_ = state.recovery_absorbed_j; failure_ = Failure::None;
    return true;
}
} // namespace scraperx::sim
