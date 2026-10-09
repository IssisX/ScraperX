#include "sim/physical_foot_push.hpp"
#include "sim/mechanism_kit.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>

namespace {
using namespace JPH;
using scraperx::sim::kit::Kit;
using scraperx::sim::kit::Part;
using scraperx::sim::PhysicalFootPush;
constexpr float kDt = 1.0F / 360.0F; // One production collision substep.
constexpr float kCellMass = .722F;
constexpr float kPlayerMass = 85.0F;
const char *scenario = "initialization";

void require(bool condition, const char *message) {
    if (condition) return;
    std::cerr << "FAIL PHYSICAL_FOOT_PUSH scenario=" << scenario << " reason=" << message << '\n';
    std::exit(EXIT_FAILURE);
}
bool finite(Vec3 value) {
    return std::isfinite(value.GetX()) && std::isfinite(value.GetY()) && std::isfinite(value.GetZ());
}
struct BroadPhase final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override { return BroadPhaseLayer(layer); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "foot_push"; }
};
struct BroadPhaseFilter final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override { return a != 0 || b.GetValue() != 0; }
};
struct LayerFilter final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override { return a != 0 || b != 0; }
};
struct Fixture final {
    BroadPhase broad_phase;
    BroadPhaseFilter broad_phase_filter;
    LayerFilter layer_filter;
    PhysicsSystem system;
    TempAllocatorImpl allocator{8 * 1024 * 1024};
    JobSystemSingleThreaded jobs{1024};
    Fixture() {
        system.Init(32, 0, 64, 128, broad_phase, broad_phase_filter, layer_filter);
        system.SetGravity(Vec3::sZero());
    }
    void tick() {
        require(system.Update(kDt, 1, &allocator, &jobs) == EPhysicsUpdateError::None,
                "isolated native substep has no capacity or simulation errors");
    }
};
struct Snapshot final {
    RVec3 center;
    Vec3 velocity, angular;
    Mat44 inertia;
    double energy;
};
Snapshot snapshot(Fixture &fixture, BodyID id) {
    BodyLockRead lock(fixture.system.GetBodyLockInterface(), id);
    require(lock.Succeeded(), "real native diagnostic body exists");
    const auto &body = lock.GetBody();
    const auto *motion = body.GetMotionProperties();
    const auto rotation = Mat44::sRotation(body.GetRotation());
    const auto inertia = rotation * motion->GetLocalSpaceInverseInertia().Inversed3x3() * rotation.Transposed3x3();
    const auto velocity = body.GetLinearVelocity(), angular = body.GetAngularVelocity();
    require(finite(Vec3(body.GetCenterOfMassPosition())) && finite(velocity) && finite(angular),
            "actual diagnostic state remains finite");
    return {body.GetCenterOfMassPosition(), velocity, angular, inertia,
            .5 * velocity.LengthSq() / motion->GetInverseMass() + .5 * angular.Dot(inertia * angular)};
}
struct Pair final {
    Fixture fixture;
    Kit kit{fixture.system, 0, 1};
    scraperx::sim::kit::BodyIndex cell, player;
    RVec3 foot;
    Pair(Vec3 foot_local = Vec3(0, .019F, 0), Vec3 leg = Vec3(0, .9F, 0),
         Quat basis = Quat::sIdentity(), float support_mass = kCellMass) {
        Part timber;
        timber.half = Vec3(.1F, .019F, .095F);
        timber.mass_kg = support_mass;
        cell = kit.add_body(2880, {timber}, RVec3::sZero(), basis, support_mass, .7F);
        foot = RVec3(basis * foot_local);
        Part capsule_proxy;
        capsule_proxy.half = Vec3(.35F, .9F, .35F);
        player = kit.add_body(3000, {capsule_proxy}, foot + RVec3(basis * leg), basis, kPlayerMass, .7F);
        // This isolates the production actuator's impulse path, not contact or
        // player integration. Actual-world contact is a separate acceptance gate.
        kit.disable_collision(cell, player);
        kit.set_damping(cell, 0, 0);
        kit.set_damping(player, 0, 0);
        for (const auto index : {cell, player}) {
            BodyLockWrite lock(fixture.system.GetBodyLockInterface(), kit.body_id(index));
            lock.GetBody().GetMotionProperties()->SetMaxAngularVelocity(500.0F);
        }
    }
};

