#include "sim/slingshot.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace JPH;
using scraperx::sim::Slingshot;
namespace kit = scraperx::sim::kit;

namespace {
struct BP final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override { return BroadPhaseLayer(layer); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "slingshot"; }
};
struct BV final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override { return a != 0 || b.GetValue() != 0; }
};
struct LP final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override { return a != 0 || b != 0; }
};
void require(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
bool same_group(const CollisionGroup &a, const CollisionGroup &b) {
    return a.GetGroupFilter() == b.GetGroupFilter() && a.GetGroupID() == b.GetGroupID() &&
           a.GetSubGroupID() == b.GetSubGroupID();
}

struct World final {
    struct Contacts final : ContactListener {
        Slingshot *machine = nullptr;
        int pouch_contacts = 0;
        int carrier_contacts = 0;
        int rider_contacts = 0;
        void OnContactAdded(const Body &a, const Body &b, const ContactManifold &, ContactSettings &) override { observe(a,b); }
        void OnContactPersisted(const Body &a, const Body &b, const ContactManifold &, ContactSettings &) override { observe(a,b); }
        void observe(const Body &a, const Body &b) {
            if (machine && (a.GetUserData() == Slingshot::kPouchEntity || b.GetUserData() == Slingshot::kPouchEntity)) {
                ++pouch_contacts;
                machine->note_pouch_contact();
            }
            if (machine && (a.GetUserData() == Slingshot::kLaunchRailEntity || b.GetUserData() == Slingshot::kLaunchRailEntity)) {
                ++carrier_contacts;
                machine->note_carrier_contact();
            }
            if (machine && (a.GetUserData() == 2 || b.GetUserData() == 2)) {
                ++rider_contacts;
                machine->note_external_influence();
            }
        }
    } contacts;
    BP bp;
    BV bv;
    LP lp;
    PhysicsSystem physics;
    TempAllocatorImpl temporary{8 * 1024 * 1024};
    JobSystemSingleThreaded jobs{1024};
    BodyID player, floor;
    std::unique_ptr<kit::Kit> parts;
    std::unique_ptr<Slingshot> machine;
    int hz;
    double maximum_source_power = 0;

    explicit World(int frequency) : hz(frequency) {
        physics.Init(256, 0, 512, 1024, bp, bv, lp);
        physics.SetGravity(Vec3(0, -9.81F, 0));
        physics.SetContactListener(&contacts);
        BodyCreationSettings floor_settings(new BoxShape(Vec3(500, .1F, 500)),
            RVec3(6, -.1, -55), Quat::sIdentity(), EMotionType::Static, 0);
        floor = physics.GetBodyInterface().CreateAndAddBody(floor_settings, EActivation::DontActivate);
        BodyCreationSettings player_settings(new CapsuleShape(.55F, .35F),
            Slingshot::neutral_position() + RVec3(0, .73, 0), Quat::sIdentity(), EMotionType::Dynamic, 1);
        player_settings.mUserData = 2;
        player_settings.mAllowSleeping = false;
        player_settings.mLinearDamping = 0;
        player_settings.mAngularDamping = 0;
        player_settings.mFriction = 0;
        player_settings.mAllowedDOFs = EAllowedDOFs::TranslationX |
            EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ;
        player_settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        player_settings.mMassPropertiesOverride.mMass = 85;
        player = physics.GetBodyInterface().CreateAndAddBody(player_settings, EActivation::Activate);
        parts = std::make_unique<kit::Kit>(physics, 0, 1);
        machine = std::make_unique<Slingshot>(physics, *parts, player);
        contacts.machine = machine.get();
    }
    ~World() {
        machine.reset();
        contacts.machine = nullptr;
        parts.reset();
        auto &bodies = physics.GetBodyInterface();
        bodies.RemoveBody(player);
        bodies.DestroyBody(player);
        bodies.RemoveBody(floor);
        bodies.DestroyBody(floor);
    }
    void tick(double draw = 0, bool action = false, bool drop = false,
              double yaw = 0, double elevation = Slingshot::State{}.elevation_rad) {
        const float dt = 1.0F / hz;
        machine->pre_step(dt, draw, yaw, elevation, action, drop);
        parts->pre_step(dt);
        physics.Update(dt, 4, &temporary, &jobs);
        parts->post_step(dt);
        machine->post_step(dt);
        maximum_source_power = std::max(maximum_source_power, machine->state().source_power_w);
    }
};

struct Shot final {
    double draw = 0, energy = 0, work = 0, apex = 0, speed = 0;
    double maximum_residual = 0, maximum_power = 0;
    double predicted_apex = 0;
    RVec3 detach_position = RVec3::sZero();
    Vec3 detach_velocity = Vec3::sZero();
};

Shot shoot(int hz, double target_draw) {
    World world(hz);
    auto &bodies = world.physics.GetBodyInterface();
    const auto original_group = bodies.GetCollisionGroup(world.player);
    const auto initial_position = bodies.GetPosition(world.player);
    world.machine->pre_step(1.0F / hz, 0, 0, Slingshot::State{}.elevation_rad, true, false);
    require(world.machine->controls_player(), "player standing in pouch did not attach");
    require((bodies.GetPosition(world.player) - initial_position).Length() == 0,
            "boarding teleported the player");
    const auto harness_group = bodies.GetCollisionGroup(world.player);
    for (std::uint32_t i = 0; i < world.parts->body_count(); ++i) {
        const kit::BodyIndex index{i};
        require(harness_group.CanCollide(bodies.GetCollisionGroup(world.parts->body_id(index))) ==
                    (index != world.machine->pouch_body()),
                "harness collision exclusion extends beyond the leather pouch pair");
    }
    require(harness_group.CanCollide(bodies.GetCollisionGroup(world.floor)),
            "harness excluded ordinary world contact");
    for (int step = 0; step < 15 * hz && world.machine->state().draw_m < target_draw; ++step)
        world.tick(1);
    Shot result;
    result.draw = world.machine->state().draw_m;
    result.energy = world.machine->state().energy_j;
    result.work = world.machine->state().work_j;
    require(result.draw >= target_draw - .02, "finite muscle source could not reach requested draw");
    require(!world.machine->state().aim_locked, "charged rail remains available for paid aiming");
    // A released manual pull retains its small carriage momentum until the
    // springs slow it and the catch takes the load; it is not velocity-reset.
    for (int step = 0; step < hz / 4; ++step) world.tick();
    const double held = world.machine->state().draw_m;
    const double held_work = world.machine->state().work_j;
    for (int step = 0; step < hz; ++step) world.tick();
    require(std::abs(world.machine->state().draw_m - held) < .015, "unilateral ratchet failed to hold");
    require(std::abs(world.machine->state().work_j - held_work) < .01, "idle ratchet created source work");
    require(world.machine->state().energy_j <= result.work, "spring work exceeds declared muscle work");
    for (const auto point : world.machine->prediction())
        result.predicted_apex = std::max(result.predicted_apex, double(point.GetY()));
    const auto before_release_velocity = bodies.GetLinearVelocity(world.player);
    world.machine->pre_step(1.0F / hz, 0, 0, Slingshot::State{}.elevation_rad, true, false);
    require((bodies.GetLinearVelocity(world.player) - before_release_velocity).Length() == 0,
            "release authored launch velocity");
    require(!world.machine->state().guide_latched && world.machine->controls_player(),
            "release did not remove guide while preserving physical harness");
    bool detached = false;
    for (int step = 0; step < 10 * hz; ++step) {
        const auto before = bodies.GetLinearVelocity(world.player);
        world.tick();
        const auto position = bodies.GetPosition(world.player);
        result.apex = std::max(result.apex, double(position.GetY()));
        if (!detached && !world.machine->controls_player()) {
            detached = true;
            result.detach_position = position;
            result.detach_velocity = bodies.GetLinearVelocity(world.player);
            result.speed = result.detach_velocity.Length();
            require(std::abs(result.detach_velocity.GetX() - before.GetX()) < .1,
                    "slack detachment changed lateral momentum");
        }
        require(world.machine->state().leather_port_violation_j < 1.0,
                "leather material port injected unexplained energy");
        if (world.machine->state().ledger_valid)
            result.maximum_residual = std::max(result.maximum_residual,
                std::abs(world.machine->state().energy_residual_j));
        if (detached && bodies.GetLinearVelocity(world.player).GetY() < 0) break;
    }
    require(detached, "taut bands never became slack and detached the rider");
    require(!world.machine->state().pouch_pair_excluded &&
            same_group(bodies.GetCollisionGroup(world.player), original_group),
            "separated rider did not restore original collision group");
    result.maximum_power = world.maximum_source_power;
    return result;
}
} // namespace

