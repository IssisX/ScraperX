#include "sim/yard_crane.hpp"
#include "sim/warehouse_forklift_yard.hpp"

#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <limits>

namespace scraperx::sim {
using namespace JPH;
namespace {
constexpr double kEfficiency = .8, kTransportPower = 20000, kTransportCapacity = 2000000;
constexpr float kSlewTorque = 100000, kTrolleyForce = 20000;
float demand(float v) { return std::isfinite(v) ? std::clamp(v, -1.F, 1.F) : 0; }
float approach(float a, float b, float step) { return std::clamp(b, a - step, a + step); }
}

struct YardCrane::Impl {
    PhysicsSystem &world;
    kit::Kit &kit;
    WarehouseForklift::Config truck;
    YardCraneGeometry geometry;
    State state;
    Command command;
    Ref<HingeConstraint> slew;
    Ref<SliderConstraint> track;
    std::unique_ptr<PoweredCable> hoist;
    std::array<std::unique_ptr<PoweredCable>, 4> slings;
    RefConst<Shape> cable_probe = new SphereShape(.025F);
    float last_angle = 0, last_track = 0, pending_dt = 0;
    double reserved_j = 0;
    bool pending = false, energized = false, registered = false;
    struct Listener final : PhysicsStepListener {
        Impl &owner;
        explicit Listener(Impl &v) : owner(v) {}
        void OnStep(const PhysicsStepListenerContext &c) override { owner.collision_step(c.mDeltaTime); }
    } listener{*this};