struct PushListener final : PhysicsStepListener {
    PhysicalFootPush &push;
    bool face_valid = true, traction_valid = true;
    explicit PushListener(PhysicalFootPush &value) : push(value) {}
    void OnStep(const PhysicsStepListenerContext &context) override {
        push.pre_step(context.mDeltaTime, face_valid, traction_valid);
    }
};
struct Actuator final {
    Pair &pair;
    PhysicalFootPush push;
    PushListener listener;
    explicit Actuator(Pair &value)
        : pair(value), push(pair.fixture.system, pair.kit.body_id(pair.player)), listener(push) {
        pair.fixture.system.AddStepListener(&listener);
    }
    ~Actuator() { pair.fixture.system.RemoveStepListener(&listener); }
    void tick() {
        pair.fixture.tick();
        push.post_step(kDt); // Manager removal always outside Update.
    }
};

void limits(const PhysicalFootPush::Readback &state, double previous_work) {
    require(std::isfinite(state.command_work_bound_j) &&
                std::isfinite(state.nominal_spring_storage_j) &&
                std::isfinite(state.last_impulse_ns) && finite(state.player_force),
            "native force and separate work/storage diagnostics stay finite");
    require(state.command_work_bound_j >= previous_work - 1.0e-8 &&
                state.command_work_bound_j <= 1285.625 + 1.0e-5 &&
                state.last_command_work_bound_j <= 5000.0 * kDt + 1.0e-4,
            "actual representable target advances respect command budget and power");
    require(state.commanded_stroke_m >= -1.0e-6F && state.commanded_stroke_m <= .3F + 1.0e-6F &&
                state.elapsed_seconds <= .25 + kDt + 1.0e-6,
            "finite physical push respects stroke and duration limits");
    require(state.last_impulse_ns >= -1.0e-6F &&
                state.last_impulse_ns <= 3500.0F * kDt + 1.0e-4F &&
                state.peak_load_n <= 3500.01F,
            "one native collision row is compression-only and force-capped");
    require(std::abs(state.command_work_bound_j - 3500.0 * state.commanded_stroke_m) < 2.0e-3,
            "charged command work equals force bound times actual target extension");
}

