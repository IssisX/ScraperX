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
            // Isolate actuator momentum/work from native sleep's low-speed
            // velocity truncation, just as the fixture excludes damping.
            lock.GetBody().SetAllowSleeping(false);
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

void limits(const PhysicalFootPush::Readback &state, double previous_work, bool planted_profile = false) {
    // Walking also uses activate_motor(), with admitted event work rather
    // than the legacy spring's force-bound-times-target-extension ledger.
    const bool motor_profile = planted_profile || state.walking_step;
    require(std::isfinite(state.command_work_bound_j) &&
                std::isfinite(state.nominal_spring_storage_j) &&
                std::isfinite(state.last_impulse_ns) && finite(state.player_force),
            "native force and separate work/storage diagnostics stay finite");
    require(state.command_work_bound_j >= previous_work - 1.0e-8 &&
                state.command_work_bound_j <= 1285.625 + 1.0e-5 &&
                state.last_command_work_bound_j <= 5000.0 * kDt + 1.0e-4 &&
                (!motor_profile || (state.motor_positive_work_j == state.command_work_bound_j &&
                    state.target_storage_delta_j == 0 && state.nominal_spring_storage_j == 0)),
            "actual target advances respect total work and the selected profile power limit");
    require(state.commanded_stroke_m >= -1.0e-6F && state.commanded_stroke_m <= .3F + 1.0e-6F &&
                state.elapsed_seconds <= .25 + kDt + 1.0e-6,
            "finite physical push respects stroke and duration limits");
    require(state.last_impulse_ns >= -1.0e-6F &&
                state.last_impulse_ns <= 3500.0F * kDt + 1.0e-4F &&
                state.peak_load_n <= 3500.01F,
            "one native collision row is compression-only and force-capped");
    if (motor_profile) {
        require((planted_profile ? state.planted_motor_profile : state.walking_step) &&
                    state.commanded_stroke_m == 0 &&
                    state.rest_distance_m == state.initial_distance_m &&
                    state.motor_absorbed_work_j >= 0 && state.retired_nominal_storage_j >= 0 &&
                    std::abs(state.actual_motor_extension_m -
                        std::max(0.0F, state.actual_distance_m - state.initial_distance_m)) < 1.0e-6F,
                "motor receipts separate actual geometry, positive work, absorption and retired storage");
    } else {
        require(std::abs(state.command_work_bound_j - 3500.0 * state.commanded_stroke_m) < 2.0e-3,
                "charged command work equals force bound times actual target extension");
    }
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

void contact_owner_loss_cancels(bool traction_loss, bool hold_target = false) {
    scenario = traction_loss ? "traction_owner_loss" : "material_face_owner_loss";
    Pair pair;
    Actuator actuator(pair);
    require(actuator.push.begin(pair.kit.body_id(pair.cell), pair.foot, Vec3::sAxisY()), "loss pair attaches");
    actuator.tick();
    const auto before_cell = snapshot(pair.fixture, pair.kit.body_id(pair.cell));
    const auto before_player = snapshot(pair.fixture, pair.kit.body_id(pair.player));
    const double charged = actuator.push.readback().command_work_bound_j;
    if (hold_target) actuator.push.hold_target();
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

void held_target_retains_row_and_finite_lifetime() {
    scenario = "held_target_retains_loaded_row_and_expiry";
    Pair pair(Vec3(0, .019F, 0), Vec3(0, .9F, 0), Quat::sIdentity(), 5000);
    Actuator actuator(pair);
    const auto support = pair.kit.body_id(pair.cell);
    require(actuator.push.begin(support, pair.foot, Vec3::sAxisY()), "hold fixture attaches");
    for (int tick = 0; tick < 4; ++tick) actuator.tick();
    const auto before = actuator.push.readback();
    require(before.last_load_n > 0 && before.nominal_spring_storage_j > 0,
            "hold begins with a real loaded compression row");
    actuator.push.hold_target();
    const auto held = actuator.push.readback();
    require(actuator.push.active() && held.target_held && held.support == before.support &&
                held.rest_distance_m == before.rest_distance_m &&
                held.nominal_spring_storage_j == before.nominal_spring_storage_j &&
                held.last_impulse_ns == before.last_impulse_ns &&
                held.elapsed_seconds == before.elapsed_seconds,
            "hold preserves actual row, preload, cached impulse and original clock");
    require(!actuator.push.begin(support, pair.foot, Vec3::sAxisY()),
            "begin cannot reopen an existing held row");
    bool loaded_after_hold = false;
    for (int tick = 4; tick < 90; ++tick) {
        actuator.push.hold_target(); // Reaffirming a hold cannot renew its clock.
        actuator.tick();
        const auto &state = actuator.push.readback();
        limits(state, before.command_work_bound_j);
        require(actuator.push.active() && state.target_held &&
                    state.rest_distance_m == before.rest_distance_m &&
                    state.command_work_bound_j == before.command_work_bound_j &&
                    state.last_command_work_bound_j == 0 && state.target_storage_delta_j == 0,
                "held real solves advance time without adding target stroke or work");
        loaded_after_hold = loaded_after_hold || state.last_load_n > 0;
    }
    require(loaded_after_hold, "held row still supplies its actual compressive reaction");
    actuator.tick();
    require(!actuator.push.active() && actuator.push.readback().stop_reason == PhysicalFootPush::StopReason::Expired &&
                std::abs(actuator.push.readback().elapsed_seconds - 90.0 * kDt) < 1.0e-8,
            "hold expires on the original finite lifetime without renewal");
    const auto foot = actuator.push.readback().world_anchor;
    require(actuator.push.begin(support, foot, Vec3::sAxisY()) && !actuator.push.readback().target_held,
            "only a fresh valid attachment starts a new extension transaction");
    actuator.tick();
    require(actuator.push.readback().command_work_bound_j > 0,
            "fresh attachment can command extension after held predecessor expires");
}

void passive_plant_activation_retains_row() {
    scenario = "passive_plant_activation_retains_row";
    Pair pair(Vec3(0, .019F, 0), Vec3(0, .9F, 0), Quat::sIdentity(), 5000);
    Actuator actuator(pair);
    const auto support = pair.kit.body_id(pair.cell);
    require(!actuator.push.activate_planted_push(), "an absent row cannot activate");
    require(actuator.push.begin_passive_plant(support, pair.foot, Vec3::sAxisY()),
            "actual material point accepts a passive plant");
    const auto initial = actuator.push.readback();
    require(initial.passive_plant && initial.target_held && initial.active &&
                initial.rest_distance_m == initial.actual_distance_m &&
                initial.command_work_bound_j == 0 && initial.nominal_spring_storage_j == 0,
            "passive plant captures actual distance without artificial preload");
    for (int tick = 0; tick < 4; ++tick) {
        actuator.tick();
        const auto &state = actuator.push.readback();
        require(state.active && state.passive_plant && state.target_held &&
                    state.command_work_bound_j == 0 && state.commanded_stroke_m == 0 &&
                    state.last_impulse_ns == 0 && state.nominal_spring_storage_j == 0,
                "unloaded passive solves add neither command work nor preload impulse");
    }
    // External compression must still receive a real passive row reaction.
    pair.fixture.system.GetBodyInterface().SetLinearVelocity(pair.kit.body_id(pair.player), Vec3(0, -.1F, 0));
    actuator.tick();
    actuator.tick();
    const auto before = actuator.push.readback();
    require(before.last_impulse_ns > 0 && before.command_work_bound_j == 0,
            "external compression loads the zero-command passive row");
    require(!actuator.push.begin_passive_plant(support, pair.foot, Vec3::sAxisY()),
            "passive acquisition cannot replace an existing row");
    const auto cell_before_clutch = snapshot(pair.fixture, support);
    const auto player_before_clutch = snapshot(pair.fixture, pair.kit.body_id(pair.player));
    require(before.nominal_spring_storage_j > 0, "clutch fixture has real stored passive compression");
    require(actuator.push.activate_planted_push(), "retained passive row activates once");
    const auto activated = actuator.push.readback();
    require(activated.active && !activated.passive_plant && !activated.target_held &&
                activated.support == before.support && activated.world_anchor == before.world_anchor &&
                activated.initial_distance_m == before.initial_distance_m &&
                activated.rest_distance_m == before.rest_distance_m &&
                activated.elapsed_seconds == before.elapsed_seconds &&
                activated.command_work_bound_j == before.command_work_bound_j &&
                activated.planted_motor_profile && activated.nominal_spring_storage_j == 0 &&
                activated.retired_nominal_storage_j == before.nominal_spring_storage_j &&
                activated.motor_positive_work_j == 0 && activated.motor_absorbed_work_j == 0,
            "clutch preserves material identity, rest and clock while retiring unfunded passive storage");
    const auto cell_after_clutch = snapshot(pair.fixture, support);
    const auto player_after_clutch = snapshot(pair.fixture, pair.kit.body_id(pair.player));
    require(cell_after_clutch.center == cell_before_clutch.center &&
                player_after_clutch.center == player_before_clutch.center &&
                cell_after_clutch.velocity == cell_before_clutch.velocity &&
                player_after_clutch.velocity == player_before_clutch.velocity &&
                cell_after_clutch.angular == cell_before_clutch.angular &&
                player_after_clutch.angular == player_before_clutch.angular,
            "activation supplies neither pose nor velocity preload");
    require(!actuator.push.activate_planted_push() &&
                actuator.push.readback().elapsed_seconds == activated.elapsed_seconds &&
                actuator.push.readback().last_impulse_ns == activated.last_impulse_ns,
            "second activation is refused without changing the row");
    actuator.tick();
    const auto &powered = actuator.push.readback();
    limits(powered, before.command_work_bound_j, true);
    require(powered.last_impulse_ns > 0 && powered.command_work_bound_j > 0 &&
                powered.elapsed_seconds > before.elapsed_seconds,
            "activation produces a positive bounded real impulse on the original clock");
    std::cout << "FOOT_PASSIVE_ACTIVATION passive_impulse_ns=" << before.last_impulse_ns
              << " powered_impulse_ns=" << powered.last_impulse_ns
              << " elapsed_before_s=" << before.elapsed_seconds
              << " elapsed_after_s=" << powered.elapsed_seconds << '\n';

    actuator.push.clear();
    const auto foot = actuator.push.readback().world_anchor;
    require(actuator.push.begin_passive_plant(support, foot, Vec3::sAxisY()), "expiry fixture replants");
    for (int tick = 0; tick < 92 && actuator.push.active(); ++tick) actuator.tick();
    const auto expired = actuator.push.readback();
    require(!expired.active && expired.stop_reason == PhysicalFootPush::StopReason::Expired &&
                expired.command_work_bound_j == 0 && !actuator.push.activate_planted_push() &&
                actuator.push.readback().elapsed_seconds == expired.elapsed_seconds,
            "expired passive attachment cannot rearm or renew its lifetime");

    require(actuator.push.begin(support, actuator.push.readback().world_anchor, Vec3::sAxisY()),
            "ordinary begin still starts a powered row");
    actuator.push.hold_target();
    require(!actuator.push.readback().passive_plant && !actuator.push.activate_planted_push() &&
                actuator.push.readback().target_held,
            "ordinary held push cannot be mistaken for a passive plant");
}

void planted_reach_uses_the_same_radial_geometry_before_and_after_activation() {
    scenario = "planted_swivel_reach_and_legacy_cylinder";
    for (const bool planted : {false, true}) {
        Pair pair(Vec3(0, .019F, 0), Vec3(.4F, .9F, 0), Quat::sIdentity(), 5000);
        const auto support = pair.kit.body_id(pair.cell), player = pair.kit.body_id(pair.player);
        pair.fixture.system.GetBodyInterface().SetLinearVelocity(player, Vec3(1, 0, 0));
        Actuator actuator(pair);
        require(planted ? actuator.push.begin_passive_plant(support, pair.foot, Vec3::sAxisY()) :
                          actuator.push.begin(support, pair.foot, Vec3::sAxisY()),
                "actual finite initial reach is admitted");
        actuator.push.hold_target();
        for (unsigned step = 0; step < 40 && actuator.push.active(); ++step) actuator.tick();
        const auto held = actuator.push.readback();
        if (!planted) {
            require(!held.active && held.stop_reason == PhysicalFootPush::StopReason::LateralReachLost,
                    "legacy pushes retain their existing cylindrical reach limit");
            continue;
        }
        const auto player_state = snapshot(pair.fixture, player);
        const Vec3 leg(player_state.center - held.world_anchor);
        require(held.active && leg.GetX() > .5F && leg.GetX() < .85F * leg.GetY() &&
                    held.actual_distance_m < held.initial_distance_m + .3F && held.command_work_bound_j == 0,
                "real unpowered plant can swivel beyond the legacy cylinder within length and traction");
        require(actuator.push.activate_planted_push() &&
                    actuator.push.readback().elapsed_seconds == held.elapsed_seconds &&
                    actuator.push.readback().initial_distance_m == held.initial_distance_m,
                "activation preserves the already valid geometry, anchor and original clock");
        bool loaded = false;
        for (unsigned step = 0; step < 40 && actuator.push.active() && !loaded; ++step) {
            actuator.tick();
            loaded = actuator.push.readback().last_impulse_ns > 0;
        }
        require(loaded && actuator.push.active(),
                "same retained radial plant supplies finite motor impulse without advancing its rest target");
    }
    Pair slipping(Vec3(0, .019F, 0), Vec3(.5F, .5F, 0), Quat::sIdentity(), 5000);
    Actuator refused(slipping);
    require(!refused.push.begin_passive_plant(slipping.kit.body_id(slipping.cell), slipping.foot,
                Vec3::sAxisY(), .85F), "radial planted reach never bypasses the actual material friction cone");
}

// This fixture has no contacts, gravity or damping. The isolated impulse event
// can close kinetic energy; this does not claim continuous world/contact closure.
void planted_motor_impulse_work_and_reaction(bool crossing_zero, Quat basis = Quat::sIdentity()) {
    scenario = crossing_zero ? "planted_motor_brakes_then_pays_for_positive_work" :
                               "planted_motor_offset_spinning_support";
    constexpr float support_mass = kCellMass;
    Pair pair(Vec3(.06F, .019F, 0), Vec3(.2F, .9F, 0), basis, support_mass);
    const auto support = pair.kit.body_id(pair.cell), player = pair.kit.body_id(pair.player);
    const Vec3 n = Vec3(snapshot(pair.fixture, player).center - pair.foot).Normalized();
    pair.fixture.system.GetBodyInterface().SetAngularVelocity(support, basis * Vec3(0, 0, .7F));
    const auto initial_support = snapshot(pair.fixture, support);
    const Vec3 r(pair.foot - initial_support.center);
    const double incoming = crossing_zero ? -2.0 : 3.0;
    const Vec3 point_velocity = initial_support.velocity + initial_support.angular.Cross(r);
    pair.fixture.system.GetBodyInterface().SetLinearVelocity(player, point_velocity + float(incoming) * n);
    Actuator actuator(pair);
    require(actuator.push.begin_passive_plant(support, pair.foot, basis * Vec3::sAxisY()) &&
                actuator.push.activate_planted_push(), "isolated real two-body motor activates");
    const auto before_support = snapshot(pair.fixture, support), before_player = snapshot(pair.fixture, player);
    const double u = n.Dot(before_player.velocity - before_support.velocity - before_support.angular.Cross(r));
    const Vec3 rxn = r.Cross(n);
    const double w = 1.0 / kPlayerMass + 1.0 / support_mass +
        rxn.Dot(before_support.inertia.Inversed3x3() * rxn);
    actuator.tick();
    const auto after_support = snapshot(pair.fixture, support), after_player = snapshot(pair.fixture, player);
    const auto state = actuator.push.readback();
    limits(state, 0, true);
    const Vec3 impulse = kPlayerMass * (after_player.velocity - before_player.velocity);
    const Vec3 reaction = support_mass * (after_support.velocity - before_support.velocity);
    const double j = impulse.Dot(n);
    const Vec3 expected_spin = before_support.inertia.Inversed3x3() * r.Cross(-impulse);
    require(j > 0 && impulse.Cross(n).Length() < 3.0e-4F &&
                (impulse + reaction).Length() < 3.0e-4F &&
                (after_support.angular - before_support.angular - expected_spin).Length() < .003F &&
                after_player.angular.Length() < 1.0e-5F &&
                std::abs(state.last_impulse_ns - j) < 3.0e-4,
            "motor has one reciprocal impulse and the actual offset-support angular reaction");
    const double final_speed = u + w * j;
    const double positive = (std::pow(std::max(final_speed, 0.0), 2) -
                             std::pow(std::max(u, 0.0), 2)) / (2 * w);
    const double absorbed = (std::pow(std::min(u, 0.0), 2) -
                             std::pow(std::min(final_speed, 0.0), 2)) / (2 * w);
    const double kinetic_delta = after_player.energy + after_support.energy -
                                 before_player.energy - before_support.energy;
    require(std::abs(kinetic_delta - (positive - absorbed)) < .008 &&
                std::abs(state.motor_positive_work_j - positive) < .003 &&
                std::abs(state.motor_absorbed_work_j - absorbed) < .003 &&
                std::abs(state.last_command_work_bound_j - positive) < .003,
            "real linear plus rotational kinetic change equals paid positive work minus uncredited absorption");
    require(positive <= 5000.0 * kDt + .003 && j <= 3500.0 * kDt + .0003 &&
                positive > 5000.0 * kDt - .01,
            "power-limited motor uses available positive work without violating force cap");
    if (crossing_zero) require(u < 0 && final_speed > 0 && absorbed > .1 &&
                state.command_work_bound_j > kinetic_delta + .1,
            "crossing zero charges all positive work instead of refunding absorbed incoming energy");
    std::cout << "FOOT_MOTOR_EVENT scenario=" << scenario << " u_before_mps=" << u
              << " u_after_mps=" << final_speed << " impulse_ns=" << j
              << " kinetic_delta_j=" << kinetic_delta << " positive_j=" << positive
              << " absorbed_j=" << absorbed << " receipt_positive_j=" << state.motor_positive_work_j
              << " receipt_absorbed_j=" << state.motor_absorbed_work_j << '\n';
}

void planted_motor_solver_iterations_and_substeps() {
    scenario = "planted_motor_40_iterations_four_collision_steps";
    Pair pair(Vec3(0, .019F, 0), Vec3(0, .9F, 0), Quat::sIdentity(), 5000);
    auto settings = pair.fixture.system.GetPhysicsSettings();
    settings.mNumVelocitySteps = 40;
    pair.fixture.system.SetPhysicsSettings(settings);
    Actuator actuator(pair);
    const auto player = pair.kit.body_id(pair.player), support = pair.kit.body_id(pair.cell);
    require(actuator.push.begin_passive_plant(support, pair.foot, Vec3::sAxisY()) &&
                actuator.push.activate_planted_push(), "iteration fixture activates");
    const auto before_player = snapshot(pair.fixture, player), before_support = snapshot(pair.fixture, support);
    require(pair.fixture.system.Update(4 * kDt, 4, &pair.fixture.allocator, &pair.fixture.jobs) ==
                EPhysicsUpdateError::None, "four production collision steps solve without capacity errors");
    actuator.push.post_step(kDt);
    const auto after_player = snapshot(pair.fixture, player), after_support = snapshot(pair.fixture, support);
    const auto state = actuator.push.readback();
    limits(state, 0, true);
    const double total_impulse = kPlayerMass * (after_player.velocity.GetY() - before_player.velocity.GetY());
    require(std::abs(total_impulse - 4 * 3500.0 * kDt) < .002 &&
                std::abs(state.last_impulse_ns - 3500.0 * kDt) < .0002 &&
                std::abs(state.elapsed_seconds - 4.0 * kDt) < 1.0e-8,
            "forty solver iterations apply exactly one capped impulse per collision step");
    const double kinetic_delta = after_player.energy + after_support.energy -
                                 before_player.energy - before_support.energy;
    require(state.command_work_bound_j <= 4 * 5000.0 * kDt + .0001 &&
                std::abs(state.motor_positive_work_j - kinetic_delta) < .003 &&
                state.motor_absorbed_work_j == 0 && state.last_command_work_bound_j > 0,
            "substep command work debits once per actual impulse and closes isolated kinetic energy");
    std::cout << "FOOT_MOTOR_SUBSTEPS total_impulse_ns=" << total_impulse
              << " last_impulse_ns=" << state.last_impulse_ns
              << " kinetic_j=" << kinetic_delta << " positive_work_j=" << state.motor_positive_work_j << '\n';
}

void planted_motor_geometric_limits_stop_real_drive(bool cone) {
    scenario = cone ? "planted_motor_finite_cone" : "planted_motor_finite_radial_extent";
    Pair pair(Vec3(0, .019F, 0), cone ? Vec3(.6F, .9F, 0) : Vec3(0, .9F, 0),
              Quat::sIdentity(), 5000);
    const auto support = pair.kit.body_id(pair.cell), player = pair.kit.body_id(pair.player);
    pair.fixture.system.GetBodyInterface().SetLinearVelocity(player, cone ? Vec3(10, 0, 0) : Vec3(0, 5, 0));
    Actuator actuator(pair);
    require(actuator.push.begin_passive_plant(support, pair.foot, Vec3::sAxisY()) &&
                actuator.push.activate_planted_push(), "finite geometry motor activates");
    double work = 0;
    for (unsigned step = 0; step < 50 && actuator.push.active(); ++step) {
        actuator.tick();
        const auto state = actuator.push.readback();
        limits(state, work, true);
        work = state.command_work_bound_j;
    }
    const auto stopped = actuator.push.readback();
    require(!stopped.active && stopped.stop_reason == (cone ? PhysicalFootPush::StopReason::TractionLost :
                                                            PhysicalFootPush::StopReason::VerticalReachLost),
            "actual motor ends when original radial reach or material cone is lost");
    const auto player_state = snapshot(pair.fixture, player), support_state = snapshot(pair.fixture, support);
    const Vec3 leg(player_state.center - stopped.world_anchor);
    require(cone ? std::abs(leg.GetX()) > .85F * leg.GetY() :
                   stopped.actual_distance_m > stopped.initial_distance_m + .3F,
            "geometric termination is supported by actual endpoint positions");
    actuator.tick();
    require(actuator.push.readback().command_work_bound_j == work &&
                actuator.push.readback().last_impulse_ns == 0 &&
                (snapshot(pair.fixture, player).velocity - player_state.velocity).Length() < 1.0e-6F &&
                (snapshot(pair.fixture, support).velocity - support_state.velocity).Length() < 1.0e-6F &&
                !actuator.push.activate_planted_push(),
            "finite geometric stop preserves departure momentum and refuses reactivation");
}

void planted_motor_hold_stops_drive_on_original_clock() {
    scenario = "planted_motor_hold_has_no_spring_or_clock_renewal";
    Pair pair(Vec3(0, .019F, 0), Vec3(0, .9F, 0), Quat::sIdentity(), 5000);
    Actuator actuator(pair);
    const auto support = pair.kit.body_id(pair.cell), player = pair.kit.body_id(pair.player);
    require(actuator.push.begin_passive_plant(support, pair.foot, Vec3::sAxisY()), "hold plant attaches");
    for (int tick = 0; tick < 4; ++tick) actuator.tick();
    require(actuator.push.activate_planted_push(), "hold plant activates after elapsed passive time");
    actuator.tick();
    const auto powered = actuator.push.readback();
    require(powered.last_impulse_ns > 0 && powered.command_work_bound_j > 0, "motor drives before hold");
    actuator.push.hold_target();
    // A new external fixture compression discriminates a resurrected passive
    // spring from a stopped motor; it does not represent commanded muscle work.
    pair.fixture.system.GetBodyInterface().SetLinearVelocity(player, Vec3(0, -.1F, 0));
    const auto before_player = snapshot(pair.fixture, player), before_support = snapshot(pair.fixture, support);
    for (int tick = 5; tick < 90; ++tick) {
        actuator.push.hold_target();
        actuator.tick();
        const auto state = actuator.push.readback();
        limits(state, powered.command_work_bound_j, true);
        require(state.active && state.target_held && state.last_impulse_ns == 0 &&
                    state.command_work_bound_j == powered.command_work_bound_j &&
                    state.last_command_work_bound_j == 0 && state.nominal_spring_storage_j == 0 &&
                    !actuator.push.activate_planted_push(),
                "held motor remains attached without warm start, new work or resurrected spring");
    }
    require((snapshot(pair.fixture, player).velocity - before_player.velocity).Length() < 1.0e-6F &&
                (snapshot(pair.fixture, support).velocity - before_support.velocity).Length() < 1.0e-6F,
            "externally compressing held motor produces no hidden velocity change");
    actuator.tick();
    const auto expired = actuator.push.readback();
    require(!expired.active && expired.stop_reason == PhysicalFootPush::StopReason::Expired &&
                std::abs(expired.elapsed_seconds - 90.0 * kDt) < 1.0e-8 &&
                expired.command_work_bound_j == powered.command_work_bound_j &&
                !actuator.push.activate_planted_push(),
            "held motor expires on original passive-attachment clock and cannot rearm");
}

void blocked_walking_commit_preserves_existing_row() {
    scenario = "blocked_walking_commit_preserves_existing_row";
    Pair pair;
    pair.fixture.system.SetGravity(Vec3(0, -9.81F, 0));
    Actuator actuator(pair);
    require(!actuator.push.commit_blocked_walking_step(),
            "blocked lift cannot create an attachment");
    require(actuator.push.begin_walking_step(pair.kit.body_id(pair.cell), pair.foot,
                Vec3::sAxisY(), .7F, .25F, Vec3::sAxisX(), .35F),
            "walking stroke starts on its supplied real material point");
    const auto before = actuator.push.readback();
    require(actuator.push.commit_blocked_walking_step(),
            "a newly begun walking stroke can commit its requested lift");
    const auto committed = actuator.push.readback();
    require(committed.support == before.support &&
                committed.initial_distance_m == before.initial_distance_m &&
                committed.stroke_limit_m == before.stroke_limit_m &&
                committed.elapsed_seconds == before.elapsed_seconds &&
                committed.command_work_bound_j == before.command_work_bound_j,
            "commit retains support, reach, age and work ledger");
    actuator.tick();
    const auto solved = actuator.push.readback();
    require(solved.last_load_n > 0 && solved.command_work_bound_j > 0,
            "committed lift supplies a real finite reciprocal solve without forward travel");
    limits(solved, 0);
    require(!actuator.push.commit_blocked_walking_step(),
            "an aged stroke cannot restart its blocked lift");
    require(actuator.push.readback().elapsed_seconds == solved.elapsed_seconds &&
                actuator.push.readback().command_work_bound_j == solved.command_work_bound_j,
            "refused recommit preserves the actual clock and spent work");
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
    contact_owner_loss_cancels(false, true);
    contact_owner_loss_cancels(true, true);
    held_target_retains_row_and_finite_lifetime();
    passive_plant_activation_retains_row();
    planted_reach_uses_the_same_radial_geometry_before_and_after_activation();
    planted_motor_impulse_work_and_reaction(false);
    planted_motor_impulse_work_and_reaction(false, Quat::sRotation(Vec3::sAxisY(), .47F));
    planted_motor_impulse_work_and_reaction(true);
    planted_motor_solver_iterations_and_substeps();
    planted_motor_geometric_limits_stop_real_drive(false);
    planted_motor_geometric_limits_stop_real_drive(true);
    planted_motor_hold_stops_drive_on_original_clock();
    blocked_walking_commit_preserves_existing_row();
    finite_actuation_and_energy();
    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;
    std::cout << "PASS PHYSICAL_FOOT_PUSH isolated=1 player_contact_integration=unverified\n";
    return EXIT_SUCCESS;
}