    Impl(PhysicsSystem &w, kit::Kit &k, const WarehouseForklift::Config &t)
        : world(w), kit(k), truck(t), geometry(build_yard_crane_geometry(k)) {}
    ~Impl() {
        if (registered) world.RemoveStepListener(&listener);
        for (auto &line : slings) line.reset();
        hoist.reset();
        if (track) world.RemoveConstraint(track);
        if (slew) world.RemoveConstraint(slew);
    }
    Body &body(kit::BodyIndex i) {
        return *world.GetBodyLockInterfaceNoLock().TryGetBody(kit.body_id(i));
    }
    RVec3 point(kit::BodyIndex i, Vec3Arg local) { return body(i).GetWorldTransform() * local; }
    PoweredCable::Config hoist_config() {
        PoweredCable::Config c;
        c.support = kit.body_id(geometry.trolley); c.load = kit.body_id(geometry.spreader);
        c.support_eye = geometry.hoist_support_eye; c.load_eye = geometry.hoist_load_eye;
        c.initial_length_m = float((point(geometry.trolley, c.support_eye) -
                                    point(geometry.spreader, c.load_eye)).Length());
        c.min_length_m = 4; c.max_length_m = 34;
        return c;
    }
    PoweredCable::Config sling_config(unsigned i) {
        PoweredCable::Config c;
        c.support = kit.body_id(geometry.spreader); c.load = kit.body_id(truck.chassis);
        c.support_eye = geometry.spreader_eyes[i];
        c.load_eye = WarehouseForkliftYard::lifting_points()[i];
        c.initial_length_m = c.min_length_m = c.max_length_m = geometry.sling_lengths[i];
        c.capacity_j = 0; c.brake_force_n = c.pull_force_n = 40000;
        return c;
    }
    bool clear_segment(kit::BodyIndex a, kit::BodyIndex b, RVec3Arg from, RVec3Arg to) {
        const Vec3 travel(to - from);
        if (travel.IsNearZero()) return true;
        IgnoreMultipleBodiesFilter filter;
        filter.IgnoreBody(kit.body_id(a)); filter.IgnoreBody(kit.body_id(b));
        ClosestHitCollisionCollector<CastShapeCollector> hit;
        const RShapeCast cast(cable_probe.GetPtr(), Vec3::sReplicate(1),
                               RMat44::sTranslation(from), travel);
        world.GetNarrowPhaseQuery().CastShape(cast, ShapeCastSettings(), from, hit, {}, {}, filter);
        return !hit.HadHit();
    }
    bool supported(const WarehouseForklift &forklift) {
        const auto wheels = forklift.wheels();
        for (const auto &wheel : wheels)
            if (!wheel.contact || wheel.contact_normal.GetY() < .7F || wheel.normal_force_n <= 0) return false;
        return body(truck.chassis).GetLinearVelocity().Length() < .10F &&
               body(truck.chassis).GetAngularVelocity().Length() < .05F;
    }
    bool transport_ready(const WarehouseForklift &forklift) {
        auto &m = *this;
        if (!supported(forklift)) return false;
    // Stow must clear the actual receiving plane without jacking the truck
    // onto its tines. A scalar lift height cannot establish this geometry.
    const auto transport = forklift.state();
    if (!transport.coupled ||
        !(std::abs(transport.tilt_angle_rad - forklift.config().tilt_min_rad) <= .02F)) return false;
    const auto wheels = forklift.wheels();
    const auto origin = wheels[0].contact_position;
    Vec3 normal = Vec3(wheels[1].contact_position - origin).Cross(Vec3(wheels[2].contact_position - origin));
    if (normal.IsNaN() || normal.LengthSq() < 1e-8F) return false;
    normal = normal.Normalized();
    if (normal.GetY() < 0) normal = -normal;
    if (normal.GetY() < .7F) return false;
    float clearance = std::numeric_limits<float>::infinity();
    const auto &parts = m.kit.body_parts(m.truck.fork_frame);
    if (parts.empty()) return false;
    for (const auto &part : parts) {
        if (part.shape != kit::Part::Shape::Box) return false;
        for (float x : {-1.F, 1.F}) for (float y : {-1.F, 1.F}) for (float z : {-1.F, 1.F}) {
            const Vec3 local = part.offset + part.rotation * Vec3(x * part.half.GetX(), y * part.half.GetY(), z * part.half.GetZ());
            const float height = Vec3(m.point(m.truck.fork_frame, local) - origin).Dot(normal);
            if (!std::isfinite(height)) return false;
            clearance = std::min(clearance, height);
        }
    }
    if (clearance < .08F) return false;
    // The wheel plane establishes grade, not clearance from raised freight
    // or obstacles. Reject actual solved external contact on the real fork.
    BodyIDVector bodies; m.world.GetBodies(bodies);
    const auto fork = m.kit.body_id(m.truck.fork_frame);
    for (const auto id : bodies) {
        if (id == fork || id == m.kit.body_id(m.truck.chassis) ||
            id == m.kit.body_id(m.truck.mast) || id == m.kit.body_id(m.truck.carriage)) continue;
        if (m.world.WereBodiesInContact(fork, id)) return false;
    }
        return true;
    }
    bool leg_ready(unsigned i) {
        auto &m = *this;
        const auto target = point(truck.chassis, WarehouseForkliftYard::lifting_points()[i]);
        const auto top = point(geometry.spreader, geometry.spreader_eyes[i]);
        return (target - state.loose_ends[i]).LengthSq() <= .06F * .06F &&
               (target - top).Length() <= geometry.sling_lengths[i] + .0001F &&
               (body(truck.chassis).GetPointVelocity(target) -
                body(geometry.spreader).GetPointVelocity(state.loose_ends[i])).Length() <= .15F &&
               m.clear_segment(geometry.spreader, truck.chassis, top, target);
    }
    bool slack(unsigned i) const {
        if (!slings[i]) return false;
        const auto s = slings[i]->state();
        return s.tensile_force_n <= 150 && s.actual_length_m <= s.rest_length_m - .02F;
    }
    bool release_path_ready(unsigned i) {
        // An attached hook is already at the eye. Its unloaded cable may
        // have more slack than the loose-end pickup capture distance.
        const auto s = slings[i]->state();
        return (body(truck.chassis).GetPointVelocity(s.load_endpoint) -
                body(geometry.spreader).GetPointVelocity(s.support_endpoint)).Length() <= .15F &&
               clear_segment(geometry.spreader, truck.chassis, s.support_endpoint, s.load_endpoint);
    }
    void account() {
        if (!pending) return;
        const double torque = slew->GetTotalLambdaMotor() / pending_dt;
        const double force = track->GetTotalLambdaMotor() / pending_dt;
        const double angle_delta = std::remainder(double(slew->GetCurrentAngle() - last_angle), 2 * JPH_PI);
        const double slew_work = torque * angle_delta;
        const double trolley_work = force * (track->GetCurrentPosition() - last_track);
        const double absorbed = std::max(0., -slew_work) + std::max(0., -trolley_work);
        state.slew_torque_nm = float(torque); state.trolley_force_n = float(force);
        if (energized) {
            const double positive = std::max(0., slew_work) + std::max(0., trolley_work);
            const double debit = positive / kEfficiency + 60 * pending_dt;
            state.reservation_excess_j = std::max(state.reservation_excess_j, debit - reserved_j);
            state.transport_positive_work_j += positive;
            state.transport_heat_j += debit - positive + absorbed;
            state.transport_energy_j = std::max(0., state.transport_energy_j - debit);
        } else state.transport_heat_j += absorbed;
        pending = false;
    }
    void collision_step(float dt) {
        account();
        const bool available = state.transport_energy_j > 0 && state.failure == Failure::None;
        const bool requested = command.slew != 0 || command.trolley != 0;
        energized = available && requested;
        state.slew_target_rad_s = energized ? approach(state.slew_target_rad_s, .05F * command.slew, .02F * dt) : 0;
        state.trolley_target_mps = energized ? approach(state.trolley_target_mps, .25F * command.trolley, .10F * dt) : 0;
        const float omega = (body(geometry.boom).GetAngularVelocity() - body(geometry.mast).GetAngularVelocity()).Length();
        const float velocity = (body(geometry.trolley).GetLinearVelocity() - body(geometry.boom).GetLinearVelocity()).Length();
        const double angular_speed = std::max({double(omega), double(std::abs(state.slew_target_rad_s)), .01});
        const double linear_speed = std::max({double(velocity), double(std::abs(state.trolley_target_mps)), .05});
        const double torque = std::min(double(kSlewTorque), .4 * kTransportPower * kEfficiency / angular_speed);
        const double force = std::min(double(kTrolleyForce), .6 * kTransportPower * kEfficiency / linear_speed);
        reserved_j = ((torque * (angular_speed + .02 * dt) + force * (linear_speed + .1 * dt)) / kEfficiency + 60) * dt;
        if (state.transport_energy_j < reserved_j) {
            energized = false; state.transport_cutoff = true;
            state.slew_target_rad_s = state.trolley_target_mps = 0;
        } else state.transport_cutoff = !available;
        slew->GetMotorSettings().SetTorqueLimit(float(torque));
        slew->SetMotorState(energized && command.slew != 0 ? EMotorState::Velocity : EMotorState::Off);
        slew->SetTargetAngularVelocity(state.slew_target_rad_s);
        slew->SetMaxFrictionTorque(energized && command.slew != 0 ? 0 : kSlewTorque);
        track->GetMotorSettings().SetForceLimit(float(force));
        track->SetMotorState(energized && command.trolley != 0 ? EMotorState::Velocity : EMotorState::Off);
        track->SetTargetVelocity(state.trolley_target_mps);
        track->SetMaxFrictionForce(energized && command.trolley != 0 ? 0 : kTrolleyForce);
        last_angle = slew->GetCurrentAngle(); last_track = track->GetCurrentPosition();
        pending_dt = dt; pending = true;
        if (hoist) hoist->collision_step(dt);
        for (auto &line : slings) if (line) line->collision_step(dt);
    }
    void refresh() {
        state.slew_angle_rad = geometry.boom_yaw_initial + slew->GetCurrentAngle();
        state.trolley_position_m = geometry.initial_trolley_m + track->GetCurrentPosition();
        state.hoist_connected = bool(hoist);
        if (hoist) state.hoist = hoist->state();
        else {
            state.hoist.tensile_force_n = state.hoist.last_impulse_ns = 0;
            state.hoist.load_force = Vec3::sZero();
        }
        for (unsigned i = 0; i < slings.size(); ++i) {
            if (slings[i]) state.slings[i] = slings[i]->state();
            else state.slings[i] = {};
            state.loose_ends[i] = point(geometry.spreader, geometry.spreader_eyes[i]) -
                                   Vec3(0, geometry.sling_lengths[i], 0);
        }
    }
};

YardCrane::YardCrane(PhysicsSystem &world, kit::Kit &kit, const WarehouseForklift::Config &truck)
    : impl_(std::make_unique<Impl>(world, kit, truck)) {
    auto &m = *impl_; const auto &g = m.geometry;
    // Fine positioning produces real sub-microradian/sub-ULP steps. Retain
    // them through the existing opt-in integrator instead of introducing an
    // artificial minimum control rate or a target-position motor.
    for (const auto body : {g.mast, g.boom, g.trolley, g.spreader})
        kit.set_accurate_integration(body);
    // Visible concrete transfers the rated mast connection into the yard.
    kit.add_breakable_weld(g.foundation, g.mast, 3000000, 25000000);
    HingeConstraintSettings h;
    h.mPoint1 = h.mPoint2 = g.pivot;
    h.mHingeAxis1 = h.mHingeAxis2 = Vec3::sAxisY();
    h.mNormalAxis1 = h.mNormalAxis2 = kit.body_rotation(g.boom) * Vec3::sAxisZ();
    h.mMaxFrictionTorque = kSlewTorque; h.mNumVelocityStepsOverride = 40; h.mNumPositionStepsOverride = 8;
    m.slew = static_cast<HingeConstraint *>(h.Create(m.body(g.mast), m.body(g.boom)));
    world.AddConstraint(m.slew); kit.disable_collision(g.mast, g.boom);
    SliderConstraintSettings s;
    s.mAutoDetectPoint = true; s.SetSliderAxis(kit.body_rotation(g.boom) * Vec3::sAxisZ());
    s.mLimitsMin = g.min_trolley_m - g.initial_trolley_m;
    s.mLimitsMax = g.max_trolley_m - g.initial_trolley_m;
    s.mMaxFrictionForce = kTrolleyForce; s.mNumVelocityStepsOverride = 40; s.mNumPositionStepsOverride = 8;
    m.track = static_cast<SliderConstraint *>(s.Create(m.body(g.boom), m.body(g.trolley)));
    world.AddConstraint(m.track); kit.disable_collision(g.boom, g.trolley);
    m.hoist = std::make_unique<PoweredCable>(world, m.hoist_config());
    world.AddStepListener(&m.listener); m.registered = true; m.refresh();
}
YardCrane::~YardCrane() = default;
void YardCrane::pre_step(Command c) {
    auto &m = *impl_; m.command = {demand(c.slew), demand(c.trolley), demand(c.hoist)};
    if (m.state.failure != Failure::None) m.command = {};
    if (m.hoist) m.hoist->pre_step(m.command.hoist);
    if (m.command.slew != 0 || m.command.trolley != 0) {
        m.world.GetBodyInterface().ActivateConstraint(m.slew);
        m.world.GetBodyInterface().ActivateConstraint(m.track);
    }
}
void YardCrane::post_step() {
    auto &m = *impl_; m.account();
    if (m.hoist) {
        m.hoist->post_step(); const auto s = m.hoist->state();
        if (s.actual_length_m > 34.34F) {
            m.state.failure = Failure::CableInventory; m.hoist.reset();
        } else if (!m.clear_segment(m.geometry.trolley, m.geometry.spreader, s.support_endpoint, s.load_endpoint)) {
            m.state.failure = Failure::Obstructed; m.hoist.reset();
        }
    }
    for (unsigned i = 0; i < m.slings.size(); ++i) if (m.slings[i]) {
        m.slings[i]->post_step(); const auto s = m.slings[i]->state();
        if (s.actual_length_m > m.geometry.sling_lengths[i] * 1.01F ||
            !m.clear_segment(m.geometry.spreader, m.truck.chassis, s.support_endpoint, s.load_endpoint)) {
            m.slings[i].reset(); m.state.attached_mask &= std::uint8_t(~(1U << i));
            if (m.state.failure == Failure::None) m.state.failure = Failure::SlingFailure;
        }
    }
    m.refresh();
}
bool YardCrane::try_attach(const WarehouseForklift &forklift) {
    auto &m = *impl_; m.refresh();
    if (m.state.attached_mask || m.state.failure != Failure::None || !m.transport_ready(forklift)) return false;
    for (unsigned i = 0; i < m.slings.size(); ++i) if (!m.leg_ready(i)) return false;
    for (unsigned i = 0; i < m.slings.size(); ++i)
        m.slings[i] = std::make_unique<PoweredCable>(m.world, m.sling_config(i));
    m.state.attached_mask = 15; m.refresh(); return true;
}
bool YardCrane::try_attach_leg(unsigned i, const WarehouseForklift &forklift) {
    auto &m = *impl_; m.refresh();
    if (i >= m.slings.size() || (m.state.attached_mask & (1U << i)) ||
        m.state.failure != Failure::None || !m.transport_ready(forklift) || !m.leg_ready(i)) return false;
    m.slings[i] = std::make_unique<PoweredCable>(m.world, m.sling_config(i));
    m.state.attached_mask |= std::uint8_t(1U << i); m.refresh(); return true;
}
bool YardCrane::try_detach_leg(unsigned i, const WarehouseForklift &forklift) {
    auto &m = *impl_; m.refresh();
    if (i >= m.slings.size() || !m.slings[i] || !m.transport_ready(forklift) ||
        !m.slack(i) || !m.release_path_ready(i)) return false;
    m.slings[i].reset(); m.state.attached_mask &= std::uint8_t(~(1U << i));
    m.refresh(); return true;
}
bool YardCrane::try_detach(const WarehouseForklift &forklift) {
    auto &m = *impl_;
    if (!m.state.attached_mask || !m.supported(forklift)) return false;
    for (const auto &line : m.slings) if (line) {
        const auto s = line->state();
        if (s.tensile_force_n > 150 || s.actual_length_m > s.rest_length_m - .02F) return false;
    }
    for (auto &line : m.slings) line.reset();
    m.state.attached_mask = 0; m.refresh(); return true;
}
YardCrane::State YardCrane::state() const { return impl_->state; }
const YardCraneGeometry &YardCrane::geometry() const { return impl_->geometry; }
void YardCrane::restore(const State &s) {
    if (!std::isfinite(s.transport_energy_j) || s.transport_energy_j < 0 ||
        s.transport_energy_j > kTransportCapacity || s.attached_mask > 15)
        throw std::invalid_argument("YardCrane invalid checkpoint");
    auto &m = *impl_; for (auto &line : m.slings) line.reset();
    m.hoist.reset(); m.state = s; m.command = {}; m.pending = m.energized = false;
    m.state.slew_target_rad_s = m.state.trolley_target_mps = 0;
    if (s.hoist_connected) {
        auto config = m.hoist_config();
        // Restored stock is bounded; actual endpoints may be slack below the
        // payout minimum or finitely extended beyond the maximum stock length.
        config.initial_length_m = s.hoist.rest_length_m;
        m.hoist = std::make_unique<PoweredCable>(m.world, config);
        m.hoist->restore(s.hoist);
    }
    for (unsigned i = 0; i < m.slings.size(); ++i) if (s.attached_mask & (1U << i)) {
        m.slings[i] = std::make_unique<PoweredCable>(m.world, m.sling_config(i));
        m.slings[i]->restore(s.slings[i]);
    }
    m.slew->SetMotorState(EMotorState::Off); m.slew->SetMaxFrictionTorque(kSlewTorque);
    m.track->SetMotorState(EMotorState::Off); m.track->SetMaxFrictionForce(kTrolleyForce);
    m.slew->ResetWarmStart(); m.track->ResetWarmStart(); m.refresh();
}
} // namespace scraperx::sim