void zero_preload_and_line_impulse(bool offset, Quat basis = Quat::sIdentity()) {
    scenario = offset ? "offset_material_anchor_torque" : "centered_reciprocal_impulse";
    Pair pair(offset ? Vec3(.06F, .019F, 0) : Vec3(0, .019F, 0),
              offset ? Vec3(.2F, .9F, 0) : Vec3(0, .9F, 0), basis);
    Actuator actuator(pair);
    require(actuator.push.begin(pair.kit.body_id(pair.cell), pair.foot, basis * Vec3::sAxisY()),
            "production helper attaches the actual material anchor to player COM");
    const auto before_cell = snapshot(pair.fixture, pair.kit.body_id(pair.cell));
    const auto before_player = snapshot(pair.fixture, pair.kit.body_id(pair.player));
    const auto &initial = actuator.push.readback();
    require(std::abs(initial.actual_distance_m - initial.rest_distance_m) < 1.0e-6F &&
                initial.command_work_bound_j == 0 && initial.nominal_spring_storage_j < 1.0e-8,
            "attach creates no preload or uncharged spring energy");
    // With no target advancement, the neutral row itself must not supply an
    // impulse. Temporarily omit the command listener, not the actual constraint.
    pair.fixture.system.RemoveStepListener(&actuator.listener);
    pair.fixture.tick();
    actuator.push.post_step(kDt);
    require(snapshot(pair.fixture, pair.kit.body_id(pair.cell)).energy < 1.0e-10 &&
                snapshot(pair.fixture, pair.kit.body_id(pair.player)).energy < 1.0e-10,
            "an actual neutral solve adds no hidden preload impulse");
    pair.fixture.system.AddStepListener(&actuator.listener);
    actuator.tick();
    // The registered listener now advanced one real command; inspect its first
    // solved response against independently derived linear/angular impulse.
    const auto after_cell = snapshot(pair.fixture, pair.kit.body_id(pair.cell));
    const auto after_player = snapshot(pair.fixture, pair.kit.body_id(pair.player));
    const Vec3 impulse = kPlayerMass * (after_player.velocity - before_player.velocity);
    const Vec3 reaction = kCellMass * (after_cell.velocity - before_cell.velocity);
    const auto initial_axis = Vec3(before_player.center - pair.foot).Normalized();
    require(impulse.Length() > 1.0e-5F && impulse.Dot(initial_axis) > 0,
            "real command produces a compressive outward player impulse");
    require((impulse + reaction).Length() < 2.0e-4F + 2.0e-4F * impulse.Length() &&
                impulse.Cross(initial_axis).Length() < 2.0e-4F,
            "native bodies receive equal/opposite impulses along their actual leg line");
    const auto torque_impulse = Vec3(pair.foot - before_cell.center).Cross(-impulse);
    const auto expected_angular = before_cell.inertia.Inversed3x3() * torque_impulse;
    require((after_cell.angular - before_cell.angular - expected_angular).Length() <
                2.0e-3F + .002F * expected_angular.Length(),
            "support torque acts at material point, excluding spurious normal-row r1+u torque");
    require(after_player.angular.Length() < 1.0e-5F,
            "player COM endpoint receives no invented angular impulse");
    const auto &state = actuator.push.readback();
    limits(state, 0);
    require(after_cell.energy + after_player.energy <= state.command_work_bound_j * 1.02 + .1,
            "first impulse does not exceed charged command energy");
    std::cout << "FOOT_PUSH_IMPULSE scenario=" << scenario << " impulse_ns=" << impulse.Length()
              << " reaction_residual_ns=" << (impulse + reaction).Length()
              << " angular_error_radps=" << (after_cell.angular - expected_angular).Length()
              << " command_bound_j=" << state.command_work_bound_j
              << " kinetic_j=" << after_cell.energy + after_player.energy
              << " nominal_storage_j=" << state.nominal_spring_storage_j << '\n';
}

void tensile_motion_has_no_pull() {
    scenario = "separating_support_no_tension";
    Pair pair;
    // Initial fixture state, before attachment: support recedes far faster
    // than the finite rest-length command can extend. No later velocity write.
    pair.fixture.system.GetBodyInterface().SetLinearVelocity(pair.kit.body_id(pair.cell), Vec3(0, -10, 0));
    Actuator actuator(pair);
    require(actuator.push.begin(pair.kit.body_id(pair.cell), pair.foot, Vec3::sAxisY()), "separating pair attaches");
    const auto before_cell = snapshot(pair.fixture, pair.kit.body_id(pair.cell));
    const auto before_player = snapshot(pair.fixture, pair.kit.body_id(pair.player));
    actuator.tick();
    const auto after_cell = snapshot(pair.fixture, pair.kit.body_id(pair.cell));
    const auto after_player = snapshot(pair.fixture, pair.kit.body_id(pair.player));
    require((after_cell.velocity - before_cell.velocity).Length() < 1.0e-5F &&
                (after_player.velocity - before_player.velocity).Length() < 1.0e-5F &&
                actuator.push.readback().last_impulse_ns <= 1.0e-6F,
            "separating material cannot pull the player or brake support through a tensile leg");
}