int main() {
    RegisterDefaultAllocator();
    Factory::sInstance = new Factory;
    RegisterTypes();
    int result = 0;
    try {
    {
        World world(90);
        world.tick(0, true);
        for (int step = 0; step < 15 * world.hz && world.machine->state().draw_m < 11.97; ++step)
            world.tick(1);
        for (int step = 0; step < world.hz; ++step) world.tick();
        const auto held = world.machine->state();
        require(held.draw_m > 11.9 && held.energy_j > 400000,
                "charged aiming proof requires a real full manual draw");
        require(!held.aim_locked, "held rubber must permit paid pitch and yaw aiming");
        for (int step = 0; step < 10 * world.hz; ++step) {
            world.tick(0, false, false, .28, 1.10);
            if (world.machine->state().aim_ready &&
                std::abs(world.machine->state().elevation_rad - 1.10) < .01) break;
        }
        const auto aimed = world.machine->state();
        require(aimed.aim_ready && std::abs(aimed.yaw_rad - .28) < .01 &&
                std::abs(aimed.elevation_rad - 1.10) < .01,
                "charged gimbal must reach both requested angles");
        require(std::abs(aimed.draw_m - held.draw_m) < .03 &&
                std::abs(aimed.energy_j - held.energy_j) < 2000,
                "charged aiming must retain actual ratchet stretch and rubber energy");
        require(aimed.work_j > held.work_j,
                "charged rail rotation must record finite actuator work");
        std::cout << "charged aim yaw=" << aimed.yaw_rad << " elevation=" << aimed.elevation_rad
                  << " draw=" << aimed.draw_m << " work_delta=" << aimed.work_j - held.work_j << '\n';
        require(aimed.release_ready, "adjusted charged pose must permit physical release");
        world.tick(0, true, false, .28, 1.10);
        for (int step = 0; step < 2 * world.hz; ++step) world.tick(0, false, false, .28, 1.10);
        const auto flight = world.physics.GetBodyInterface().GetPosition(world.player);
        require(world.machine->state().released && !world.machine->state().seated &&
                flight.GetX() > held.pouch_position.GetX() + 10 && flight.GetY() > 40,
                "release must leave the real guide with momentum along the charged aim");
        std::cout << "charged aimed release position=" << flight.GetX() << ',' << flight.GetY()
                  << ',' << flight.GetZ() << '\n';
    }
        {
            World world(90);
            for (int step = 0; step < world.hz; ++step) world.tick(1);
            require(!world.machine->controls_player() && world.machine->state().work_j == 0 &&
                    world.machine->state().energy_j < 1.0,
                    "backward approach seated or charged without explicit BOARD");
            world.tick(0, true);
            require(world.machine->controls_player(), "explicit BOARD did not join the rider");
            world.physics.GetBodyInterface().AddImpulse(world.player, Vec3(0, 0, 170));
            for (int step = 0; step < world.hz / 2; ++step) world.tick();
            require(world.machine->state().draw_m < .03 && world.machine->state().work_j == 0 &&
                    world.machine->state().energy_j < 1.0 && !world.machine->state().aim_locked,
                    "boarding momentum opened the undrawn seat restraint");
            for (int step = 0; step < world.hz; ++step) world.tick(1);
            require(world.machine->state().draw_m > .1 && world.machine->state().work_j > 0 &&
                    world.machine->state().energy_j > 1.0 && !world.machine->state().aim_locked,
                    "authorized manual draw did not store real work while retaining aim");
        }
        {
            World world(90);
            auto &bodies = world.physics.GetBodyInterface();
            world.tick(0, true);
            for (int step = 0; step < 12; ++step) world.tick(1);
            require(world.machine->state().ledger_valid, "closed draw fixture began with invalid audit");
            const auto pouch = world.parts->body_position(world.machine->pouch_body());
            BodyCreationSettings obstruction(new BoxShape(Vec3(.25F, .05F, .25F)),
                pouch + RVec3(0, -.23, 0), Quat::sIdentity(), EMotionType::Static, 0);
            obstruction.mUserData = 9900;
            const auto obstacle = bodies.CreateAndAddBody(obstruction, EActivation::DontActivate);
            for (int step = 0; step < 3; ++step) world.tick(1);
            require(world.contacts.pouch_contacts > 0, "draw audit fixture produced no real pouch contact");
            require(!world.machine->state().released && !world.machine->state().ledger_valid,
                    "real pouch collision during draw retained a closed-system audit");
            require(std::isfinite(world.machine->state().energy_residual_j) &&
                    std::abs(world.machine->state().energy_residual_j) > .001,
                    "contact invalidation erased the raw energy residual");
            bodies.RemoveBody(obstacle);
            bodies.DestroyBody(obstacle);
        }
        {
            World world(90);
            auto &bodies = world.physics.GetBodyInterface();
            world.tick(0, true, false, 0, 1.0);
            const auto carrier = world.parts->body_id(world.parts->body_for_entity(Slingshot::kLaunchRailEntity));
            const auto point = bodies.GetPosition(carrier) + RVec3(bodies.GetRotation(carrier) * Vec3(-1.05F, 0, -4));
            BodyCreationSettings obstruction(new BoxShape(Vec3(.18F, .18F, .18F)),
                point, Quat::sIdentity(), EMotionType::Static, 0);
            obstruction.mUserData = 9901;
            const auto obstacle = bodies.CreateAndAddBody(obstruction, EActivation::DontActivate);
            for (int step = 0; step < 3; ++step) world.tick(0, false, false, 0, 1.0);
            require(world.contacts.carrier_contacts > 0, "aim audit fixture produced no real rail contact");
            require(world.machine->state().energy_j < 1.0 && !world.machine->state().ledger_valid,
                    "real launch-rail collision during slack aim retained a closed-system audit");
            bodies.RemoveBody(obstacle);
            bodies.DestroyBody(obstacle);
        }
        {
            World world(90);
            auto &bodies = world.physics.GetBodyInterface();
            world.tick(0, true);
            world.tick(0, true);
            require(world.machine->controls_player() && !world.machine->state().released &&
                    world.machine->state().launch_count == 0 && !world.machine->state().release_ready,
                    "zero-work release escaped the charging guide");
            for (int step = 0; step < 3 * world.hz && world.machine->state().draw_m < 3; ++step) world.tick(1);
            world.tick(0, true);
            require(!world.machine->state().released && world.machine->controls_player(),
                    "underfunded pull trapped the rider in an incomplete launch");
            for (int step = 0; step < 3 * world.hz && world.machine->state().draw_m < 6; ++step) world.tick(1);
            require(world.machine->state().release_ready, "funded partial pull did not enable release");
            const auto before = bodies.GetLinearVelocity(world.player);
            world.machine->pre_step(1.0F / world.hz, 0, 0, Slingshot::State{}.elevation_rad, true, false);
            require(world.machine->state().released && world.machine->state().launch_count == 1 &&
                    (bodies.GetLinearVelocity(world.player) - before).Length() == 0,
                    "continued manual charge failed physical release");
        }
        {
            World world(360);
            const double dt = 1.0 / world.hz;
            world.tick(0, true, false, 0, .35);
            for (int step = 0; step < 6 * world.hz && !world.machine->state().aim_ready; ++step)
                world.tick(0, false, false, 0, .35);
            std::cout << "minimum aim=" << world.machine->state().elevation_rad << " ready=" << world.machine->state().aim_ready
                      << " draw=" << world.machine->state().draw_m << " work=" << world.machine->state().work_j << '\n';
            require(std::abs(world.machine->state().elevation_rad - .35) < .01 && world.machine->state().aim_ready,
                    "physical gimbal did not settle at minimum aim");
            const auto fork_before = world.machine->state().anchor_left;
            for (int step = 0; step < 8 * world.hz; ++step) {
                const double before = world.machine->state().aim_control_work_j;
                world.tick(0, false, false, .3, 1.48);
                if (world.machine->state().aim_control_work_j - before >
                    Slingshot::kPlayerPowerW * Slingshot::kTransmissionEfficiency * dt + 1)
                    std::cout << "aim step=" << step << " delta=" << world.machine->state().aim_control_work_j - before
                              << " elevation=" << world.machine->state().elevation_rad << '\n';
                require(world.machine->state().aim_control_work_j - before <=
                            Slingshot::kPlayerPowerW * Slingshot::kTransmissionEfficiency * dt + 1,
                        "aim actuator exceeded finite manual work rate");
                require(world.machine->state().energy_j < 1.0,
                        "slack aiming created uncharged band energy");
                if (world.machine->state().aim_ready && std::abs(world.machine->state().elevation_rad - 1.48) < .01) break;
            }
            std::cout << "aim elevation=" << world.machine->state().elevation_rad << " draw="
                      << world.machine->state().draw_m << " U=" << world.machine->state().energy_j << '\n';
            require(std::abs(world.machine->state().elevation_rad - 1.48) < .01 && world.machine->state().aim_ready &&
                    (world.machine->state().anchor_left - fork_before).Length() < .0001,
                    "finite aim control never reached requested elevation");
        }
        {
            World world(90);
            auto &bodies = world.physics.GetBodyInterface();
            const auto original_group = bodies.GetCollisionGroup(world.player);
            kit::Kit::Checkpoint pristine_physics;
            world.parts->capture(pristine_physics);
            const auto pristine = world.machine->state();
            const auto player_at = bodies.GetPosition(world.player);
            world.tick(0, true, false, .2, 1.35);
            for (int step = 0; step < 4 * world.hz && world.machine->state().draw_m < 6; ++step)
                world.tick(1, false, false, .2, 1.35);
            for (int step = 0; step < world.hz; ++step) world.tick(0, false, false, .2, 1.35);
            kit::Kit::Checkpoint charged_physics;
            world.parts->capture(charged_physics);
            const auto charged = world.machine->state();
            const auto charged_player_at = bodies.GetPosition(world.player);
            const auto charged_velocity = bodies.GetLinearVelocity(world.player);
            world.tick(0, true, false, .2, 1.35);
            for (int step = 0; step < world.hz && !world.machine->state().track_exit; ++step) world.tick();
            require(world.machine->state().pouch_pair_excluded,
                    "track exit dropped collision filtering before leather separation");
            world.machine->reset();
            require(same_group(bodies.GetCollisionGroup(world.player), original_group),
                    "reset during separation failed original group cleanup");
            bodies.SetPosition(world.player, player_at, EActivation::Activate);
            bodies.SetLinearVelocity(world.player, Vec3::sZero());
            world.tick(0, true, false, -.4, .8);
            world.parts->restore(charged_physics);
            bodies.SetPosition(world.player, charged_player_at, EActivation::Activate);
            bodies.SetLinearVelocity(world.player, charged_velocity);
            world.machine->restore(charged);
            require((world.machine->state().anchor_left - charged.anchor_left).Length() < .0001 &&
                    world.machine->controls_player(), "charged restore retained later live aim or lost harness");
            world.tick(0, false, false, charged.yaw_rad, charged.elevation_rad);
            require(std::abs(world.machine->state().energy_residual_j - charged.energy_residual_j) < 100 &&
                    std::abs(world.machine->state().work_j - charged.work_j) < .01,
                    "restored guide injected unexplained work on its next step");
            world.parts->restore(pristine_physics);
            bodies.SetPosition(world.player, player_at, EActivation::Activate);
            bodies.SetLinearVelocity(world.player, Vec3::sZero());
            world.machine->restore(pristine);
            world.tick();
            require(world.machine->state().energy_residual_j == 0 && world.machine->state().work_j == 0 &&
                    same_group(bodies.GetCollisionGroup(world.player), original_group),
                    "pristine restore retained old audit baseline or pair filter");
            world.tick(0, true);
            for (int step = 0; step < world.hz; ++step) world.tick(1);
            require(world.machine->state().work_j > 0 && world.machine->state().ledger_valid,
                    "pristine restore failed to start a new audited manual draw");
        }
        {
            World world(90);
            auto &bodies = world.physics.GetBodyInterface();
            world.tick(0, true);
            for (int step = 0; step < world.hz; ++step) world.tick(1);
            kit::Kit::Checkpoint physics;
            world.parts->capture(physics);
            const auto saved = world.machine->state();
            world.machine->detach();
            world.parts->restore(physics);
            bodies.SetPosition(world.player, RVec3(10, .9, -82), EActivation::Activate);
            bodies.SetLinearVelocity(world.player, Vec3::sZero());
            world.machine->restore(saved);
            require(!world.machine->controls_player() && !world.machine->state().seated,
                    "nonfitting checkpoint rider was attached remotely");
            world.contacts.rider_contacts = 0;
            for (int step = 0; step < 5; ++step) world.tick();
            require(world.contacts.rider_contacts > 0, "nonmember rider fixture produced no real ground contacts");
            require(world.machine->state().ledger_valid,
                    "failed checkpoint boarding retained unrelated rider in machine audit");
        }
        {
            World world(180);
            auto &bodies = world.physics.GetBodyInterface();
            bodies.SetPosition(world.player, RVec3(10, .9, -82), EActivation::Activate);
            world.tick(0, true);
            require(!world.machine->controls_player(), "remote Action seated player");
            for (int i = 0; i < 360; ++i) world.tick(1);
            require(world.machine->state().energy_j < .01 && world.machine->state().work_j == 0,
                    "unoccupied/no-input machine acquired launch energy");
        }
        {
            World world(90);
            auto &bodies = world.physics.GetBodyInterface();
            world.tick(0, true, false, .3, .95);
            require(world.machine->controls_player(), "slack aim prevented normal boarding");
            const auto frame_at = world.parts->body_position(world.machine->frame_body());
            const auto frame_rotation = world.parts->body_rotation(world.machine->frame_body());
            require((frame_at + RVec3(frame_rotation * Vec3(-3, 0, -10)) -
                    world.machine->state().anchor_left).Length() < .0001,
                    "visible fork tip differs from physical band anchor");
            require(world.machine->state().energy_j < .01, "undrawn aiming preloaded the bands");
            for (int step = 0; step < 6 * world.hz && !world.machine->state().aim_ready; ++step)
                world.tick(0, false, false, .3, .95);
            require(world.machine->state().aim_ready, "physical aim did not settle before draw fixture");
            for (int step = 0; step < 6 * world.hz && world.machine->state().draw_m < 6; ++step)
                world.tick(1, false, false, .3, .95);
            const auto drop_velocity = bodies.GetLinearVelocity(world.player);
            world.machine->pre_step(1.0F / world.hz, 0, .3, .95, false, true);
            require(!world.machine->controls_player() &&
                    (bodies.GetLinearVelocity(world.player) - drop_velocity).Length() == 0,
                    "manual unseating authored momentum");
            for (int step=0;step<world.hz/2 && !world.machine->state().station_available;++step)
                world.tick(0,false,false,.3,.95);
            world.tick(0, true, false, .3, .95);
            require(world.machine->controls_player(), "physically resupported rider could not rejoin held pouch");
            for (int step = 0; step < world.hz; ++step) world.tick(0, false, false, .3, .95);
            kit::Kit::Checkpoint checkpoint;
            world.parts->capture(checkpoint);
            const auto saved = world.machine->state();
            std::cout << "LEATHER reboard_rest_y=" << saved.harness_rest_local.GetY() << " q=" << saved.leather_deflection_m << "\n";
            require(saved.harness_rest_local.GetY() >= .68 && saved.harness_rest_local.GetY() < .85 &&
                    saved.leather_deflection_m < -.015 && saved.leather_deflection_m > -.10,
                    "reboarding ratcheted the leather rest height or lost load compliance");
            const auto player_at = bodies.GetPosition(world.player);
            const auto player_velocity = bodies.GetLinearVelocity(world.player);
            world.tick(0, true, false, .3, .95);
            require(world.machine->state().launch_count == saved.launch_count + 1,
                    "checkpoint fixture did not actually release its charged shot");
            for (int step = 0; step < 20; ++step) world.tick();
            world.machine->detach();
            world.parts->restore(checkpoint);
            bodies.SetPosition(world.player, player_at, EActivation::Activate);
            bodies.SetLinearVelocity(world.player, player_velocity);
            world.machine->restore(saved);
            require(world.machine->controls_player() && world.machine->state().guide_latched &&
                    std::abs(world.machine->state().energy_j - saved.energy_j) < .01,
                    "checkpoint failed to restore physical draw/harness");
            require(world.machine->state().launch_count > saved.launch_count,
                    "checkpoint rewind erased cumulative launch history");
            world.tick(0, true);
            const auto count = world.machine->state().launch_count;
            const auto player_before_reset = bodies.GetPosition(world.player);
            world.machine->reset();
            require(!world.machine->controls_player() && world.machine->state().launch_count == count &&
                    (bodies.GetPosition(world.player) - player_before_reset).Length() == 0,
                    "explicit machine reset teleported player or erased launch history");
        }
        {
            World world(90);
            auto &bodies = world.physics.GetBodyInterface();
            world.tick(0, true);
            for (int step = 0; step < 10 * world.hz && world.machine->state().draw_m < 11.999; ++step) world.tick(1);
            world.tick(0, true);
            for (int step = 0; step < 25 * world.hz; ++step) world.tick();
            bodies.SetPosition(world.player, Slingshot::retrieval_control_position() + RVec3(0, .20, -.4), EActivation::Activate);
            bodies.SetLinearVelocity(world.player, Vec3::sZero());
            world.tick(0, true);
            require(world.machine->state().recovering, "ordinary grade control could not activate retrieval");
            for (int step = 0; step < world.hz; ++step) world.tick();
            require(world.machine->state().retrieval_work_j == 0 && !world.machine->state().guide_latched,
                    "retrieval without manual hold supplied work or relatched");
            for (int step=0;step<world.hz*7/10;++step) world.tick(1);
            for (int step=0;step<world.hz*15/100;++step) world.tick();
            const double stopped_reel_work = world.machine->state().retrieval_work_j;
            for (int step=0;step<world.hz*3/10;++step) world.tick();
            require(std::abs(world.machine->state().retrieval_work_j-stopped_reel_work)<.1,
                    "released reel counted stale motor impulse as manual work");
            double recovery_peak = 0;
            int recovery_ticks = 0;
            while (!world.machine->state().guide_latched && recovery_ticks < 15 * world.hz) {
                world.tick(1);
                recovery_peak = std::max(recovery_peak, world.machine->state().retrieval_source_power_w);
                ++recovery_ticks;
            }
            std::cout << "recovery seconds=" << double(recovery_ticks)/world.hz << " power=" << recovery_peak
                      << " pouch=" << world.machine->state().pouch_position.GetY() << ','
                      << world.machine->state().pouch_position.GetZ() << " U=" << world.machine->state().energy_j << '\n';
            require(world.machine->state().guide_latched && !world.machine->state().released,
                    "manual physical pouch/rail retrieval did not complete within15seconds");
            require(recovery_peak <= Slingshot::kRetrievalPowerW * 1.025,
                    "retrieval source exceeded its shared power budget");
            require(world.machine->state().energy_residual_j == 0,
                    "new physically recovered shot retained stale ledger residual");
            bodies.SetPosition(world.player, world.machine->state().pouch_position + RVec3(0, .73, 0), EActivation::Activate);
            for (int step=0;step<world.hz/2 && !world.machine->state().station_available;++step)
                world.tick(0,false,false,.2,1.35);
            world.tick(0, true, false, .2, 1.35);
            require(world.machine->controls_player(),
                    "returned pouch could not board and select fresh aim");
            for (int step = 0; step < 6 * world.hz && !world.machine->state().aim_ready; ++step)
                world.tick(0, false, false, .2, 1.35);
            require(std::abs(world.machine->state().yaw_rad - .2) < .01 && world.machine->state().aim_ready,
                    "returned physical gimbal did not reach fresh aim");
            const auto count = world.machine->state().launch_count;
            for (int step = 0; step < 10 * world.hz && world.machine->state().draw_m < 8; ++step)
                world.tick(1, false, false, .2, 1.35);
            world.tick(0, true, false, .2, 1.35);
            require(world.machine->state().launch_count == count + 1,
                    "physically retrieved launcher could not fire a second shot");
            for (int step = 0; step < 3; ++step) world.tick(0, false, false, .2, 1.35);
            const auto guided = world.machine->state();
            require(guided.guided_launch && world.machine->controls_player(),
                    "guided checkpoint fixture reached track exit too early");
            kit::Kit::Checkpoint guided_physics;
            world.parts->capture(guided_physics);
            const auto guided_player_at = bodies.GetPosition(world.player);
            const auto guided_velocity = bodies.GetLinearVelocity(world.player);
            for (int step = 0; step < 10; ++step) world.tick(0, false, false, .2, 1.35);
            world.parts->restore(guided_physics);
            bodies.SetPosition(world.player, guided_player_at, EActivation::Activate);
            bodies.SetLinearVelocity(world.player, guided_velocity);
            world.machine->restore(guided);
            require((world.machine->state().launch_track_start - guided.launch_track_start).Length() < .0001 &&
                    (world.machine->state().pouch_position - guided.pouch_position).Length() < .0001,
                    "recovered-offset guided restore shifted the physical release reference");
            world.tick(0, false, false, .2, 1.35);
            require(std::abs(world.machine->state().energy_residual_j - guided.energy_residual_j) < 500 &&
                    std::abs(world.machine->state().work_j - guided.work_j) < .01,
                    "guided restore injected source work or relocked the wrong rail draw");
        }
        const auto short_shot = shoot(180, 6);
        const auto medium_shot = shoot(180, 9);
        const auto full_90 = shoot(90, 11.999);
        const auto full_180 = shoot(180, 11.999);
        const auto full_360 = shoot(360, 11.999);
        for (const auto &shot : {short_shot, medium_shot, full_90, full_180, full_360})
            std::cout << "draw=" << shot.draw << " spring_J=" << shot.energy << " player_J=" << shot.work
                      << " apex_m=" << shot.apex << " exit_mps=" << shot.speed
                      << " predicted_apex_m=" << shot.predicted_apex << " residual_J=" << shot.maximum_residual << " source_W=" << shot.maximum_power << '\n';
        require(short_shot.apex < medium_shot.apex && medium_shot.apex < full_180.apex,
                "player draw does not determine increasing tower height");
        require(full_180.apex > 300, "full manual draw fails substantial tower height");
        require(std::abs(full_180.apex - full_360.apex) < .30 &&
                std::abs(full_90.apex - full_360.apex) < .30,
                "native launch does not converge across physics rates");
        for (const auto &shot : {full_90, full_180, full_360}) {
            require(shot.maximum_residual < .02 * shot.energy,
                    "native loaded-pouch energy ledger exceeds 2 percent");
            require(shot.maximum_power <= Slingshot::kPlayerPowerW * 1.025,
                    "arcade muscle source exceeds declared power");
            require(std::abs(shot.apex - shot.predicted_apex) < .6,
                    "same-law preview differs materially from unobstructed native trajectory");
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;
    return result;
}