void externally_loaded_row_hits_force_cap() {
    scenario = "external_closing_energy_saturates_cap";
    Pair pair(Vec3(.06F, .019F, 0), Vec3(.2F, .9F, 0));
    const auto player_id = pair.kit.body_id(pair.player), cell_id = pair.kit.body_id(pair.cell);
    const Vec3 axis = Vec3(snapshot(pair.fixture, player_id).center - pair.foot).Normalized();
    // Isolated fixture initialization BEFORE begin. This preexisting kinetic
    // energy is explicit external loading, never credited as muscle work.
    pair.fixture.system.GetBodyInterface().SetLinearVelocity(player_id, -50.0F * axis);
    const auto before_player = snapshot(pair.fixture, player_id), before_cell = snapshot(pair.fixture, cell_id);
    Actuator actuator(pair);
    require(actuator.push.begin(cell_id, pair.foot, Vec3::sAxisY()), "overload pair attaches");
    actuator.tick();
    const auto after_player = snapshot(pair.fixture, player_id), after_cell = snapshot(pair.fixture, cell_id);
    const auto &state = actuator.push.readback();
    const Vec3 impulse = kPlayerMass * (after_player.velocity - before_player.velocity);
    const Vec3 reaction = kCellMass * (after_cell.velocity - before_cell.velocity);
    const float cap = 3500.0F * state.last_collision_dt;
    require(std::abs(state.last_collision_dt - kDt) < 1.0e-8F &&
                std::abs(state.last_impulse_ns - cap) < 1.0e-4F &&
                std::abs(impulse.Dot(axis) - cap) < 1.0e-3F &&
                impulse.Length() <= cap + 1.0e-3F,
            "actual externally loaded row saturates and never exceeds3500N times collision dt");
    const Vec3 expected_angular = before_cell.inertia.Inversed3x3() *
        Vec3(pair.foot - before_cell.center).Cross(reaction);
    require((impulse + reaction).Length() < 1.0e-3F &&
                (after_cell.angular - before_cell.angular - expected_angular).Length() < .02F &&
                after_player.angular.Length() < 1.0e-5F,
            "saturated row retains reciprocal momentum and material-point torque");
    limits(state, 0);
    std::cout << "FOOT_PUSH_CAP cap_ns=" << cap << " actual_lambda_ns=" << state.last_impulse_ns
              << " measured_player_impulse_ns=" << impulse.Length()
              << " external_initial_kinetic_j=" << before_player.energy + before_cell.energy
              << " actual_final_kinetic_j=" << after_player.energy + after_cell.energy
              << " command_bound_j=" << state.command_work_bound_j << '\n';
}

void contact_owner_loss_cancels(bool traction_loss) {
    scenario = traction_loss ? "traction_owner_loss" : "material_face_owner_loss";
    Pair pair;
    Actuator actuator(pair);
    require(actuator.push.begin(pair.kit.body_id(pair.cell), pair.foot, Vec3::sAxisY()), "loss pair attaches");
    actuator.tick();
    const auto before_cell = snapshot(pair.fixture, pair.kit.body_id(pair.cell));
    const auto before_player = snapshot(pair.fixture, pair.kit.body_id(pair.player));
    const double charged = actuator.push.readback().command_work_bound_j;
    actuator.listener.face_valid = traction_loss;
    actuator.listener.traction_valid = !traction_loss;
    actuator.tick();
    const auto after_cell = snapshot(pair.fixture, pair.kit.body_id(pair.cell));
    const auto after_player = snapshot(pair.fixture, pair.kit.body_id(pair.player));
    require(!actuator.push.active() &&
                actuator.push.readback().stop_reason == (traction_loss ? PhysicalFootPush::StopReason::TractionLost :
                                                                         PhysicalFootPush::StopReason::MaterialFaceLost),
            "authoritative contact loss disables row and removes it outside Update");
    require((after_cell.velocity - before_cell.velocity).Length() < 1.0e-5F &&
                (after_player.velocity - before_player.velocity).Length() < 1.0e-5F &&
                actuator.push.readback().command_work_bound_j == charged,
            "lost support receives neither warmed-start boost nor new actuation work");
}

void finite_actuation_and_energy() {
    scenario = "finite_stroke_power_time_energy";
    Pair pair(Vec3(0, .019F, 0), Vec3(0, .9F, 0), Quat::sIdentity(), 5000);
    Actuator actuator(pair);
    require(actuator.push.begin(pair.kit.body_id(pair.cell), pair.foot, Vec3::sAxisY()), "loaded pair attaches");
    double work = 0, peak_kinetic = 0;
    bool stopped = false;
    Vec3 stopped_cell_velocity = Vec3::sZero(), stopped_player_velocity = Vec3::sZero();
    for (int i = 0; i < 180; ++i) {
        actuator.tick();
        const auto &state = actuator.push.readback();
        limits(state, work);
        const auto cell = snapshot(pair.fixture, pair.kit.body_id(pair.cell));
        const auto player = snapshot(pair.fixture, pair.kit.body_id(pair.player));
        if (stopped) {
            require(!actuator.push.active() && state.command_work_bound_j == work &&
                        state.last_impulse_ns == 0 && state.player_force.LengthSq() == 0 &&
                        (cell.velocity - stopped_cell_velocity).Length() < 1.0e-5F &&
                        (player.velocity - stopped_player_velocity).Length() < 1.0e-5F,
                    "after finite termination no further work or native boost is delivered");
        } else if (!actuator.push.active()) {
            stopped = true;
            stopped_cell_velocity = cell.velocity;
            stopped_player_velocity = player.velocity;
            std::cout << "FOOT_PUSH_STOP stop=" << int(state.stop_reason)
                      << " active=" << state.active << " elapsed_s=" << state.elapsed_seconds
                      << " actual_distance_m=" << state.actual_distance_m
                      << " initial_distance_m=" << state.initial_distance_m
                      << " stroke_m=" << state.commanded_stroke_m
                      << " command_bound_j=" << state.command_work_bound_j << '\n';
            if (state.stop_reason == PhysicalFootPush::StopReason::VerticalReachLost)
                require(state.actual_distance_m > state.initial_distance_m + .3F + 1.0e-4F,
                        "vertical-reach termination is justified by measured finite leg separation");
        }
        work = state.command_work_bound_j;
        const double kinetic = cell.energy + player.energy;
        peak_kinetic = std::max(peak_kinetic, kinetic);
        require(kinetic <= work * 1.02 + .1, "finite actuator cannot manufacture uncharged kinetic energy");
    }
    const auto &state = actuator.push.readback();
    require(!actuator.push.active() && state.command_work_bound_j > 1 &&
                (state.stop_reason == PhysicalFootPush::StopReason::StrokeExhausted ||
                 state.stop_reason == PhysicalFootPush::StopReason::Expired ||
                 state.stop_reason == PhysicalFootPush::StopReason::CommandBudgetExhausted ||
                 state.stop_reason == PhysicalFootPush::StopReason::VerticalReachLost),
            "bounded command terminates without guaranteed launch velocity");
    std::cout << "FOOT_PUSH_ENERGY command_bound_j=" << work << " peak_kinetic_j=" << peak_kinetic
              << " nominal_storage_j=" << state.nominal_spring_storage_j
              << " stroke_m=" << state.commanded_stroke_m << " elapsed_s=" << state.elapsed_seconds
              << " peak_force_n=" << state.peak_load_n << " stop=" << int(state.stop_reason)
              << " full_energy_closure=unverified\n";
}
} // namespace

int main() {
    std::cout << std::fixed << std::setprecision(9);
    RegisterDefaultAllocator();
    Factory::sInstance = new Factory;
    RegisterTypes();
    zero_preload_and_line_impulse(false);
    zero_preload_and_line_impulse(true);
    zero_preload_and_line_impulse(true, Quat::sRotation(Vec3::sAxisY(), .47F));
    tensile_motion_has_no_pull();
    externally_loaded_row_hits_force_cap();
    contact_owner_loss_cancels(false);
    contact_owner_loss_cancels(true);
    finite_actuation_and_energy();
    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;
    std::cout << "PASS PHYSICAL_FOOT_PUSH isolated=1 player_contact_integration=unverified\n";
    return EXIT_SUCCESS;
}
