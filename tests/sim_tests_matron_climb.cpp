#include <sstream>
#include <array>
#include <chrono>
#include <cstdio>
// The Matron's climbing moves (matron_traversal.hpp) on small worlds of real
// steel: each move up and down, and each way a move must fail physically
// (a missing rung, a member that is no longer the one she took, a mantle her
// capsule cannot pass, a pole overloaded until it slips, a jump that misses,
// a hold on a dynamic body pushing back on it). Nothing here sets her
// position after the start: every meter she makes, her capped forces make.
#include "sim_tests.hpp"
#include "sim/matron.hpp"
#include "sim/matron_traversal.hpp"
#include "sim/physics_world_support.hpp"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>

namespace {
using namespace JPH;
using namespace scraperx::sim;
using traversal::Climber;
using traversal::Failure;
using traversal::MoveKind;
using traversal::MoveSpec;

// Her limits in the sim stay inside the owner's laws (sim_tests.hpp): a
// raise past them does not build until the law itself is changed.
static_assert(traversal::Limits{}.total_force_n <= law::kForceN, "her shared force cap is the law's 8,000 N at most");
static_assert(traversal::Limits{}.power_w <= law::kPowerW, "her shared power cap is the law's 44 kW at most");
static_assert(traversal::Limits{}.leg_travel_m <= law::kLegTravelM, "a push-off's stroke is the law's 0.85 m at most");
static_assert(traversal::Limits{}.launch_dv_mps <= law::kLaunchDvMps, "a push-off earns the law's 6.30 m/s at most");
static_assert(traversal::Limits{}.mass_kg == law::kMassKg && Matron::kMassKg == law::kMassKg, "she weighs 220 kg");
static_assert(Matron::kClimbGripMaxN <= law::kForceN, "her old climb's grip is inside the law's force cap");
static_assert(Matron::kTurnLungeForceN <= law::kLungeForceN, "the Turn lunge's force cap is the law's 9,800 N at most");
static_assert(Matron::kTurnLungePowerW <= law::kLungePowerW, "the Turn lunge's power cap is the law's 65 kW at most");

struct ClimbWorld {
    detail::JoltRuntimeLease runtime;
    detail::BroadPhaseLayerInterface layers;
    detail::ObjectVsBroadPhaseFilter broad;
    detail::ObjectLayerPairFilter pairs;
    TempAllocatorImpl temp{16 * 1024 * 1024};
    JobSystemThreadPool jobs{cMaxPhysicsJobs, cMaxPhysicsBarriers, 1};
    PhysicsSystem physics;
    std::vector<BodyID> bodies;
    BodyID her;
    static constexpr float dt = 1.0F / 90.0F;
    float peak_force = 0.0F;
    bool gravity_always = true;

    ClimbWorld() {
        physics.Init(4096, 0, 4096, 2048, layers, broad, pairs);
        box(Vec3(20, 0.2F, 20), RVec3(0, -0.2F, 0));   // the floor, top 0
    }
    ~ClimbWorld() {
        BodyInterface &b = physics.GetBodyInterface();
        for (const BodyID id : bodies) {
            if (b.IsAdded(id)) b.RemoveBody(id);
            b.DestroyBody(id);
        }
    }
    BodyID box(const Vec3 half, const RVec3 at, const bool sensor = false, const EMotionType motion = EMotionType::Static,
               const float mass = 0.0F) {
        BodyCreationSettings s(new BoxShape(half, std::min(0.05F, half.ReduceMin())), at, Quat::sIdentity(), motion,
                               motion == EMotionType::Static ? detail::object_layers::kStatic : detail::object_layers::kMoving);
        s.mIsSensor = sensor;
        if (mass > 0.0F) {
            s.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
            s.mMassPropertiesOverride.mMass = mass;
        }
        const BodyID id = physics.GetBodyInterface().CreateAndAddBody(s, motion == EMotionType::Static ? EActivation::DontActivate : EActivation::Activate);
        bodies.push_back(id);
        return id;
    }
    // An upper deck: a solid block whose south face is at z = -1, top at h.
    void deck(const float h) { box(Vec3(5, 0.5F * h, 3), RVec3(0, 0.5F * h, -4)); }
    // Rungs on the deck's face (x 0, z -0.95), from y0 to y1 every pitch.
    std::vector<BodyID> rungs(const float y0, const float y1, const float pitch = 0.3F, const bool sensor = false) {
        std::vector<BodyID> out;
        for (float y = y0; y <= y1 + 1.0e-3F; y += pitch) out.push_back(box(Vec3(0.25F, 0.02F, 0.02F), RVec3(0, y, -0.95F), sensor));
        return out;
    }
    void spawn(const RVec3 feet) {
        BodyCreationSettings s(new CapsuleShape(Matron::kHalfHeight, Matron::kRadius), feet + Vec3(0, Matron::kStandCentre, 0),
                               Quat::sIdentity(), EMotionType::Dynamic, detail::object_layers::kMoving);
        s.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ;
        s.mAllowSleeping = false;
        s.mFriction = 0.0F;
        s.mLinearDamping = 0.0F;
        s.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        s.mMassPropertiesOverride.mMass = Matron::kMassKg;
        her = physics.GetBodyInterface().CreateAndAddBody(s, EActivation::Activate);
        bodies.push_back(her);
    }
    RVec3 at() const { return physics.GetBodyInterface().GetPosition(her); }
    float feet() const { return float(at().GetY()) - Matron::kStandCentre; }
    Vec3 force(const BodyID body) const {
        BodyLockRead lock(physics.GetBodyLockInterface(), body);
        require(lock.Succeeded(), "matron climb: force observation requires a live body");
        return lock.GetBody().GetAccumulatedForce();
    }
    void actuate(Climber &climber) {
        const Vec3 before_velocity = physics.GetBodyInterface().GetLinearVelocity(her);
        const Vec3 before_force = force(her);
        climber.step(dt);
        const Vec3 applied = force(her) - before_force;
        require((physics.GetBodyInterface().GetLinearVelocity(her) - before_velocity).Length() < 1.0e-6F,
                "matron climb: actuators never change velocity before the physics step");
        require(applied.Length() <= climber.limits().total_force_n + 0.5F &&
                    applied.Length() <= climber.view().force_n + 0.5F,
                "matron climb: actual applied force is capped and charged to the shared budget");
        require(climber.view().power_w <= climber.limits().power_w + 1.0F,
                "matron climb: every tick's positive power inside her one budget");
        peak_force = std::max(peak_force, climber.view().force_n);
        require(climber.view().force_n <= climber.limits().total_force_n + 0.5F, "matron climb: every tick's force inside her one budget");
        gravity_always = gravity_always && physics.GetBodyInterface().GetGravityFactor(her) == 1.0F;
    }
    void tick(Climber &climber) {
        actuate(climber);
        physics.Update(dt, 1, &temp, &jobs);
    }
    // Runs until the move ends (or seconds), with a check on the way.
    template <class Each>
    Climber::Status run(Climber &climber, const float seconds, Each each) {
        for (int i = 0; i < int(seconds * 90.0F); ++i) {
            tick(climber);
            each(i);
            if (climber.status() != Climber::Status::Running) break;
        }
        return climber.status();
    }
    Climber::Status run(Climber &climber, const float seconds) { return run(climber, seconds, [](int) {}); }
    // Lets her settle (no hands, no feet), then reports where her feet stand.
    float settle(const float seconds = 3.0F) {
        for (int i = 0; i < int(seconds * 90.0F); ++i) physics.Update(dt, 1, &temp, &jobs);
        return feet();
    }
};

MoveSpec ladder_up(const float top, const float y0, const float y1) {
    MoveSpec s;
    s.kind = MoveKind::RungUp;
    s.line = Vec3(0, 0, -0.95F);
    s.out = Vec3(0, 0, 1);
    s.bottom_y = y0;
    s.top_y = y1;
    s.start = Vec3(0, 0, -0.5F);
    s.goal = Vec3(0, top, -2.0F);
    s.goal_top = top;
    return s;
}

void test_rungs() {
    // Up a ladder onto the deck, then down it again (a separate move).
    ClimbWorld w;
    w.deck(4.0F);
    w.rungs(0.3F, 3.9F);
    w.spawn(RVec3(0, 0, 1.0F));
    Climber climber(w.physics, w.her, {w.her, BodyID()});
    climber.begin(ladder_up(4.0F, 0.3F, 3.9F));
    float max_feet = 0.0F;
    const auto up = w.run(climber, 30.0F, [&](int i) {
        max_feet = std::max(max_feet, w.feet());
        if (std::getenv("CLIMB_DEBUG") && (i % 9 == 0 || climber.status() != Climber::Status::Running))
            std::cout << "    t " << i << " phase " << climber.view().phase << " kind " << int(climber.view().kind) << " at " << w.at().GetY() << "," << w.at().GetZ() << " hold " << climber.view().holding << " f " << climber.view().force_n << "\n";
    });
    const float on_top = w.settle(1.0F);
    std::cout << "  matron climb rungs: up " << int(up) << " (" << traversal::failure_name(climber.failure()) << ") z " << w.at().GetZ() << " grips " << climber.view().grips << " feet " << on_top
              << " peak " << w.peak_force << " N\n";
    require(up == Climber::Status::Done && std::abs(on_top - 4.0F) < 0.05F && w.at().GetZ() < -1.2F,
            "matron climb: up the rungs and over the head onto the deck");
    require(climber.view().grips >= 10, "matron climb: hand over hand, rung by rung");
    MoveSpec down = ladder_up(4.0F, 0.3F, 3.9F);
    down.kind = MoveKind::RungDown;
    down.start = Vec3(0, 4.0F, -1.35F);
    down.goal = Vec3(0, 0, -0.5F);
    down.goal_top = 0.0F;
    climber.begin(down);
    const auto dn = w.run(climber, 40.0F);
    const float below = w.settle(1.0F);
    std::cout << "  matron climb rungs: down " << int(dn) << " (" << traversal::failure_name(climber.failure()) << ") feet " << below << "\n";
    require(dn == Climber::Status::Done && std::abs(below) < 0.05F, "matron climb: down the rungs, top entry, to the floor");
    require(w.gravity_always, "matron climb: her gravity never off on the rungs");
    require(climber.view().peak_power_w <= climber.limits().power_w + 1.0F, "matron climb: rung power within the shared cap");
    std::cout << "PASS scraperx_sim matron climb rungs up and down: peak_n=" << w.peak_force << "\n";
}

void test_rung_failures() {
    // A gap wider than her long reach and a throw cross (2.1 m): she climbs
    // to it and can go no higher; she lets go and falls.
    {
        ClimbWorld w;
        w.deck(5.0F);
        w.rungs(0.3F, 1.8F);
        w.rungs(3.9F, 4.9F);
        w.spawn(RVec3(0, 0, 1.0F));
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        climber.begin(ladder_up(5.0F, 0.3F, 4.9F));
        float max_feet = 0.0F;
        const auto st = w.run(climber, 30.0F, [&](int) { max_feet = std::max(max_feet, w.feet()); });
        const float landed = w.settle();
        std::cout << "  matron climb missing rung: " << traversal::failure_name(climber.failure()) << " highest feet " << max_feet
                  << " landed " << landed << "\n";
        require(st == Climber::Status::Failed && climber.failure() == Failure::MissingRung,
                "matron climb: a missing rung stops her: no hold, no climb");
        require(max_feet < 1.8F && std::abs(landed) < 0.05F, "matron climb: never over the gap; she lets go and lands on the floor");
    }
    // Sensor rungs are not steel: nothing to take.
    {
        ClimbWorld w;
        w.deck(4.0F);
        w.rungs(0.3F, 3.9F, 0.3F, true);
        w.spawn(RVec3(0, 0, 1.0F));
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        climber.begin(ladder_up(4.0F, 0.3F, 3.9F));
        const auto st = w.run(climber, 15.0F);
        require(st == Climber::Status::Failed && climber.failure() == Failure::NoHold && w.feet() < 0.05F,
                "matron climb: sensor rungs refuse her hands");
    }
    // A wrong compound member: the rung she holds is moved within its body.
    {
        ClimbWorld w;
        w.deck(4.0F);
        Ref<MutableCompoundShapeSettings> settings = new MutableCompoundShapeSettings;
        for (int i = 0; i < 13; ++i) settings->AddShape(Vec3(0, 0.3F + 0.3F * float(i), 0), Quat::sIdentity(), new BoxShape(Vec3(0.25F, 0.02F, 0.02F), 0.01F));
        Ref<Shape> shape = settings->Create().Get();
        BodyCreationSettings s(shape, RVec3(0, 0, -0.95F), Quat::sIdentity(), EMotionType::Static, detail::object_layers::kStatic);
        const BodyID ladder = w.physics.GetBodyInterface().CreateAndAddBody(s, EActivation::DontActivate);
        w.bodies.push_back(ladder);
        w.spawn(RVec3(0, 0, 1.0F));
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        climber.begin(ladder_up(4.0F, 0.3F, 3.9F));
        bool moved = false;
        float at_move = 0.0F;
        float max_after = 0.0F;
        const auto st = w.run(climber, 30.0F, [&](int) {
            if (!moved && w.feet() > 1.0F) {
                // Every rung shifted 0.6 m along x inside the same body: same ids, different steel.
                auto *mutable_shape = static_cast<MutableCompoundShape *>(const_cast<Shape *>(shape.GetPtr()));
                for (uint k = 0; k < mutable_shape->GetNumSubShapes(); ++k)
                    mutable_shape->ModifyShape(k, Vec3(0.6F, 0.3F + 0.3F * float(k), 0), Quat::sIdentity());
                w.physics.GetBodyInterface().NotifyShapeChanged(ladder, Vec3::sZero(), false, EActivation::DontActivate);
                moved = true;
                at_move = w.feet();
            }
            if (moved) max_after = std::max(max_after, w.feet());
        });
        const float landed = w.settle();
        std::cout << "  matron climb wrong member: " << traversal::failure_name(climber.failure()) << " at feet " << at_move
                  << " landed " << landed << "\n";
        require(moved && st == Climber::Status::Failed && climber.failure() == Failure::LostHold,
                "matron climb: a member that is no longer the one she took is no hold");
        require(max_after < at_move + 0.3F && std::abs(landed) < 0.05F, "matron climb: the lost hold drops her to the floor");
    }
    // A blocked mantle: a cabinet where she would stand over the head. (A
    // strip 0.4 m deep in front of it is a narrow ledge she now stands on;
    // 0.15 m is too little for her soles.)
    {
        ClimbWorld w;
        w.deck(4.0F);
        w.rungs(0.3F, 3.9F);
        w.box(Vec3(1.5F, 1.2F, 0.8F), RVec3(0, 4.0F + 1.2F, -1.95F));   // a cabinet 0.15 m behind the lip
        w.spawn(RVec3(0, 0, 1.0F));
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        climber.begin(ladder_up(4.0F, 0.3F, 3.9F));
        float max_feet = 0.0F;
        const auto st = w.run(climber, 30.0F, [&](int) { max_feet = std::max(max_feet, w.feet()); });
        const float landed = w.settle();
        std::cout << "  matron climb blocked mantle: " << traversal::failure_name(climber.failure()) << " highest feet " << max_feet << "\n";
        // Too little room for her soles in front of it, the lip is no hold to
        // mantle on (or, reached anyway, her sweep is blocked).
        require(st == Climber::Status::Failed && (climber.failure() == Failure::BlockedSweep || climber.failure() == Failure::NoHold),
                "matron climb: her capsule does not fit over the head: she does not go");
        require(max_feet < 4.0F && std::abs(landed) < 0.05F, "matron climb: never through the cabinet; down to the floor");
    }
    std::cout << "PASS scraperx_sim matron climb failures: missing rung, sensor, wrong member, blocked mantle\n";
}

void test_pole() {
    const auto pole_world = [](ClimbWorld &w) {
        w.deck(6.0F);
        w.box(Vec3(0.05F, 2.975F, 0.05F), RVec3(3, 2.975F, -0.55F));   // a 0.10 m standpipe 0.45 m off the face, top 5.95
    };
    MoveSpec up;
    up.kind = MoveKind::PoleUp;
    up.line = Vec3(3, 0, -0.55F);
    up.out = Vec3(0, 0, 1);
    up.bottom_y = 0.0F;
    up.top_y = 5.95F;
    up.goal = Vec3(3, 6.0F, -2.6F);
    up.goal_top = 6.0F;
    {
        ClimbWorld w;
        pole_world(w);
        w.spawn(RVec3(3, 0, 0.0F));
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        climber.begin(up);
        const auto st = w.run(climber, 40.0F);
        const float top = w.settle(1.0F);
        std::cout << "  matron climb pole up: " << int(st) << " (" << traversal::failure_name(climber.failure()) << ") feet " << top
                  << " slips " << climber.view().slips << "\n";
        require(st == Climber::Status::Done && std::abs(top - 6.0F) < 0.05F && climber.view().slips == 0,
                "matron climb: up the standpipe by friction, onto the deck over it");
        MoveSpec down = up;
        down.kind = MoveKind::PoleDown;
        down.start = Vec3(3, 6.0F, -1.32F);
        down.goal = Vec3(3, 0, -1.0F);
        down.goal_top = 0.0F;
        climber.begin(down);
        float fastest = 0.0F;
        const auto dn = w.run(climber, 30.0F, [&](int) {
            fastest = std::min(fastest, w.physics.GetBodyInterface().GetLinearVelocity(w.her).GetY());
        });
        const float bottom = w.settle(1.0F);
        std::cout << "  matron climb pole down: " << int(dn) << " (" << traversal::failure_name(climber.failure()) << ") feet "
                  << bottom << " fastest " << fastest << " m/s\n";
        require(dn == Climber::Status::Done && std::abs(bottom) < 0.05F && fastest > -3.5F,
                "matron climb: down the standpipe, braked by friction, to the floor");
        require(w.gravity_always, "matron climb: her gravity never off on the pole");
    }
    // Overload: 400 kg dropped onto her shoulders halfway up. Friction cannot
    // hold both: she slips, and slides down under it.
    {
        ClimbWorld w;
        pole_world(w);
        w.spawn(RVec3(3, 0, 0.0F));
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        climber.begin(up);
        bool dropped = false;
        float at_drop = 0.0F;
        float lowest_after = 99.0F;
        const auto st = w.run(climber, 40.0F, [&](int) {
            if (!dropped && w.feet() > 2.5F) {
                const RVec3 head = w.at() + Vec3(0, 1.4F, 0);
                w.box(Vec3(0.3F, 0.15F, 0.3F), head, false, EMotionType::Dynamic, 400.0F);
                dropped = true;
                at_drop = w.feet();
            }
            if (dropped) lowest_after = std::min(lowest_after, w.feet());
        });
        std::cout << "  matron climb pole overload: " << traversal::failure_name(climber.failure()) << " slips "
                  << climber.view().slips << " at " << at_drop << " lowest after " << lowest_after << "\n";
        require(dropped && climber.view().slips >= 1 && lowest_after < at_drop - 0.5F,
                "matron climb: overloaded past static friction, she slips down the pole");
        (void)st;
    }
    std::cout << "PASS scraperx_sim matron climb pole: grip, descent, slip under overload\n";
}

// These reachable launches exercise the actuator itself, before Jolt
// integrates its forces. The older, higher jump remains an independent
// acceptance case below.
void test_capped_launches() {
    for (const int launch : {0, 1, 2, 3}) {
        const bool dyno = launch == 1;
        const traversal::Limits budget;
        const float full_rise = budget.launch_dv_mps * budget.launch_dv_mps / (2.0F * 9.81F);
        const float full_bar = budget.center_above_feet() + budget.hand_above_center + budget.leg_travel_m + full_rise - 0.16F;
        const float first = launch == 3 ? full_bar : launch == 2 ? 3.13F : 2.55F;
        ClimbWorld w;
        w.deck(launch == 3 ? full_bar + 1.5F : 6.0F);
        if (dyno) w.rungs(1.3F, 1.3F);
        w.rungs(first, launch == 3 ? full_bar + 0.9F : 5.9F);
        w.spawn(RVec3(0, 0, dyno ? -0.5F : -0.6F));
        w.settle(0.3F);
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        MoveSpec move = ladder_up(6.0F, dyno ? 1.3F : first, first + 0.3F * 10.0F);
        if (!dyno) move.kind = MoveKind::JumpCatch;
        climber.begin(move);
        int drive_ticks = 0;
        float travel = 0.0F;
        float peak_power = 0.0F;
        float peak_end_power = 0.0F;
        float takeoff_speed = 0.0F;
        float worst_step = 0.0F;
        bool extended_contact = false;
        for (int tick = 0; tick < 90 * 8 && climber.status() == Climber::Status::Running; ++tick) {
            const auto launches = climber.view().launches;
            const RVec3 before = w.at();
            w.actuate(climber);
            const Vec3 push = w.force(w.her);
            if (climber.view().launches > launches) {
                takeoff_speed = w.physics.GetBodyInterface().GetLinearVelocity(w.her).GetY();
                require(takeoff_speed > 0.0F && takeoff_speed <= climber.limits().launch_dv_mps + 0.001F,
                        "matron launch: actual toe-off speed stays within the declared limit");
            }
            if (climber.view().leg_driving) {
                ++drive_ticks;
                travel = std::max(travel, climber.view().leg_travel_m);
                const float power = std::max(w.force(w.her).Dot(climber.relative_velocity()), 0.0F);
                peak_power = std::max(peak_power, power);
                require(power <= climber.limits().power_w + 1.0F,
                        "matron launch: actual force times relative velocity is within the power cap");
                require(climber.view().leg_travel_m >= -0.001F &&
                            climber.view().leg_travel_m <= climber.limits().leg_travel_m + 0.001F,
                        "matron launch: drive stays within the named leg travel");
                if (launch >= 2 && climber.view().leg_travel_m > 0.15F) {
                    require(!climber.supported(), "matron launch: extended legs do not widen ordinary standing probes");
                    extended_contact = true;
                }
            }
            w.physics.Update(ClimbWorld::dt, 1, &w.temp, &w.jobs);
            worst_step = std::max(worst_step, float((w.at() - before).Length()));
            if (climber.view().leg_driving) {
                const float end_power = std::max(push.Dot(climber.relative_velocity()), 0.0F);
                peak_end_power = std::max(peak_end_power, end_power);
                require(end_power <= climber.limits().power_w + 1.0F,
                        "matron launch: actual end-of-tick power remains within the cap");
                const float after_travel = climber.view().leg_travel_m + float(w.at().GetY() - before.GetY());
                travel = std::max(travel, after_travel);
                require(after_travel <= climber.limits().leg_travel_m + 0.001F,
                        "matron launch: the integrated force step stays within leg travel");
            }
            if (climber.view().catches != 0) break;
        }
        std::cout << "  matron capped " << (launch == 3 ? "full leg jump" : launch == 2 ? "extended jump" : dyno ? "dyno" : "jump")
                  << ": drive_ticks " << drive_ticks
                  << " travel " << travel << " power " << peak_power << " end_power " << peak_end_power
                  << " launches " << climber.view().launches
                  << " catches " << climber.view().catches << " takeoff_mps " << takeoff_speed
                  << " worst_step " << worst_step << " failure " << traversal::failure_name(climber.failure()) << "\n";
        require(drive_ticks > 1 && climber.view().launches == 1 && climber.view().catches == 1,
                "matron launch: both moves earn one launch and catch through a drive over several ticks");
        require(w.gravity_always, "matron launch: gravity stays on throughout the leg drive");
        require(worst_step < 0.20F, "matron launch: no step moves her more than 0.20 m");
        if (dyno) require(climber.view().releases >= 1, "matron launch: the dyno releases its old handhold");
        if (launch == 2) {
            require(extended_contact && travel > 0.20F,
                    "matron launch: the intermediate bar still needs physical leg extension");
        }
        if (launch == 3) {
            require(extended_contact && travel > climber.limits().leg_travel_m - 0.12F &&
                        takeoff_speed >= climber.limits().launch_dv_mps - 0.01F,
                    "matron launch: a real extended leg reaches the measured take-off limit");
            require(peak_end_power > climber.limits().power_w - 1.0F,
                    "matron launch: the longer drive exercises the power-limited phase");
        }
    }
    // A real dynamic tread receives the opposite actuator force. Removing
    // that tread during extension must end the drive, even with other
    // floor below; cancellation must also stop it before another step.
    for (const bool cancel : {false, true}) {
        ClimbWorld w;
        const BodyID tread = w.box(Vec3(1.0F, 0.2F, 0.5F), RVec3(0, 0.2F, -0.4F),
                                  false, EMotionType::Dynamic, 1000.0F);
        w.deck(6.4F);
        w.rungs(2.95F, 6.1F);
        w.spawn(RVec3(0, 0.4F, -0.6F));
        w.settle(0.3F);
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        MoveSpec move = ladder_up(6.4F, 2.95F, 6.1F);
        move.kind = MoveKind::JumpCatch;
        move.start.SetY(0.4F);
        climber.begin(move);
        int drive_ticks = 0;
        for (int tick = 0; tick < 90 * 8 && drive_ticks < 2; ++tick) {
            const Vec3 tread_before = w.force(tread);
            w.actuate(climber);
            if (climber.view().leg_driving) {
                ++drive_ticks;
                const Vec3 reaction = w.force(tread) - tread_before;
                require((w.force(w.her) + reaction).Length() < 1.0F,
                        "matron launch: dynamic tread receives the equal and opposite force");
                require(std::abs(reaction.Length() - climber.view().support_reaction_n) < 1.0F,
                        "matron launch: reported reaction matches force actually applied to the tread");
            }
            w.physics.Update(ClimbWorld::dt, 1, &w.temp, &w.jobs);
            if (climber.status() != Climber::Status::Running) break;
        }
        require(drive_ticks == 2, "matron launch: dynamic tread starts a sustained leg drive");
        if (cancel) climber.cancel();
        else w.physics.GetBodyInterface().RemoveBody(tread);
        w.actuate(climber);
        require(!climber.view().leg_driving && w.force(w.her).GetY() <= 0.5F,
                "matron launch: removed support or cancellation cannot keep pushing upward");
    }
    // A planted sole belongs to the same physical member even while a
    // rectangular tread yaws beneath it. The world's axis-aligned bounds
    // change as it rotates; the tread's own geometry has not changed.
    {
        ClimbWorld w;
        const BodyID tread = w.box(Vec3(1.0F, 0.2F, 0.5F), RVec3(0, 0.2F, -0.4F),
                                   false, EMotionType::Dynamic, 1000.0F);
        w.deck(6.4F);
        w.rungs(2.95F, 6.1F);
        w.spawn(RVec3(0, 0.4F, -0.6F));
        w.settle(0.3F);
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        MoveSpec move = ladder_up(6.4F, 2.95F, 6.1F);
        move.kind = MoveKind::JumpCatch;
        move.start.SetY(0.4F);
        climber.begin(move);
        BodyInterface &bodies = w.physics.GetBodyInterface();
        int drive_ticks = 0;
        float yaw_turn = 0.0F;
        bool planted = false;
        for (int tick = 0; tick < 90 * 4 && climber.status() == Climber::Status::Running; ++tick) {
            w.actuate(climber);
            if (climber.view().leg_driving) {
                planted = true;
                ++drive_ticks;
            }
            w.physics.Update(ClimbWorld::dt, 1, &w.temp, &w.jobs);
            if (planted) {
                // An external tread motor advances its rotation; the
                // Matron is never moved or given velocity by the fixture.
                yaw_turn += 0.035F;
                bodies.SetRotation(tread, Quat::sRotation(Vec3::sAxisY(), yaw_turn), EActivation::Activate);
            }
            if (climber.view().launches > 0) break;
        }
        std::cout << "  matron rotating tread: drive_ticks " << drive_ticks << " yaw " << yaw_turn
                  << " launches " << climber.view().launches << "\n";
        require(planted && yaw_turn > 0.12F && drive_ticks >= 6 && climber.view().launches == 1,
                "matron launch: planted leg follows the unchanged member on a rotating tread");
    }
    // The jump she is allowed to make, with the bar taken at toe-off. Rise is
    // after the foot leaves. The landing is the speed she comes back with,
    // not a velocity written onto her on the way down.
    {
        const traversal::Limits budget;
        const float ballistic = budget.launch_dv_mps * budget.launch_dv_mps / (2.0F * 9.81F);
        const float bar_y = budget.center_above_feet() + budget.hand_above_center + budget.leg_travel_m + ballistic - 0.16F;
        ClimbWorld w;
        w.deck(bar_y + 1.5F);
        std::vector<BodyID> bars = w.rungs(bar_y, bar_y + 0.6F);
        w.spawn(RVec3(0, 0, -0.6F));
        w.settle(0.3F);
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        MoveSpec move = ladder_up(bar_y + 1.5F, bar_y, bar_y + 0.6F);
        move.kind = MoveKind::JumpCatch;
        climber.begin(move);
        float takeoff = 0.0F;
        float toe_feet = 0.0F;
        float travel = 0.0F;
        float peak_force = 0.0F;
        float peak_power = 0.0F;
        float apex_feet = 0.0F;
        float landing = 0.0F;
        bool airborne = false;
        bool falling = false;
        for (int tick = 0; tick < 90 * 6; ++tick) {
            const auto launches = climber.view().launches;
            w.actuate(climber);
            if (climber.view().leg_driving) {
                travel = std::max(travel, climber.view().leg_travel_m);
                peak_force = std::max(peak_force, climber.view().force_n);
                peak_power = std::max(peak_power, climber.view().power_w);
            }
            if (!airborne && climber.view().launches > launches) {
                takeoff = w.physics.GetBodyInterface().GetLinearVelocity(w.her).GetY();
                toe_feet = w.feet();
                for (const BodyID bar : bars) {
                    if (w.physics.GetBodyInterface().IsAdded(bar)) w.physics.GetBodyInterface().RemoveBody(bar);
                }
                airborne = true;
            }
            w.physics.Update(ClimbWorld::dt, 1, &w.temp, &w.jobs);
            if (!airborne) continue;
            const float vy = w.physics.GetBodyInterface().GetLinearVelocity(w.her).GetY();
            apex_feet = std::max(apex_feet, w.feet());
            if (vy < 0.0F) falling = true;
            if (falling && w.feet() > 0.05F) landing = std::max(landing, -vy);
            if (falling && w.feet() <= 0.08F && vy > -1.0F) break;
        }
        const float landed = w.settle(1.0F);
        const float apex_m = apex_feet - toe_feet;
        std::cout << "  JUMP takeoff_mps=" << takeoff
                  << " apex_m=" << apex_m
                  << " apex_ft=" << (apex_m / 0.3048F)
                  << " landing_mps=" << landing
                  << " peak_force_n=" << peak_force
                  << " peak_power_w=" << peak_power
                  << " travel_m=" << travel
                  << " toe_feet=" << toe_feet
                  << " landed_feet=" << landed
                  << " hard_landing_mps=" << Matron::kHardLandingMps
                  << " alive=1\n";
        require(airborne && takeoff > 0.0F, "matron launch: the full stroke leaves the floor");
        require(takeoff <= budget.launch_dv_mps + 0.001F, "matron launch: toe-off does not exceed the earned limit");
        require(apex_m >= 1.83F && apex_m <= 2.13F, "matron launch: rise after toe-off is six to seven feet");
        require(peak_force <= budget.total_force_n + 0.5F, "matron launch: the push stays inside 8 kN");
        require(peak_power <= budget.power_w + 1.0F, "matron launch: the push stays inside the power cap");
        require(travel <= budget.leg_travel_m + 0.02F, "matron launch: the foot stays inside the stroke");
        require(landing + 0.05F >= takeoff, "matron launch: what went up comes back at least that fast");
        require(landing <= budget.catch_arrest_mps + 0.05F,
                "matron launch: she can arrest the return of a jump she is allowed to make");
        require(landing < Matron::kLethalLandingMps, "matron launch: the return is not a lethal landing");
        require(landing < Matron::kHardLandingMps, "matron launch: her own jump does not knock her down");
        require(std::abs(landed) < 0.15F, "matron launch: the missed full jump comes back to the floor she left");
        require(w.gravity_always, "matron launch: gravity stays on through the jump and the fall");
    }
    std::cout << "PASS scraperx_sim matron capped launches: force, power, stroke, reaction, support loss, cancellation\n";
}

void test_jump() {
    MoveSpec jump = ladder_up(6.0F, 3.4F, 5.8F);
    jump.kind = MoveKind::JumpCatch;
    {
        ClimbWorld w;
        w.deck(6.0F);
        w.rungs(3.4F, 5.8F);
        w.spawn(RVec3(0, 0, 1.0F));
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        climber.begin(jump);
        const auto st = w.run(climber, 40.0F);
        const float top = w.settle(1.0F);
        std::cout << "  matron climb jump: " << int(st) << " (" << traversal::failure_name(climber.failure()) << ") launches "
                  << climber.view().launches << " catches " << climber.view().catches << " feet " << top << "\n";
        require(st == Climber::Status::Done && climber.view().catches == 1 && std::abs(top - 6.0F) < 0.05F,
                "matron climb: a jump for a bar 3.4 m up, caught, and up the rungs over it");
    }
    // A miss: the bars are gone while she is in the air.
    {
        ClimbWorld w;
        w.deck(6.0F);
        std::vector<BodyID> bars = w.rungs(3.4F, 5.8F);
        w.spawn(RVec3(0, 0, 1.0F));
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        climber.begin(jump);
        float highest = 0.0F;
        const auto st = w.run(climber, 20.0F, [&](int) {
            if (climber.view().launches == 1 && w.physics.GetBodyInterface().IsAdded(bars.front())) {
                for (const BodyID b : bars) w.physics.GetBodyInterface().RemoveBody(b);
            }
            highest = std::max(highest, w.feet());
        });
        const float landed = w.settle();
        std::cout << "  matron climb jump miss: " << traversal::failure_name(climber.failure()) << " highest feet " << highest
                  << " landed " << landed << "\n";
        require(st == Climber::Status::Failed && climber.failure() == Failure::Missed && climber.view().misses == 1,
                "matron climb: nothing to catch at the top of the jump is a miss");
        require(highest > 0.6F && highest < 1.6F && std::abs(landed) < 0.05F, "matron climb: the missed jump falls back to the floor");
    }
    // Out of range: a bar higher than her legs can put her hands. The margin
    // stays past the declared take-off, so a longer stroke does not turn this
    // into a jump she is allowed to make.
    {
        const traversal::Limits budget;
        const float ballistic = budget.launch_dv_mps * budget.launch_dv_mps / (2.0F * 9.81F);
        const float too_high = budget.center_above_feet() + budget.hand_above_center + budget.leg_travel_m + ballistic + 0.80F;
        ClimbWorld w;
        w.deck(too_high + 1.5F);
        w.rungs(too_high, too_high + 0.6F);
        w.spawn(RVec3(0, 0, 1.0F));
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        MoveSpec far = ladder_up(too_high + 1.5F, too_high, too_high + 0.6F);
        far.kind = MoveKind::JumpCatch;
        climber.begin(far);
        const auto st = w.run(climber, 20.0F);
        require(st == Climber::Status::Failed && climber.failure() == Failure::OutOfRange && climber.view().launches == 0,
                "matron climb: a launch past her legs' limit is not made");
    }
    std::cout << "PASS scraperx_sim matron climb jump: catch, miss, out of range\n";
}

// Narrow support (an open question in the lab, settled by measurement, not
// asserted either way): a 0.30 m board 3 m up, 4 m long, under her 0.60 m
// body. She walks it with her own capped feet under a steady sideways push;
// the result says whether a board under her axis is support for her.
void test_narrow_board() {
    for (const float push : {0.0F, 0.1F, 0.2F, 0.5F}) {
        ClimbWorld w;
        w.box(Vec3(0.15F, 0.05F, 2.0F), RVec3(0, 2.95F, -2.0F));   // the board, top 3.0
        w.spawn(RVec3(0, 3.0F, -0.4F));
        (void)w.settle(0.5F);
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        MoveSpec walk;
        walk.kind = MoveKind::Walk;
        walk.start = Vec3(0, 3.0F, -0.4F);
        walk.goal = Vec3(0, 3.0F, -3.6F);
        walk.goal_top = 3.0F;
        climber.begin(walk);
        float drift = 0.0F;
        const auto st = w.run(climber, 12.0F, [&](int) {
            BodyInterface &b = w.physics.GetBodyInterface();
            b.AddForce(w.her, Vec3(Matron::kMassKg * push, 0, 0));
            drift = std::max(drift, std::abs(float(b.GetPosition(w.her).GetX())));
        });
        const float feet = w.feet();
        std::cout << "  matron narrow board, sideways " << push << " m/s^2: " << int(st) << " ("
                  << traversal::failure_name(climber.failure()) << ") feet " << feet << " drift " << drift << " m\n";
    }
}

void test_drops_and_mantle() {
    {
        // A hang-drop off a 3.5 m ledge.
        ClimbWorld w;
        w.deck(3.5F);
        w.spawn(RVec3(0, 3.5F, -2.0F));
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        MoveSpec drop;
        drop.kind = MoveKind::HangDrop;
        drop.start = Vec3(0, 3.5F, -1.4F);
        drop.goal = Vec3(0, 0, 0.0F);
        drop.goal_top = 0.0F;
        drop.out = Vec3(0, 0, 1);
        float fastest = 0.0F;
        const auto st = w.run(climber, 20.0F, [&](int) {
            fastest = std::min(fastest, w.physics.GetBodyInterface().GetLinearVelocity(w.her).GetY());
        });
        (void)st;
        climber.begin(drop);
        const auto st2 = w.run(climber, 20.0F, [&](int) {
            fastest = std::min(fastest, w.physics.GetBodyInterface().GetLinearVelocity(w.her).GetY());
        });
        const float landed = w.settle(1.0F);
        std::cout << "  matron climb hang-drop: " << int(st2) << " (" << traversal::failure_name(climber.failure()) << ") landed "
                  << landed << " fastest " << fastest << " m/s\n";
        // From a full hang her feet are 1.15 m over the floor: about 4.7 m/s, not the 8.3 of the whole 3.5 m.
        require(st2 == Climber::Status::Done && std::abs(landed) < 0.05F && fastest > -6.0F,
                "matron climb: hands on the lip, a full hang, then the drop to the floor");
    }
    {
        // A mantle onto a 1.6 m ledge, and the same with the ledge's top taken.
        for (int blocked = 0; blocked < 2; ++blocked) {
            ClimbWorld w;
            w.deck(1.6F);
            if (blocked) w.box(Vec3(1.5F, 1.0F, 0.8F), RVec3(0, 1.6F + 1.0F, -1.9F));   // a cabinet 0.1 m behind the lip (0.6 m is room enough to stand in now)
            w.spawn(RVec3(0, 0, 0.5F));
            Climber climber(w.physics, w.her, {w.her, BodyID()});
            MoveSpec mantle;
            mantle.kind = MoveKind::Mantle;
            mantle.start = Vec3(0, 0, -0.6F);
            mantle.goal = Vec3(0, 1.6F, -1.6F);
            mantle.goal_top = 1.6F;
            mantle.out = Vec3(0, 0, 1);
            climber.begin(mantle);
            float highest = 0.0F;
            const auto st = w.run(climber, 20.0F, [&](int) { highest = std::max(highest, w.feet()); });
            const float end = w.settle(1.0F);
            std::cout << "  matron climb mantle" << (blocked ? " (blocked)" : "") << ": " << int(st) << " ("
                      << traversal::failure_name(climber.failure()) << ") feet " << end << "\n";
            if (blocked) {
                require(st == Climber::Status::Failed &&
                            (climber.failure() == Failure::BlockedSweep || climber.failure() == Failure::NoHold) && highest < 0.1F,
                        "matron climb: a mantle her capsule does not fit is not tried");
            } else {
                require(st == Climber::Status::Done && std::abs(end - 1.6F) < 0.05F, "matron climb: a 1.6 m mantle");
            }
        }
    }
    std::cout << "PASS scraperx_sim matron climb drops and mantles\n";
}

void test_dynamic_support() {
    // A ladder hung on a spring: her weight on its rungs goes onto it at the
    // contact (it sinks), and comes off when she lets go.
    ClimbWorld w;
    StaticCompoundShapeSettings settings;
    settings.AddShape(Vec3(0, 0, -0.05F), Quat::sIdentity(), new BoxShape(Vec3(0.3F, 3.0F, 0.02F), 0.01F));
    for (int i = 0; i < 16; ++i) settings.AddShape(Vec3(0, -2.4F + 0.3F * float(i), 0.03F), Quat::sIdentity(), new BoxShape(Vec3(0.25F, 0.02F, 0.02F), 0.01F));
    BodyCreationSettings s(settings.Create().Get(), RVec3(0, 3.6F, -0.98F), Quat::sIdentity(), EMotionType::Dynamic, detail::object_layers::kMoving);
    s.mAllowedDOFs = EAllowedDOFs::TranslationY;
    s.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
    s.mMassPropertiesOverride.mMass = 80.0F;
    s.mAllowSleeping = false;
    const BodyID ladder = w.physics.GetBodyInterface().CreateAndAddBody(s, EActivation::Activate);
    w.bodies.push_back(ladder);
    DistanceConstraintSettings spring;
    spring.mPoint1 = RVec3(0, 12.0F, -0.98F);
    spring.mPoint2 = RVec3(0, 6.6F, -0.98F);
    spring.mMinDistance = spring.mMaxDistance = 5.4F;
    spring.mLimitsSpringSettings.mFrequency = 1.0F;
    spring.mLimitsSpringSettings.mDamping = 1.0F;
    Body *ladder_body = w.physics.GetBodyLockInterfaceNoLock().TryGetBody(ladder);
    w.physics.AddConstraint(spring.Create(Body::sFixedToWorld, *ladder_body));
    w.spawn(RVec3(0, 0, 1.0F));
    for (int i = 0; i < 270; ++i) w.physics.Update(ClimbWorld::dt, 1, &w.temp, &w.jobs);
    const float rest = float(w.physics.GetBodyInterface().GetPosition(ladder).GetY());
    Climber climber(w.physics, w.her, {w.her, BodyID()});
    MoveSpec up = ladder_up(30.0F, 1.2F, 5.7F);
    up.line = Vec3(0, 0, -0.95F);
    climber.begin(up);
    float lowest = rest;
    float reaction = 0.0F;
    w.run(climber, 6.0F, [&](int) {
        lowest = std::min(lowest, float(w.physics.GetBodyInterface().GetPosition(ladder).GetY()));
        reaction = std::max(reaction, climber.view().support_reaction_n);
    });
    std::cout << "  matron climb dynamic support: rest " << rest << " lowest " << lowest << " reaction " << reaction << " N\n";
    // 220 kg on a 1 Hz spring holding 80 kg sinks it about her weight's share.
    // (it sinks until its foot meets the floor, 0.35 m under its rest)
    require(reaction > 1500.0F && lowest < rest - 0.3F, "matron climb: her weight goes onto a dynamic hold at the contact");
    std::cout << "PASS scraperx_sim matron climb dynamic support: sink_m=" << rest - lowest << "\n";
}
// The shared power cap: work along her velocity is clamped, braking and
// sideways force are not, and force never exceeds what is left.
// Cory, 2026-10-27 15:50 CT: the player keeps at least 0.5 m of escape over
// her planned jump-catch. His reach is worked out here from his own constants
// (centre over feet, a standing jump's rise, the hang band's top), not read
// back from the limit under test.
void test_jump_catch_escape_margin() {
    namespace d = scraperx::sim::detail;
    const float jump_rise = d::kJumpSpeed * d::kJumpSpeed / (2.0F * 9.81F);
    const float player_reach = d::kPlayerHalfHeight + d::kPlayerRadius + jump_rise + d::kHangMaximumRiseAboveCentre;
    const traversal::Limits limits;
    std::printf("matron jump-catch: plans to %.2f m, player's jump-and-hang reaches %.2f m (margin %.2f m)\n",
                limits.jump_hold_max_m, player_reach, player_reach - limits.jump_hold_max_m);
    require(player_reach - limits.jump_hold_max_m >= 0.5F - 1.0e-4F,
            "matron jump-catch: her planned catch stays 0.5 m under the player's jump-and-hang reach");
}

void test_power_budget() {
    using traversal::Budget;
    const traversal::Limits limits;
    // Faster than force-cap / power, so the power cap binds before 8 kN does.
    const float speed = limits.power_w / limits.total_force_n + 2.5F;
    Budget b{limits.total_force_n, 0.0F, limits.power_w, 0.0F, Vec3(0, speed, 0)};
    const Vec3 up = b.take(Vec3(0, limits.total_force_n, 0));
    require(std::abs(up.GetY() - limits.power_w / speed) < 1.0F && b.power_left < 1.0F,
            "matron power: forward actuator force is limited by the power cap");
    const Vec3 side = b.take(Vec3(2000.0F, 0, 0));
    require(std::abs(side.GetX() - 2000.0F) < 1.0F, "matron power: force across her motion costs no power");
    const Vec3 brake = b.take(Vec3(0, -500.0F, 0));
    require(std::abs(brake.GetY() + 500.0F) < 1.0F, "matron power: braking is not power-limited");
    require(b.used <= limits.total_force_n + 1.0F, "matron power: force total still capped");
    Budget still{limits.total_force_n, 0.0F, limits.power_w, 0.0F, Vec3::sZero()};
    require(std::abs(still.take(Vec3(0, 5000.0F, 0)).GetY() - 5000.0F) < 1.0F, "matron power: at rest, force cap only");
    std::cout << "  matron power: " << limits.power_w << " W cap clamps along-velocity force (" << up.GetY()
              << " N at " << speed << " m/s); sideways/braking free\n";
}
// Gripping a moving bar: power is charged on her velocity relative to the
// bar, so holding still on a bar that carries her (a lift, a swinging boom)
// costs force but no power, while pulling up along a bar that falls away
// from her is charged for the speed she makes on it, not over the ground.
void test_power_moving_bar() {
    using traversal::Budget;
    const traversal::Limits limits;
    const float weight = Matron::kMassKg * 9.81F;
    // Fast enough that charging this power cap over the ground would cut a hold under her weight.
    const float bar_mps = limits.power_w / (weight - 300.0F) + 4.0F;
    const Vec3 bar(0, bar_mps, 0);
    // Carried by it, still on it: her whole weight held, no power spent.
    Budget carried{limits.total_force_n, 0.0F, limits.power_w, 0.0F, bar - bar};
    const Vec3 hold = carried.take(Vec3(0, weight, 0));
    require(std::abs(hold.GetY() - weight) < 1.0F && carried.power_used < 1.0F,
            "matron power: holding still on a bar that carries her costs no power");
    // Charged over the ground instead, the same hold would be cut below her weight:
    // the cap would drop her off any moving support.
    Budget ground{limits.total_force_n, 0.0F, limits.power_w, 0.0F, bar};
    require(ground.take(Vec3(0, weight, 0)).GetY() < weight - 300.0F,
            "matron power: (control) charged over the ground the hold would be cut");
    // Climbing it, relative speed high enough that power binds before 8 kN.
    const float climb_mps = limits.power_w / limits.total_force_n + 2.5F;
    Budget climbing{limits.total_force_n, 0.0F, limits.power_w, 0.0F, Vec3(0, climb_mps, 0)};
    const Vec3 pull = climbing.take(Vec3(0, limits.total_force_n, 0));
    require(std::abs(pull.GetY() - limits.power_w / climb_mps) < 1.0F && climbing.power_used <= limits.power_w + 1.0F,
            "matron power: climbing a moving bar charges positive power on relative speed");
    // A bar dropping away under her hands: braking against it is free.
    Budget falling{limits.total_force_n, 0.0F, limits.power_w, 0.0F, Vec3(0, 0, 0) - Vec3(0, -3.0F, 0)};
    const Vec3 brake = falling.take(Vec3(0, -2000.0F, 0));
    require(std::abs(brake.GetY() + 2000.0F) < 1.0F && falling.power_used < 1.0F,
            "matron power: slowing herself against a falling bar is not power-limited");
    std::cout << "PASS scraperx_sim matron power moving bar: hold_n=" << hold.GetY() << " climb_n=" << pull.GetY() << "\n";
}

// A mantle onto a ledge that is lifted under her hands (a deck on a hoist):
// the braking rule. Power is charged only on positive work relative to the
// hold; when the lip jerks up away from her she brakes against it (force up,
// her velocity relative to it down), and that is capped by force alone. The
// jerk is fast enough that 8 kN against it is more watts than the positive
// power cap. That work stays bounded by force alone, which the power cap
// would have refused. She keeps her grip, arrives standing on the moving
// deck, and her positive power never exceeds the shared cap.
void test_mantle_moving_ledge() {
    ClimbWorld w;
    const BodyID deck = w.box(Vec3(5, 0.8F, 3), RVec3(0, 0.8F, -4), false, EMotionType::Kinematic);
    w.spawn(RVec3(0, 0, 0.5F));
    Climber climber(w.physics, w.her, {w.her, BodyID()});
    MoveSpec mantle;
    mantle.kind = MoveKind::Mantle;
    mantle.start = Vec3(0, 0, -0.6F);
    mantle.goal = Vec3(0, 1.6F, -1.6F);
    mantle.goal_top = 1.6F;
    mantle.out = Vec3(0, 0, 1);
    climber.begin(mantle);
    BodyInterface &bodies = w.physics.GetBodyInterface();
    int lift_ticks = -1;
    float peak_power = 0.0F;
    float peak_force = 0.0F;
    float peak_brake = 0.0F;
    bool held_through = true;
    const auto st = w.run(climber, 20.0F, [&](int) {
        if (lift_ticks < 0 && climber.view().holding && climber.phase() >= 3) lift_ticks = 0;
        if (lift_ticks >= 0) {
            ++lift_ticks;
            const bool lifting = lift_ticks <= 18;   // 0.2 s at 6.2 m/s: past the positive power cap at 8 kN
            bodies.SetLinearVelocity(deck, Vec3(0, lifting ? 6.2F : 0.0F, 0));
            if (lifting && !climber.view().holding) held_through = false;
        }
        peak_power = std::max(peak_power, climber.view().power_w);
        peak_force = std::max(peak_force, climber.view().force_n);
        peak_brake = std::max(peak_brake, climber.view().peak_brake_w);
    });
    bodies.SetLinearVelocity(deck, Vec3::sZero());
    const float deck_top = float(bodies.GetPosition(deck).GetY()) + 0.8F;
    const float end = w.settle(1.0F);
    std::cout << "  matron climb mantle on a lifted ledge: " << int(st) << " (" << traversal::failure_name(climber.failure())
              << ") lifted " << (lift_ticks >= 0) << " deck top " << deck_top << " feet " << end << " peak power " << peak_power
              << " W peak brake " << peak_brake << " W peak force " << peak_force << " N\n";
    require(lift_ticks > 18, "matron climb moving ledge: the deck was lifted while she held its lip");
    require(held_through, "matron climb moving ledge: she keeps her grip while the lip jerks up");
    require(peak_power <= climber.limits().power_w + 1.0F &&
                peak_force <= climber.limits().total_force_n + 1.0F,
            "matron climb moving ledge: positive power and force within her shared Budget");
    require(peak_brake > climber.limits().power_w,
            "matron climb moving ledge: braking against a lifted lip can exceed positive-power cap (force-capped only)");
    require(st == Climber::Status::Done && std::abs(end - deck_top) < 0.06F,
            "matron climb moving ledge: she arrives standing on the deck that moved");
    std::cout << "PASS scraperx_sim matron climb moving ledge: brake_w=" << peak_brake << " power_w=" << peak_power << "\n";
}

// The player bumps her partway down a ladder she has never seen (seeded
// height, pitch and rung length): an impulse out from the face while she is
// lowering hand under hand. She recovers on her general moves alone: her
// hands catch and keep the rungs and she finishes the lower, or, the hold
// lost, she drops in control onto what is under her. Never snapped (no tick
// moves her more than 0.20 m), gravity always on, alive, on her feet.
void test_bump_mid_lower() {
    std::uint32_t rng = 0x9E3779B9U;
    const auto uniform = [&](const float lo, const float hi) {
        rng = rng * 1664525U + 1013904223U;
        return lo + (hi - lo) * float(rng >> 8) / float(1U << 24);
    };
    int regripped = 0;
    int dropped = 0;
    for (int run = 0; run < 6; ++run) {
        const float height = uniform(4.0F, 7.5F);
        const float pitch = uniform(0.25F, 0.40F);
        const float bump_mps = run < 3 ? uniform(0.8F, 2.2F) : uniform(3.0F, 5.0F);   // a nudge, then a hard shove
        const float side = uniform(-0.6F, 0.6F);
        ClimbWorld w;
        w.deck(height);
        w.rungs(0.3F, height - 0.1F, pitch);
        w.spawn(RVec3(0, height, -2.0F));
        w.settle(0.5F);
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        MoveSpec down = ladder_up(height, 0.3F, height - 0.1F);
        down.kind = MoveKind::RungDown;
        down.start = Vec3(0, height, -1.35F);
        down.goal = Vec3(0, 0, -0.5F);
        down.goal_top = 0.0F;
        climber.begin(down);
        BodyInterface &bodies = w.physics.GetBodyInterface();
        bool bumped = false;
        RVec3 last = w.at();
        float worst_step = 0.0F;
        float fastest_fall = 0.0F;
        float pushed_out = 0.0F;   // how far off the face the bump took her
        const auto st = w.run(climber, 60.0F, [&](int) {
            if (!bumped && climber.view().holding && climber.phase() >= 2 && w.feet() < 0.6F * height) {
                bodies.AddImpulse(w.her, Vec3(side, 0.0F, 1.0F).Normalized() * (Matron::kMassKg * bump_mps));
                bumped = true;
            }
            const RVec3 now = w.at();
            worst_step = std::max(worst_step, float((now - last).Length()));
            last = now;
            fastest_fall = std::min(fastest_fall, bodies.GetLinearVelocity(w.her).GetY());
            if (bumped) pushed_out = std::max(pushed_out, float(now.GetZ()) + 0.5F);
        });
        // Whatever the move ended in, she stands up where she is.
        for (int i = 0; i < 270; ++i) {
            w.physics.Update(ClimbWorld::dt, 1, &w.temp, &w.jobs);
            const RVec3 now = w.at();
            worst_step = std::max(worst_step, float((now - last).Length()));
            last = now;
            fastest_fall = std::min(fastest_fall, bodies.GetLinearVelocity(w.her).GetY());
        }
        const float feet = w.feet();
        const bool kept = st == Climber::Status::Done;
        regripped += kept ? 1 : 0;
        dropped += kept ? 0 : 1;
        std::cout << "  matron bump mid-lower run " << run << ": height " << height << " pitch " << pitch << " bump " << bump_mps
                  << " m/s, out " << pushed_out << " m -> " << (kept ? "kept her hold, lowered to the floor" : traversal::failure_name(climber.failure()))
                  << ", feet " << feet << ", worst step " << worst_step << " m, fastest fall " << fastest_fall << " m/s\n";
        require(bumped && pushed_out > 0.0F, "matron bump mid-lower: the bump came while she was lowering, and moved her");
        require(worst_step <= 0.20F, "matron bump mid-lower: no tick moves her more than 0.20 m (no snapping)");
        require(w.gravity_always, "matron bump mid-lower: gravity always on");
        require(std::abs(feet) < 0.06F, "matron bump mid-lower: she ends on her feet on the floor below");
        // A controlled drop: no worse than a fall from her full hang off the
        // lowest rung she could have held (the 12 m lethal fall is 15.3 m/s).
        require(fastest_fall > -8.0F, "matron bump mid-lower: a lost hold is a controlled drop, never a free fall from height");
    }
    std::cout << "PASS scraperx_sim matron bump mid-lower: kept " << regripped << " dropped " << dropped << " of 6\n";
}

// A wall standing across a step (ledge exam seed 35's shape, built here
// from plain boxes): the step's top is cut into a sliver in front of the
// wall and a 0.7 m strip behind it. Her scan finds the strip she fits on,
// she plans up onto it, and her walk to its side goes round the wall's end,
// never through its body. (Geometry only: nothing names this stack.)
void test_wall_across_step() {
    ClimbWorld w;
    w.box(Vec3(1.5F, 0.72F, 1.53F), RVec3(0, 0.72F, -1.53F));    // step 0: top 1.44, front z -3.06
    w.box(Vec3(1.5F, 1.44F, 1.02F), RVec3(0, 1.44F, -1.02F));    // step 1: top 2.88, front z -2.04
    w.box(Vec3(2.6F, 1.5F, 0.1F), RVec3(-0.83F, 1.5F, -2.83F));  // the wall: 3 m, x -3.43..1.77
    w.spawn(RVec3(-2.0F, 0, -6.0F));
    w.settle(0.3F);
    const traversal::Ignore ignore{w.her, BodyID()};
    traversal::AffordanceMap map;
    map.scan(w.physics, AABox(Vec3(-8, -1, -9), Vec3(8, 5, 4)), ignore);
    bool strip = false;
    for (const auto &r : map.regions()) strip = strip || std::abs(r.top - 1.44F) < 0.02F;
    const int floor = map.region_at(Vec3(-2.0F, 0, -6.0F));
    const int top = map.region_at(Vec3(0, 2.88F, -1.0F));
    const std::vector<int> plan = map.plan(floor, top);
    const std::vector<Vec3> path = traversal::grid_walk(w.physics, ignore, traversal::Limits{}, Vec3(-2.0F, 0, -6.0F), Vec3(-1.9F, 0, -2.36F));
    float west = 0.0F;
    for (const Vec3 &q : path) west = std::min(west, q.GetX());
    std::cout << "  matron climb wall across a step: strip region " << strip << " plan " << plan.size() << " edges, walk "
              << path.size() << " turns, farthest west " << west << "\n";
    require(strip, "matron scan: a step cut by a wall standing on it is walkable where she fits beside the wall");
    require(plan.size() >= 2, "matron scan: she plans up through the strip behind the wall");
    require(!path.empty() && west < -3.43F, "matron walk: round the wall's end, not through its body");
    std::cout << "PASS scraperx_sim matron climb wall across a step\n";
}

// Gaps of missing rungs on panels she has never seen (seeded height, pitch
// and gap): her scan joins the rungs across a gap she can cross and no
// wider; up, she crosses it with a long reach or a throw (a dyno) under the
// one budget, with real misses (a short throw falls back past the rung she
// left and snatches it, or she falls); down, from a full hang her other hand
// reaches past the gap. Nothing here names a panel: the same skill on each.
void test_rung_gaps() {
    std::uint32_t rng = 0x2545F491U;
    const auto uniform = [&](const float lo, const float hi) {
        rng = rng * 1664525U + 1013904223U;
        return lo + (hi - lo) * float(rng >> 8) / float(1U << 24);
    };
    const traversal::Limits limits{};
    const float widest = limits.hand_above_center - 0.35F + limits.dyno_rise_m;
    int crossed_up = 0, crossed_down = 0, joined = 0, refused = 0, throws = 0, caught = 0, missed = 0, runs = 0;
    for (int run = 0; run < 10; ++run) {
        const float height = uniform(5.0F, 7.0F);
        const float pitch = uniform(0.28F, 0.42F);
        const int missing = 1 + static_cast<int>(uniform(0.0F, 3.0F));   // 1 to 3 rungs gone
        const float gap = float(missing + 1) * pitch;
        const float gap_from = 0.3F + pitch * float(static_cast<int>(uniform(2.0F, 5.0F)));
        ClimbWorld w;
        w.deck(height);
        const float top_rung = height - 0.1F;
        w.rungs(0.3F, gap_from, pitch);
        w.rungs(gap_from + gap, top_rung, pitch);
        w.spawn(RVec3(0, 0, 1.0F));
        w.settle(0.3F);
        const traversal::Ignore ignore{w.her, BodyID()};
        traversal::AffordanceMap map;
        map.scan(w.physics, AABox(Vec3(-6, -1, -8), Vec3(6, height + 3.0F, 4)), ignore);
        bool spans = false;
        for (const auto &l : map.ladders()) spans = spans || (l.bottom < 0.5F && l.top > top_rung - 0.5F);
        const bool crossable = gap <= widest;
        std::cout << "  matron rung gap run " << run << ": height " << height << " pitch " << pitch << " gap " << gap
                  << " m (" << missing << " missing) scan " << (spans ? "joins it" : "splits it");
        require(spans == crossable, "matron rung gap: the scan joins rungs across a gap she can cross, and only those");
        if (!spans) {
            ++refused;
            std::cout << "\n";
            continue;
        }
        ++joined;
        ++runs;
        Climber climber(w.physics, w.her, ignore);
        float l_top = 0.0F;
        float l_bottom = 0.0F;
        for (const auto &l : map.ladders()) if (l.bottom < 0.5F) { l_top = l.top; l_bottom = l.bottom; }
        climber.begin(ladder_up(height, l_bottom, l_top));
        RVec3 last = w.at();
        float worst = 0.0F;
        const auto track = [&](int) {
            const RVec3 now = w.at();
            worst = std::max(worst, float((now - last).Length()));
            last = now;
        };
        const auto up = w.run(climber, 60.0F, track);
        const float top = w.settle(1.0F);
        throws += int(climber.view().launches);
        caught += int(climber.view().catches);
        missed += int(climber.view().misses);
        std::cout << "; up " << (up == Climber::Status::Done ? "done" : traversal::failure_name(climber.failure())) << " launches "
                  << climber.view().launches << " catches " << climber.view().catches << " misses " << climber.view().misses
                  << " peak " << climber.view().peak_force_n << " N " << climber.view().peak_power_w << " W";
        if (up != Climber::Status::Done) {
            std::cout << " phase " << climber.view().phase << " grips " << climber.view().grips
                      << " releases " << climber.view().releases << " landed " << top;
        }
        require(worst <= 0.20F, "matron rung gap: no tick moves her more than 0.20 m");
        require(climber.view().peak_power_w <= limits.power_w + 1.0F, "matron rung gap: power within the shared cap");
        if (up != Climber::Status::Done) {
            // A real miss: she fell, and is on her feet on the floor.
            require(climber.view().misses > 0 && std::abs(top) < 0.06F, "matron rung gap: a failed crossing is a real miss, and she lands");
            std::cout << "\n";
            continue;
        }
        ++crossed_up;
        require(std::abs(top - height) < 0.06F, "matron rung gap: up across the gap onto the deck");
        MoveSpec down = ladder_up(height, l_bottom, l_top);
        down.kind = MoveKind::RungDown;
        down.start = Vec3(0, height, -1.35F);
        down.goal = Vec3(0, 0, -0.5F);
        down.goal_top = 0.0F;
        climber.begin(down);
        const auto dn = w.run(climber, 60.0F, track);
        const float below = w.settle(1.0F);
        std::cout << "; down " << (dn == Climber::Status::Done ? "done" : traversal::failure_name(climber.failure())) << ", worst step "
                  << worst << " m\n";
        require(worst <= 0.20F, "matron rung gap: no tick moves her more than 0.20 m (down)");
        require(dn == Climber::Status::Done && std::abs(below) < 0.06F, "matron rung gap: down past the gap to the floor");
        ++crossed_down;
    }
    // A real miss: high on a ladder, a gap at the edge of what she crosses,
    // and a third of the Matron's power: the throw comes up short, she
    // falls back past the rung she left and snatches at it; whatever her
    // hands keep or lose, she ends on her feet on the floor, never snapped.
    {
        ClimbWorld w;
        w.deck(6.0F);
        w.rungs(0.3F, 3.0F, 0.3F);
        w.rungs(4.45F, 5.95F, 0.3F);
        w.spawn(RVec3(0, 0, 1.0F));
        traversal::Limits weak;
        weak.power_w = 3000.0F;
        Climber climber(w.physics, w.her, {w.her, BodyID()}, weak);
        climber.begin(ladder_up(6.0F, 0.3F, 5.95F));
        RVec3 last = w.at();
        float worst = 0.0F;
        const auto st = w.run(climber, 60.0F, [&](int) {
            const RVec3 now = w.at();
            worst = std::max(worst, float((now - last).Length()));
            last = now;
        });
        const float landed = w.settle(2.0F);
        std::cout << "  matron rung gap, short throw: " << traversal::failure_name(climber.failure()) << " launches "
                  << climber.view().launches << " catches " << climber.view().catches << " misses " << climber.view().misses
                  << " landed " << landed << " worst step " << worst << "\n";
        require(st == Climber::Status::Failed && climber.view().launches >= 1 && climber.view().misses >= 1,
                "matron rung gap: a throw her arms cannot make is a real miss");
        require(std::abs(landed) < 0.06F && worst <= 0.20F, "matron rung gap: after the miss she lands on her feet, never snapped");
    }
    std::cout << "PASS scraperx_sim matron rung gaps: joined " << joined << " refused " << refused << " up " << crossed_up << "/" << runs
              << " down " << crossed_down << " throws " << throws << " catches " << caught << " misses " << missed << "\n";
}

// A pole standing just off a deck, its head 0.8 m over the lip, between
// the posts of a gap in the deck's rail (the shape of exam seeds 57, 77,
// 81, 83 and 96). Her body is 0.60 m across. With the posts 0.95 m apart
// the clearance between pole and post is 0.57 m: there is no way for her
// body from the pole onto the deck, so her scan refuses the pole, for that
// reason, both ways. With them 1.30 m apart (0.71 m clearance) the same
// pole is usable and she climbs it onto the deck.
void test_pole_head_clearance() {
    for (const float opening : {0.95F, 1.30F}) {
        ClimbWorld w;
        w.box(Vec3(2.5F, 2.0F, 2.0F), RVec3(0, 2.0F, 2.4F));            // the deck: top 4.0, face at z 0.4
        w.box(Vec3(0.05F, 2.4F, 0.05F), RVec3(0, 2.4F, 0));             // the pole: head 4.8
        for (const float side : {-1.0F, 1.0F}) {
            w.box(Vec3(0.03F, 0.5F, 0.03F), RVec3(side * 0.5F * opening, 4.5F, 0.45F));   // the rail's posts
            w.box(Vec3(0.5F * (2.5F - 0.5F * opening), 0.03F, 0.03F),
                  RVec3(side * (0.5F * opening + 0.5F * (2.5F - 0.5F * opening)), 5.0F, 0.45F));
        }
        w.spawn(RVec3(0, 0, -2.0F));
        w.settle(0.3F);
        const traversal::Ignore ignore{w.her, BodyID()};
        traversal::AffordanceMap map;
        map.scan(w.physics, AABox(Vec3(-5, -1, -5), Vec3(5, 7, 5)), ignore);
        const traversal::Edge *up = nullptr;
        const traversal::Edge *down = nullptr;
        for (const auto &e : map.edges()) {
            if (e.kind == MoveKind::PoleUp) up = &e;
            if (e.kind == MoveKind::PoleDown) down = &e;
        }
        require(up != nullptr && down != nullptr, "matron pole head: the pole is found, up and down");
        std::cout << "  matron pole head clearance, posts " << opening << " m apart: up usable " << up->usable << " (" << up->reason
                  << ")\n";
        if (opening < 1.0F) {
            require(!up->usable && !down->usable && up->reason.find("past the pole's head") != std::string::npos,
                    "matron pole head: no room for her body past the pole's head, refused both ways for that reason");
            continue;
        }
        require(up->usable && down->usable, "matron pole head: with room past its head the pole is usable");
        Climber climber(w.physics, w.her, ignore);
        climber.begin(up->spec);
        const auto st = w.run(climber, 40.0F);
        const float top = w.settle(1.0F);
        std::cout << "  matron pole head clearance, wide: up " << int(st) << " (" << traversal::failure_name(climber.failure())
                  << ") feet " << top << "\n";
        require(st == Climber::Status::Done && std::abs(top - 4.0F) < 0.06F, "matron pole head: up the pole, round its head, onto the deck");
    }
    std::cout << "PASS scraperx_sim matron pole head clearance\n";
}

// Narrow ledges she has never seen (seeded): a board 0.20 to 0.40 m wide
// over a 6 m drop, from one deck to another a step higher or lower, open
// both sides or along a wall. Her scan joins the decks across it; her walk
// finds it, steps onto it, shuffles along it balanced over it, and steps
// off onto the far deck. Nothing names a board: a narrow floor is a floor.
struct NarrowRun {
    bool joined = false;
    bool arrived = false;
    float worst_step = 0.0F;
    float lowest = 0.0F;
    float peak_force = 0.0F;
};
NarrowRun narrow_ledge_run(const float width, const float rise, const float length, const bool wall, const float shove_mps,
                           const std::uint32_t seed) {
    (void)seed;
    ClimbWorld w;
    const float base = 6.0F;   // the decks stand 6 m over the floor
    w.box(Vec3(2.0F, 0.5F * base, 2.0F), RVec3(0, 0.5F * base, -2.0F));                       // deck A: z -4..0, top 6
    const float far = length;
    w.box(Vec3(2.0F, 0.5F * (base + rise), 2.0F), RVec3(0, 0.5F * (base + rise), far + 2.0F));  // deck B: from z = length
    const float board_top = base + 0.5F * rise;
    w.box(Vec3(0.5F * width, 0.1F, 0.5F * far + 0.1F), RVec3(0.4F, board_top - 0.1F, 0.5F * far));
    if (wall) w.box(Vec3(0.1F, 2.0F, 0.5F * far), RVec3(0.4F + 0.5F * width + 0.1F, board_top + 2.0F, 0.5F * far));
    w.spawn(RVec3(0.4F, base, -1.0F));
    w.settle(0.3F);
    const traversal::Ignore ignore{w.her, BodyID()};
    traversal::AffordanceMap map;
    map.scan(w.physics, AABox(Vec3(-4, -1, -6), Vec3(4, base + 4.0F, far + 6.0F)), ignore);
    NarrowRun r;
    const Vec3 goal(0.4F, base + rise, far + 1.2F);
    r.joined = map.region_at(Vec3(0.4F, base, -1.0F)) >= 0 && map.region_at(Vec3(0.4F, base, -1.0F)) == map.region_at(goal);
    Climber climber(w.physics, w.her, ignore);
    std::vector<Vec3> path = traversal::grid_walk(w.physics, ignore, climber.limits(), Vec3(0.4F, base, -1.0F), goal);
    if (path.empty()) return r;
    std::size_t wp = 0;
    RVec3 last = w.at();
    r.lowest = w.feet();
    bool shoved = false;
    for (int i = 0; i < 90 * 40; ++i) {
        const RVec3 at = w.at();
        const Vec3 f(float(at.GetX()), w.feet(), float(at.GetZ()));
        while (wp + 1 < path.size() && Vec3(path[wp] - f).GetX() * Vec3(path[wp] - f).GetX() +
                                               Vec3(path[wp] - f).GetZ() * Vec3(path[wp] - f).GetZ() < 0.3F * 0.3F) ++wp;
        r.peak_force = std::max(r.peak_force, climber.walk_between(ClimbWorld::dt, path[wp], 1.2F));
        if (!shoved && shove_mps > 0.0F && float(at.GetZ()) > 0.5F * far) {
            w.physics.GetBodyInterface().AddImpulse(w.her, Vec3(-1.0F, 0, 0) * (Matron::kMassKg * shove_mps));
            shoved = true;
        }
        w.physics.Update(ClimbWorld::dt, 1, &w.temp, &w.jobs);
        const RVec3 now = w.at();
        r.worst_step = std::max(r.worst_step, float((now - last).Length()));
        last = now;
        r.lowest = std::min(r.lowest, w.feet());
        if (float(now.GetZ()) > far + 0.8F && std::abs(w.feet() - (base + rise)) < 0.06F) {
            r.arrived = true;
            break;
        }
        if (w.feet() < 1.0F) break;   // fell off
    }
    return r;
}

void test_narrow_ledges() {
    std::uint32_t rng = 0x6C8E9CF5U;
    const auto uniform = [&](const float lo, const float hi) {
        rng = rng * 1664525U + 1013904223U;
        return lo + (hi - lo) * float(rng >> 8) / float(1U << 24);
    };
    int crossed = 0;
    for (int run = 0; run < 8; ++run) {
        const float width = 0.20F + 0.20F * float(run) / 7.0F + uniform(-0.01F, 0.01F);   // across 0.20 to 0.40
        const float rise = uniform(-0.30F, 0.30F);
        const float length = uniform(1.5F, 3.0F);
        const bool wall = run % 2 == 1;
        const NarrowRun r = narrow_ledge_run(width, rise, length, wall, 0.0F, rng);
        std::cout << "  matron narrow ledge run " << run << ": width " << width << " rise " << rise << " length " << length
                  << (wall ? " along a wall" : " open") << ": joined " << r.joined << " arrived " << r.arrived << " lowest "
                  << r.lowest << " worst step " << r.worst_step << " peak " << r.peak_force << " N\n";
        require(r.joined, "matron narrow ledge: her scan joins the decks across the narrow ledge");
        require(r.arrived && r.lowest > 5.5F, "matron narrow ledge: along it and onto the far deck, never off it");
        require(r.worst_step <= 0.20F && r.peak_force <= traversal::Limits{}.total_force_n + 1.0F,
                "matron narrow ledge: no snapping, within her budget");
        ++crossed;
    }
    // A real slip: on the narrowest ledge, open both sides, a hard shove
    // across it. Her balance is force-capped: she goes off and falls (alive
    // or not is the fall's business), never pulled back by magic.
    const NarrowRun slip = narrow_ledge_run(0.20F, 0.0F, 3.0F, false, 3.0F, 1U);
    std::cout << "  matron narrow ledge shove: arrived " << slip.arrived << " lowest " << slip.lowest << "\n";
    require(!slip.arrived && slip.lowest < 5.0F, "matron narrow ledge: a shove past her balance is a real slip off the ledge");
    const NarrowRun nudge = narrow_ledge_run(0.30F, 0.0F, 3.0F, false, 0.4F, 2U);
    std::cout << "  matron narrow ledge nudge: arrived " << nudge.arrived << " lowest " << nudge.lowest << "\n";
    require(nudge.arrived && nudge.lowest > 5.5F, "matron narrow ledge: a small nudge she balances out and goes on");
    // Onto a narrow ledge from the floor: a shelf 0.20 to 0.40 m deep on a
    // wall, 1.4 to 2.0 m up. Her body cannot stand square on it (the wall);
    // she mantles onto it leaning off the wall, her soles on the shelf.
    for (int run = 0; run < 4; ++run) {
        const float depth = 0.20F + 0.066F * float(run);
        const float h = uniform(1.4F, 2.0F);
        ClimbWorld w;
        w.box(Vec3(2.0F, 2.5F, 0.2F), RVec3(0, 2.5F, -0.2F - depth));            // the wall: face at z = -depth
        w.box(Vec3(1.5F, 0.1F, 0.5F * depth), RVec3(0, h - 0.1F, -0.5F * depth));   // the shelf: z -depth..0, top h
        w.spawn(RVec3(0, 0, 1.0F));
        w.settle(0.3F);
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        MoveSpec m;
        m.kind = MoveKind::Mantle;
        m.start = Vec3(0, 0, 0.45F);
        m.goal = Vec3(0, h, -0.5F * depth);
        m.goal_top = h;
        m.out = Vec3(0, 0, 1);
        climber.begin(m);
        RVec3 last = w.at();
        float worst = 0.0F;
        const auto st = w.run(climber, 20.0F, [&](int) {
            const RVec3 now = w.at();
            worst = std::max(worst, float((now - last).Length()));
            last = now;
        });
        // Standing there, the move done and nothing else asked of her (no
        // walk): her balance alone keeps her on it, 3 s.
        for (int i = 0; i < 270; ++i) {
            (void)climber.step(ClimbWorld::dt);
            w.physics.Update(ClimbWorld::dt, 1, &w.temp, &w.jobs);
        }
        const float feet = w.feet();
        std::cout << "  matron narrow shelf mantle: depth " << depth << " h " << h << " -> " << int(st) << " ("
                  << traversal::failure_name(climber.failure()) << ") feet " << feet << " worst step " << worst << "\n";
        require(st == Climber::Status::Done && std::abs(feet - h) < 0.06F && worst <= 0.20F,
                "matron narrow ledge: she mantles onto a narrow shelf and stays on it");
    }
    std::cout << "PASS scraperx_sim matron narrow ledges: crossed " << crossed << " of 8\n";
}

} // namespace


// Knocked off a deck partway through a journey onto a narrow board below
// its side (0.25-0.40 m, seeded, never seen): she lands on it, keeps her
// balance on it, plans again from the board and reaches her goal, up (a
// ladder, then a mantle) and down (a drop, then down to the floor). The
// executor is a journey's: plan, walk to a move's start, make the move,
// and plan again from wherever she stands when she is off her plan.
struct KnockRun {
    bool knocked = false, on_board = false, arrived = false;
    int replans = 0;
    std::vector<double> replan_us;   // each replan's cost, in the tick it ran
    float worst_step = 0.0F, peak_force = 0.0F;
};
KnockRun knock_run(const bool up, const float deck_h, const float upper, const float width, const float drop,
                   const bool wall, const float shove_mps) {
    ClimbWorld w;
    w.box(Vec3(0.9F, 0.5F * deck_h, 1.5F), RVec3(0, 0.5F * deck_h, -2.5F));                        // deck A: z -4..-1
    w.box(Vec3(0.9F, 0.5F * (deck_h + upper), 1.5F), RVec3(0, 0.5F * (deck_h + upper), -5.5F));    // deck U behind it
    w.rungs(0.3F, deck_h - 0.1F, 0.3F);                                                             // up A's face
    const float board_top = deck_h - drop;
    // Room between the deck's side and the wall beyond the board for her
    // body to come down onto it (0.62 m, wider than she is).
    const float gap = std::max(0.03F, 0.62F - width);
    const float bx = 0.9F + gap + 0.5F * width;
    w.box(Vec3(0.5F * width, 0.1F, 1.5F), RVec3(bx, board_top - 0.1F, -2.5F));                     // the board along A's side
    if (wall) w.box(Vec3(0.1F, 1.2F, 1.5F), RVec3(bx + 0.5F * width + 0.1F, board_top + 1.2F, -2.5F));
    const Vec3 start = up ? Vec3(0, 0, 1.0F) : Vec3(0, deck_h + upper, -5.5F);
    const Vec3 goal = up ? Vec3(0, deck_h + upper, -5.8F) : Vec3(0, 0, 1.5F);
    w.spawn(RVec3(start));
    w.settle(0.3F);
    const traversal::Ignore ignore{w.her, BodyID()};
    traversal::AffordanceMap map;
    map.scan(w.physics, AABox(Vec3(-5, -1, -9), Vec3(5, deck_h + upper + 3.0F, 4)), ignore);
    Climber climber(w.physics, w.her, ignore);
    const auto feet = [&] { const RVec3 a = w.at(); return Vec3(float(a.GetX()), w.feet(), float(a.GetZ())); };
    const int goal_region = map.region_at(goal);
    const int board_region = map.region_at(Vec3(bx, board_top, -2.5F));
    KnockRun r;
    std::vector<int> plan = map.plan(map.region_at(feet()), goal_region);
    if (plan.empty() || board_region < 0 || board_region == map.region_at(Vec3(0, deck_h, -2.5F))) return r;
    std::size_t next = 0, wp = 0;
    std::vector<Vec3> path;
    int region = map.region_at(feet());
    const auto walk_to = [&](const Vec3 target) {
        path = traversal::grid_walk(w.physics, ignore, climber.limits(), feet(), target);
        if (path.empty()) path.push_back(target);
        wp = 0;
    };
    enum { Walk, Move, Final } mode = Walk;
    walk_to(map.edges()[std::size_t(plan[0])].spec.start);
    float mode_s = 0.0F;
    RVec3 last = w.at();
    const auto replan = [&](const int from) {
        const auto t0 = std::chrono::steady_clock::now();
        plan = map.plan(from, goal_region);
        next = 0;
        if (!plan.empty()) walk_to(map.edges()[std::size_t(plan[0])].spec.start);
        r.replan_us.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count());
        ++r.replans;
        region = from;
        mode = Walk;
        mode_s = 0.0F;
    };
    for (int i = 0; i < 90 * 120; ++i) {
        mode_s += ClimbWorld::dt;
        const Vec3 f = feet();
        float top = 0.0F;
        const int under = climber.supported(&top) ? map.region_at(Vec3(f.GetX(), top, f.GetZ())) : -1;
        // The knock: on deck A, partway along her way, a shove off its side.
        if (!r.knocked && std::abs(f.GetY() - deck_h) < 0.1F && f.GetZ() < -1.6F && f.GetZ() > -3.4F && mode == Walk) {
            w.physics.GetBodyInterface().AddImpulse(w.her, Vec3(1.0F, 0, 0) * (Matron::kMassKg * shove_mps));
            r.knocked = true;
        }
        if (r.knocked && under == board_region) r.on_board = true;
        if (mode == Walk || mode == Final) {
            // Off her plan, standing on somewhere else: plan from there.
            if (under >= 0 && under != region && mode == Walk) {
                if (under == goal_region) { mode = Final; walk_to(goal); }
                else { replan(under); if (plan.empty()) break; }
            }
            while (wp + 1 < path.size() && Vec3(path[wp] - f).GetX() * Vec3(path[wp] - f).GetX() +
                                                   Vec3(path[wp] - f).GetZ() * Vec3(path[wp] - f).GetZ() < 0.35F * 0.35F) ++wp;
            (void)climber.walk_between(ClimbWorld::dt, path[wp], 1.2F);
            const Vec3 d = path.back() - f;
            const float left = std::sqrt(d.GetX() * d.GetX() + d.GetZ() * d.GetZ());
            if (mode == Walk && under == region && ((wp + 1 == path.size() && left < 0.6F) || mode_s > 25.0F)) {
                climber.begin(map.edges()[std::size_t(plan[next])].spec);
                mode = Move;
                mode_s = 0.0F;
            } else if (mode == Final && left < 0.3F && under == goal_region) {
                r.arrived = true;
                break;
            }
        } else {
            const Climber::Status st = climber.step(ClimbWorld::dt);
            if (st == Climber::Status::Done) {
                const int to = map.edges()[std::size_t(plan[next])].to;
                region = under >= 0 ? under : to;
                if (region == goal_region) { mode = Final; walk_to(goal); }
                else if (region == to && ++next < plan.size()) { mode = Walk; walk_to(map.edges()[std::size_t(plan[next])].spec.start); }
                else replan(region);
                mode_s = 0.0F;
            } else if (st == Climber::Status::Failed) {
                // Fallen: plan again once she stands.
                for (int k = 0; k < 90 * 4 && !climber.supported(&top); ++k) { (void)climber.stand(ClimbWorld::dt); w.physics.Update(ClimbWorld::dt, 1, &w.temp, &w.jobs); }
                const Vec3 g = feet();
                const int at = map.region_at(Vec3(g.GetX(), top, g.GetZ()));
                if (at < 0 || r.replans > 6) break;
                replan(at);
                if (plan.empty()) break;
            }
        }
        r.peak_force = std::max(r.peak_force, climber.view().force_n);
        require(climber.view().force_n <= climber.limits().total_force_n + 0.5F, "matron knock: every tick's force inside her one budget");
        require(w.physics.GetBodyInterface().GetGravityFactor(w.her) == 1.0F, "matron knock: her gravity stays on");
        w.physics.Update(ClimbWorld::dt, 1, &w.temp, &w.jobs);
        const RVec3 now = w.at();
        r.worst_step = std::max(r.worst_step, float((now - last).Length()));
        last = now;
    }
    return r;
}

// The chained exam: one course of every skill in a row, then knocked off it.
// From the floor: rungs up deck A's face; a 0.10 m standpipe off B's face
// from A's top; a panel of rungs on C's face whose lowest bar is 3.0 m over
// B (a gap she jumps to); along a 0.30 m board out from C; a 1.2 m ledge
// (E) she mantles from the board's end. Beside the board, 1 m down, a deck
// (D) with rungs up E's face. On the board she is shoved off sideways onto
// D; from there she plans again (her scan, nothing named) and finishes. Down
// is the same course from E to the floor, knocked off the board the same way.
struct ChainRun : KnockRun {
    std::string kinds;   // her first plan's moves, in order
};
// The course's seeded measures (seed 0: the fixed course).
struct Course {
    float a = 3.0F, b = 6.0F, c = 11.0F, gap = 3.0F, board = 0.30F, ledge = 1.2F, pole_x = 0.8F;
};
Course course_of(const std::uint32_t seed) {
    Course k;
    if (seed == 0) return k;
    std::uint32_t rng = seed * 2654435761U + 0x9E3779B9U;
    const auto uniform = [&](const float lo, const float hi) {
        rng = rng * 1664525U + 1013904223U;
        return lo + (hi - lo) * float(rng >> 8) / float(1U << 24);
    };
    k.a = uniform(2.4F, 3.6F);
    k.b = k.a + uniform(2.6F, 3.6F);
    k.gap = uniform(2.6F, 3.3F);
    k.c = k.b + k.gap + uniform(1.6F, 2.4F);
    k.board = uniform(0.25F, 0.40F);
    k.ledge = uniform(1.0F, 1.5F);
    k.pole_x = uniform(-0.9F, 0.9F);
    return k;
}
ChainRun chain_run(const bool up, const std::uint32_t seed = 0, ChainRun *back = nullptr) {
    ClimbWorld w;
    const Course k = course_of(seed);
    const float a = k.a, b = k.b, c = k.c, e = c + k.ledge;
    w.box(Vec3(1.5F, 0.5F * a, 1.5F), RVec3(0, 0.5F * a, -2.5F));                 // A: z -1..-4
    for (float y = 0.3F; y <= a - 0.1F + 1.0e-3F; y += 0.3F) w.box(Vec3(0.25F, 0.02F, 0.02F), RVec3(0, y, -0.95F));
    w.box(Vec3(1.5F, 0.5F * b, 1.5F), RVec3(0, 0.5F * b, -5.5F));                 // B: z -4..-7
    // The pole on A, 0.45 m off B's face, its head 0.05 m under B's top.
    w.box(Vec3(0.05F, 0.5F * (b - 0.05F - a), 0.05F), RVec3(k.pole_x, a + 0.5F * (b - 0.05F - a), -3.55F));
    w.box(Vec3(1.5F, 0.5F * c, 1.5F), RVec3(0, 0.5F * c, -8.5F));                 // C: z -7..-10
    for (float y = b + k.gap; y <= c - 0.1F + 1.0e-3F; y += 0.3F) w.box(Vec3(0.25F, 0.02F, 0.02F), RVec3(0, y, -6.95F));
    w.box(Vec3(0.5F * k.board, 0.1F, 1.5F), RVec3(0, c - 0.1F, -11.5F));          // the board: z -10..-13
    w.box(Vec3(1.1F, 0.5F * (c - 1.0F), 1.5F), RVec3(2.0F, 0.5F * (c - 1.0F), -11.5F));   // D beside it, 1 m down
    w.box(Vec3(2.5F, 0.5F * e, 1.5F), RVec3(1.0F, 0.5F * e, -14.5F));             // E: z -13..-16
    for (float y = c - 0.7F; y <= e - 0.1F + 1.0e-3F; y += 0.3F) w.box(Vec3(0.25F, 0.02F, 0.02F), RVec3(2.0F, y, -12.95F));
    const Vec3 bottom(0, 0, 1.2F), top_at(0, e, -14.7F);
    const Vec3 board_at(0, c, -11.5F);
    const Vec3 catch_at(2.0F, c - 1.0F, -11.5F);
    w.spawn(RVec3(up ? bottom : top_at));
    w.settle(0.3F);
    const float shove_mps = 5.0F;
    const traversal::Ignore ignore{w.her, BodyID()};
    traversal::AffordanceMap map;
    map.scan(w.physics, AABox(Vec3(-5, -1, -19), Vec3(6, e + 4.0F, 4)), ignore);
    Climber climber(w.physics, w.her, ignore);
    const auto feet = [&] { const RVec3 at = w.at(); return Vec3(float(at.GetX()), w.feet(), float(at.GetZ())); };
    RVec3 last = w.at();
    const auto drive = [&](const Vec3 goal, ChainRun &r) {
        const int goal_region = map.region_at(goal);
        const int catch_region = map.region_at(catch_at);
        const int board_region = map.region_at(board_at);
            std::vector<int> plan = map.plan(map.region_at(feet()), goal_region);
        r.kinds.clear();
        for (const int k : plan) r.kinds += std::string(r.kinds.empty() ? "" : ", ") + traversal::move_name(map.edges()[std::size_t(k)].kind);
        if (plan.empty() || board_region < 0) return;
        std::size_t next = 0, wp = 0;
        std::vector<Vec3> path;
        int region = map.region_at(feet());
        const auto walk_to = [&](const Vec3 target) {
            path = traversal::grid_walk(w.physics, ignore, climber.limits(), feet(), target);
            if (path.empty()) path.push_back(target);
            wp = 0;
        };
        enum { Walk, Move, Final } mode = Walk;
        walk_to(map.edges()[std::size_t(plan[0])].spec.start);
        float mode_s = 0.0F;
        const auto replan = [&](const int from) {
            const auto t0 = std::chrono::steady_clock::now();
            plan = map.plan(from, goal_region);
            next = 0;
            if (!plan.empty()) walk_to(map.edges()[std::size_t(plan[0])].spec.start);
            r.replan_us.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count());
            ++r.replans;
            region = from;
            mode = Walk;
            mode_s = 0.0F;
        };
        for (int i = 0; i < 90 * 300; ++i) {
            mode_s += ClimbWorld::dt;
            const Vec3 f = feet();
            float top = 0.0F;
            const int under = climber.supported(&top) ? map.region_at(Vec3(f.GetX(), top, f.GetZ())) : -1;
            // The knock: on deck A, partway along her way, a shove off its side.
            if (!r.knocked && under == board_region && f.GetZ() < board_at.GetZ() + 0.6F && f.GetZ() > board_at.GetZ() - 0.6F &&
                mode == Walk) {
                w.physics.GetBodyInterface().AddImpulse(w.her, Vec3(1.0F, 0, 0) * (Matron::kMassKg * shove_mps));
                r.knocked = true;
            }
            if (r.knocked && under == catch_region) r.on_board = true;
            if (mode == Walk || mode == Final) {
                // Off her plan, standing on somewhere else: plan from there.
                if (under >= 0 && under != region && mode == Walk) {
                    if (under == goal_region) { mode = Final; walk_to(goal); }
                    else { replan(under); if (plan.empty()) break; }
                }
                while (wp + 1 < path.size() && Vec3(path[wp] - f).GetX() * Vec3(path[wp] - f).GetX() +
                                                       Vec3(path[wp] - f).GetZ() * Vec3(path[wp] - f).GetZ() < 0.35F * 0.35F) ++wp;
                (void)climber.walk_between(ClimbWorld::dt, path[wp], 1.2F);
                const Vec3 d = path.back() - f;
                const float left = std::sqrt(d.GetX() * d.GetX() + d.GetZ() * d.GetZ());
                if (mode == Walk && under == region && ((wp + 1 == path.size() && left < 0.6F) || mode_s > 25.0F)) {
                    climber.begin(map.edges()[std::size_t(plan[next])].spec);
                    mode = Move;
                    mode_s = 0.0F;
                } else if (mode == Final && left < 0.3F && under == goal_region) {
                    r.arrived = true;
                    break;
                }
            } else {
                const Climber::Status st = climber.step(ClimbWorld::dt);
                if (st == Climber::Status::Done) {
                    const int to = map.edges()[std::size_t(plan[next])].to;
                    region = under >= 0 ? under : to;
                    if (region == goal_region) { mode = Final; walk_to(goal); }
                    else if (region == to && ++next < plan.size()) { mode = Walk; walk_to(map.edges()[std::size_t(plan[next])].spec.start); }
                    else replan(region);
                    mode_s = 0.0F;
                } else if (st == Climber::Status::Failed) {
                    // Fallen: plan again once she stands.
                    for (int k = 0; k < 90 * 4 && !climber.supported(&top); ++k) {
                        (void)climber.stand(ClimbWorld::dt);
                        w.physics.Update(ClimbWorld::dt, 1, &w.temp, &w.jobs);
                        const RVec3 fell = w.at();   // every tick of the fall counts its own step
                        r.worst_step = std::max(r.worst_step, float((fell - last).Length()));
                        last = fell;
                    }
                    const Vec3 g = feet();
                    const int at = map.region_at(Vec3(g.GetX(), top, g.GetZ()));
                    if (at < 0 || r.replans > 12) break;
                    replan(at);
                    if (plan.empty()) break;
                }
            }
            r.peak_force = std::max(r.peak_force, climber.view().force_n);
            require(climber.view().force_n <= climber.limits().total_force_n + 0.5F, "matron knock: every tick's force inside her one budget");
            require(w.physics.GetBodyInterface().GetGravityFactor(w.her) == 1.0F, "matron knock: her gravity stays on");
            w.physics.Update(ClimbWorld::dt, 1, &w.temp, &w.jobs);
            const RVec3 now = w.at();
            r.worst_step = std::max(r.worst_step, float((now - last).Length()));
            last = now;
        }
    };
    ChainRun r;
    drive(up ? top_at : bottom, r);
    // Back the other way from where she stands: the same world, no reset.
    if (back != nullptr && r.arrived) drive(up ? bottom : top_at, *back);
    return r;
}

void test_knocked_onto_board() {
    std::uint32_t rng = 0x1B873593U;
    const auto uniform = [&](const float lo, const float hi) {
        rng = rng * 1664525U + 1013904223U;
        return lo + (hi - lo) * float(rng >> 8) / float(1U << 24);
    };
    int done = 0;
    for (int run = 0; run < 6; ++run) {
        const bool up = run % 2 == 0;
        const float deck_h = uniform(2.8F, 3.6F);
        const float upper = uniform(1.3F, 1.8F);
        const float width = uniform(0.25F, 0.40F);
        const float drop = uniform(0.9F, 1.2F);
        const bool wall = (run / 2) % 2 == 0;
        const KnockRun r = knock_run(up, deck_h, upper, width, drop, wall, 5.0F);
        std::cout << "  matron knock run " << run << " (" << (up ? "up" : "down") << "): deck " << deck_h << " upper " << upper
                  << " board " << width << " m, " << drop << " m below, " << (wall ? "along a wall" : "open") << ": knocked "
                  << r.knocked << " on board " << r.on_board << " replans " << r.replans << " (";
        for (std::size_t k = 0; k < r.replan_us.size(); ++k) std::cout << (k ? ", " : "") << r.replan_us[k] << " us";
        std::cout << ") arrived " << r.arrived << " worst step " << r.worst_step << " peak " << r.peak_force << " N\n";
        require(r.knocked && r.on_board, "matron knock: shoved off the deck, she lands on the narrow board below");
        require(r.replans >= 1 && r.arrived, "matron knock: from the board she plans again and reaches her goal");
        require(r.worst_step <= 0.2F, "matron knock: no tick moves her more than 0.2 m");
        for (const double us : r.replan_us) require(us < 1.0e4, "matron knock: a replan costs its tick under 10 ms");
        ++done;
    }
    std::cout << "PASS scraperx_sim matron knocked onto a narrow board: replanned and arrived " << done << " of 6\n";
}

// Standing still on an open board, nothing asked of her but to stand, a
// little off its middle and drifting: her balance alone keeps her on it.
void test_stand_on_board() {
    for (const float width : {0.25F, 0.32F, 0.40F}) {
        ClimbWorld w;
        w.box(Vec3(0.5F * width, 0.1F, 1.5F), RVec3(0, 3.9F, 0));   // top 4, over a 4 m drop
        w.spawn(RVec3(0.06F, 4.0F, 0));
        w.physics.GetBodyInterface().SetLinearVelocity(w.her, Vec3(0.15F, 0, 0.1F));
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        for (int i = 0; i < 90 * 5; ++i) {
            (void)climber.step(ClimbWorld::dt);   // no move begun: she stands
            w.physics.Update(ClimbWorld::dt, 1, &w.temp, &w.jobs);
        }
        std::cout << "  matron stands on a " << width << " m board: feet " << w.feet() << " x " << w.at().GetX() << "\n";
        require(std::abs(w.feet() - 4.0F) < 0.06F && std::abs(float(w.at().GetX())) < 0.5F * width,
                "matron stand: standing still, her balance keeps her on a narrow board");
    }
    std::cout << "PASS scraperx_sim matron stands balanced on narrow boards\n";
}

void run_matron_narrow() {
    test_narrow_ledges();
    test_stand_on_board();
    test_knocked_onto_board();
}

void run_matron_climb() {
    test_power_budget();
    test_jump_catch_escape_margin();
    test_power_moving_bar();
    test_rungs();
    test_rung_failures();
    test_pole();
    test_capped_launches();
    test_jump();
    test_drops_and_mantle();
    test_dynamic_support();
    test_mantle_moving_ledge();
    test_bump_mid_lower();
    test_wall_across_step();
    test_rung_gaps();
    test_pole_head_clearance();
    run_matron_narrow();
}

namespace {
// The scan of the real tower, written out for the inventory comparison
// (tools/matron/climb_inventory_diff.py) when CLIMB_SCAN_OUT names a file.
void dump_scan(const traversal::AffordanceMap &map, const char *path) {
    FILE *f = std::fopen(path, "w");
    if (f == nullptr) return;
    const auto &st = map.stats();
    std::fprintf(f, "{\"stats\":{\"seconds\":%.4f,\"bodies\":%u,\"leaves\":%u,\"patches\":%u,\"usable_patches\":%u,\"bars\":%u,"
                    "\"ladders\":%u,\"poles\":%u,\"regions\":%u,\"edges\":%u,\"usable_edges\":%u,\"capsule_queries\":%u},\n",
                 st.seconds, st.bodies, st.leaves, st.patches, st.usable_patches, st.bars, st.ladders, st.poles, st.regions,
                 st.edges, st.usable_edges, st.capsule_queries);
    std::fprintf(f, "\"regions\":[\n");
    for (std::size_t i = 0; i < map.regions().size(); ++i) {
        const auto &r = map.regions()[i];
        std::fprintf(f, "%s{\"id\":%d,\"top\":%.3f,\"min_y\":%.3f,\"max_y\":%.3f,\"x\":[%.2f,%.2f],\"z\":[%.2f,%.2f],\"area\":%.2f,\"patches\":%zu}",
                     i ? ",\n" : "", r.id, double(r.top), double(r.min_y), double(r.max_y), double(r.min_x), double(r.max_x),
                     double(r.min_z), double(r.max_z), double(r.area), r.patches.size());
    }
    std::fprintf(f, "],\n\"edges\":[\n");
    for (std::size_t i = 0; i < map.edges().size(); ++i) {
        const auto &e = map.edges()[i];
        std::string reason = e.reason;
        for (char &c : reason) if (c == '"') c = '\'';
        std::fprintf(f, "%s{\"from\":%d,\"to\":%d,\"kind\":\"%s\",\"usable\":%s,\"line\":[%.2f,%.2f],\"start\":[%.2f,%.2f,%.2f],"
                        "\"goal\":[%.2f,%.2f,%.2f],\"reason\":\"%s\"}",
                     i ? ",\n" : "", e.from, e.to, traversal::move_name(e.kind), e.usable ? "true" : "false",
                     double(e.spec.line.GetX()), double(e.spec.line.GetZ()), double(e.spec.start.GetX()), double(e.spec.start.GetY()),
                     double(e.spec.start.GetZ()), double(e.spec.goal.GetX()), double(e.spec.goal.GetY()), double(e.spec.goal.GetZ()),
                     reason.c_str());
    }
    std::fprintf(f, "],\n\"ladders\":[\n");
    for (std::size_t i = 0; i < map.ladders().size(); ++i) {
        const auto &l = map.ladders()[i];
        std::fprintf(f, "%s{\"x\":%.2f,\"z\":%.2f,\"bottom\":%.2f,\"top\":%.2f,\"rungs\":%d,\"pitch\":%.3f}", i ? ",\n" : "",
                     double(l.x), double(l.z), double(l.bottom), double(l.top), l.rungs, double(l.pitch));
    }
    std::fprintf(f, "],\n\"poles\":[\n");
    for (std::size_t i = 0; i < map.poles().size(); ++i) {
        const auto &p = map.poles()[i];
        std::fprintf(f, "%s{\"x\":%.2f,\"z\":%.2f,\"bottom\":%.2f,\"top\":%.2f,\"section\":%.3f}", i ? ",\n" : "", double(p.x),
                     double(p.z), double(p.bottom), double(p.top), double(p.section));
    }
    std::fprintf(f, "]}\n");
    std::fclose(f);
}

void test_tower_scan() {
    Simulation s(InitialSpawn::Deck154);
    (void)s.advance_frame(0.1);
    const traversal::AffordanceMap &map = s.matron_affordances();
    const auto &st = map.stats();
    std::cout << "  matron climb scan: " << st.seconds * 1000.0 << " ms; bodies " << st.bodies << " leaves " << st.leaves
              << " tops " << st.patches << " (with headroom " << st.usable_patches << ") bars " << st.bars << " ladders "
              << st.ladders << " poles " << st.poles << " regions " << st.regions << " edges " << st.edges << " (usable "
              << st.usable_edges << ") capsule queries " << st.capsule_queries << "\n";
    if (const char *out = std::getenv("CLIMB_SCAN_OUT")) dump_scan(map, out);
    if (std::getenv("CLIMB_SYMMETRY")) {
        // Is her feet's floor search symmetric? Both ways between pairs of tops in one region.
        const auto &ps = map.patches();
        int pairs = 0, asym = 0, both = 0, none = 0;
        for (std::size_t a = 0; a < ps.size() && pairs < 60; a += 7) {
            for (std::size_t b = a + 13; b < ps.size() && pairs < 60; b += 29) {
                if (ps[a].region != ps[b].region || ps[a].narrow || ps[b].narrow) continue;
                const auto mid = [](const traversal::Patch &p) {
                    return std::array<double, 3>{0.5 * (p.min_x + p.max_x), double(p.top), 0.5 * (p.min_z + p.max_z)};
                };
                const auto A = mid(ps[a]);
                const auto B = mid(ps[b]);
                if (std::hypot(A[0] - B[0], A[2] - B[2]) > 25.0) continue;
                ++pairs;
                const bool ab = s.debug_matron_grid_walk(A[0], A[1], A[2], B[0], B[1], B[2], 8.0F, 20000) > 0;
                const bool ba = s.debug_matron_grid_walk(B[0], B[1], B[2], A[0], A[1], A[2], 8.0F, 20000) > 0;
                if (ab != ba) {
                    ++asym;
                    std::printf("  asym (%.2f %.2f %.2f) %s (%.2f %.2f %.2f)\n", A[0], A[1], A[2], ab ? "->" : "<-", B[0], B[1], B[2]);
                }
                both += ab && ba;
                none += !ab && !ba;
            }
        }
        const double P[4][3] = {{-6.0, 154.85, -131.9}, {5.0, 154.0, -129.5}, {11.0, 154.0, -127.15}, {-6.0, 154.0, -128.6}};
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                if (i != j)
                    std::printf("  walk %d->%d: %zu\n", i, j,
                                s.debug_matron_grid_walk(P[i][0], P[i][1], P[i][2], P[j][0], P[j][1], P[j][2], 8.0F, 20000));
        std::printf("  symmetry: pairs %d both %d neither %d asymmetric %d\n", pairs, both, none, asym);
    }
    if (std::getenv("CLIMB_ASCII")) {
        // A plan view: '.' her capsule fits standing on floor, '#' it does not, ' ' no floor.
        for (float z = -124.0F; z >= -135.0F; z -= 0.4F) {
            std::string row;
            for (float x = -16.0F; x <= 4.0F; x += 0.4F) {
                const std::size_t n = s.debug_matron_grid_walk(x, 154.0, z, x, 154.0, z, 0.0F, -1);
                row += n == 0 ? ' ' : n == 1 ? '#' : '.';
            }
            std::printf("%7.1f %s\n", double(z), row.c_str());
        }
    }
    if (std::getenv("CLIMB_PATCHES")) {
        for (const traversal::Patch &p : map.patches()) {
            if (p.max_x > -9 && p.min_x < -3 && p.max_z > -133 && p.min_z < -127 && p.top > 153 && p.top < 157)
                std::printf("patch top %.2f x %.2f..%.2f z %.2f..%.2f region %d\n", p.top, p.min_x, p.max_x, p.min_z, p.max_z, p.region);
        }
    }
}
// End to end on the tower itself: from her locker deck she climbs the home
// stack 132 -> 143 -> 154 -> 176.25 and on up the standpipe to 198.25, then
// back down every edge to 132, each leg planned over the scanned map and every
// move physical, gravity on throughout.
// A leg passes only clean: every move it took ended Done (no lost hold, no
// fall caught by luck and replanned from).
bool journey_leg(Simulation &s, const traversal::AffordanceMap &map, const double top, const double limit_s) {
    const unsigned failed_before = s.matron_journey().moves_failed;
    if (!s.debug_matron_travel(top)) {
        std::cout << "  leg to " << top << ": no plan\n";
        return false;
    }
    double t = 0.0;
    Simulation::MatronJourney j = s.matron_journey();
    double peak = 0.0;
    double min_gravity = 1.0;
    while (j.active && t < limit_s) {
        (void)s.advance_frame(1.0 / 60.0);
        t += 1.0 / 60.0;
        j = s.matron_journey();
        peak = std::max(peak, j.peak_force_n);
        min_gravity = std::min(min_gravity, j.gravity_factor);
    }
    const double got = j.region >= 0 ? map.regions()[static_cast<std::size_t>(j.region)].top : -1.0;
    std::cout << "  leg to " << top << ": " << t << " s, at " << got << ", moves " << j.moves_done << " failed "
              << j.moves_failed << " replans " << j.replans << " grips " << j.grips << " catches " << j.catches
              << " slips " << j.slips << " peak force " << peak << " N\n";
    require(min_gravity == 1.0, "her gravity stays on through every journey");
    require(peak <= traversal::Limits{}.total_force_n + 1.0, "every journey force within her shared cap");
    require(j.moves_failed == failed_before, "every move of a journey leg ends Done (no recovery counted as a climb)");
    return !j.active && j.arrivals > 0 && std::abs(got - top) < 0.3;
}

void test_tower_journeys() {
    Simulation s(InitialSpawn::Deck154);
    (void)s.advance_frame(0.1);
    const traversal::AffordanceMap &map = s.matron_affordances();
    const double ups[] = {132.0, 143.0, 154.0, 176.25};
    for (double top : ups) {
        require(journey_leg(s, map, top, 90.0), "Matron climbs the home stack to each deck");
    }
    const double downs[] = {154.0, 143.0, 132.0};
    for (double top : downs) {
        require(journey_leg(s, map, top, 90.0), "Matron climbs back down the home stack to each deck");
    }
    // Held-out exam, a gap the inventory lists as blocked (no pole grip).
    // Nothing in the runtime path knows this pole.
    Simulation e(InitialSpawn::Deck154);
    (void)e.advance_frame(0.1);
    const traversal::AffordanceMap &m = e.matron_affordances();
    require(journey_leg(e, m, 176.25, 90.0), "Matron climbs to 176.25 for the standpipe exam");
    require(journey_leg(e, m, 198.25, 120.0), "Matron climbs the standpipe 176.25 -> 198.25 on her own");
    // Held-out exam two: the catwalk to the scaffold panel. With its bars
    // raised 0.2 m (the integrated geometry) its foot is in her reach: she
    // finds that on her own and climbs to 220.25 (never a fall, never forcing).
    {
        const unsigned failed_before = e.matron_journey().moves_failed;
        require(e.debug_matron_travel(220.25), "the 220 exam: a plan to try");
        double t = 0.0;
        Simulation::MatronJourney j = e.matron_journey();
        while (j.active && t < 120.0) {
            (void)e.advance_frame(1.0 / 60.0);
            t += 1.0 / 60.0;
            j = e.matron_journey();
            require(j.gravity_factor == 1.0 && j.peak_force_n <= traversal::Limits{}.total_force_n + 1.0, "the 220 exam: laws hold");
        }
        const double at = j.region >= 0 ? m.regions()[static_cast<std::size_t>(j.region)].top : -1.0;
        std::cout << "  matron 198.25 -> 220.25 panel: " << (j.arrivals > 0 && std::abs(at - 220.25) < 0.3 ? "arrived" : "refused")
                  << " after " << t << " s, failed starts " << (j.moves_failed - failed_before) << ", standing on " << at << "\n";
        require(!j.active && j.arrivals > 0 && std::abs(at - 220.25) < 0.3 && j.moves_failed == failed_before,
                "the 220 exam: she climbs the panel to 220.25 by her own plan, every move Done");
    }
}
} // namespace

void run_matron_chain() {
    int done = 0;
    for (const bool up : {true, false}) {
        const ChainRun r = chain_run(up);
        std::cout << "  matron chained exam " << (up ? "up" : "down") << ": plan [" << r.kinds << "], knocked " << r.knocked << " onto the side deck "
                  << r.on_board << " replans " << r.replans << " (";
        for (std::size_t k = 0; k < r.replan_us.size(); ++k) std::cout << (k ? ", " : "") << r.replan_us[k] << " us";
        std::cout << ") arrived " << r.arrived << " worst step " << r.worst_step << " m peak " << r.peak_force << " N\n";
        for (const char *skill : up ? std::vector<const char *>{"rung_up", "pole_up", "jump_catch", "mantle"}
                                    : std::vector<const char *>{"rung_down", "pole_down", "hang_drop"}) {
            require(r.kinds.find(skill) != std::string::npos, "matron chain: the course takes every skill");
        }
        require(r.knocked && r.on_board, "matron chain: knocked off the board onto the deck beside it");
        require(r.replans >= 1 && r.arrived, "matron chain: she plans again from there and finishes the course");
        require(r.worst_step <= 0.2F, "matron chain: no tick moves her more than 0.2 m");
        for (const double us : r.replan_us) require(us < 1.0e4, "matron chain: a replan costs its tick under 10 ms");
        ++done;
    }
    std::cout << "PASS scraperx_sim matron chained exam (ladder, pole, gap, board, ledge, knock-off, replan): " << done << " of 2\n";
}

// Ten unseen towers (seeded measures: deck heights, the pole's place, the
// gap under the panel, the board's width, the ledge), each climbed from the
// floor to its top and back down to the floor in one world with no reset,
// knocked off its board each way, replanning from where she lands.
void run_matron_towers() {
    int done = 0;
    std::cout << "  | tower | up | down | replans up/down | worst replan | worst step |\n";
    for (std::uint32_t seed = 1; seed <= 10; ++seed) {
        ChainRun down;
        const ChainRun up = chain_run(true, seed, &down);
        double worst_us = 0.0;
        for (const double us : up.replan_us) worst_us = std::max(worst_us, us);
        for (const double us : down.replan_us) worst_us = std::max(worst_us, us);
        const float worst = std::max(up.worst_step, down.worst_step);
        std::cout << "  | " << seed << " | " << (up.arrived ? "arrived" : "FAILED") << " [" << up.kinds << "] | "
                  << (down.arrived ? "arrived" : "FAILED") << " [" << down.kinds << "] | " << up.replans << "/" << down.replans
                  << " | " << worst_us << " us | " << worst << " m |\n";
        require(up.arrived && down.arrived, "matron towers: up to the top and back down, no reset");
        require(up.knocked && down.knocked && up.on_board && down.on_board, "matron towers: knocked off the board each way");
        require(worst <= 0.2F, "matron towers: no tick moves her more than 0.2 m");
        for (const double us : up.replan_us) require(us < 1.0e4, "matron towers: a replan costs its tick under 10 ms");
        for (const double us : down.replan_us) require(us < 1.0e4, "matron towers: a replan costs its tick under 10 ms");
        ++done;
    }
    std::cout << "PASS scraperx_sim matron unseen towers up and down without a reset: " << done << " of 10\n";
}

// The player's own climbing is untouched by her: the same ladder climb,
// alone and with her climbing down the stack beside him, takes the
// same holds to the same top in the same time. (Her body on the same
// ladder foot does block him physically; that is contact, not a rule.)
void test_player_unchanged() {
    const auto climb = [](const bool with_her, double &seconds, double &top) {
        Simulation s(InitialSpawn::Deck154);
        require(s.advance_frame(1.0).accepted, "player climb: settle");
        if (with_her) require(s.debug_matron_travel(132.0), "player climb: she sets off down the stack");
        const double t0 = s.snapshot().simulation_time_seconds;
        const bool ok = climb_route_ladder(s);
        if (!ok) {
            const Snapshot n = s.snapshot();
            std::cout << "  player at " << n.player_position.x << " " << n.player_position.y << " " << n.player_position.z
                      << " climbing " << is_climbing(n) << " grip " << n.grip_available << " held " << n.held_by_matron << "\n";
        }
        require(ok, with_her ? "player climb (her travelling): the 154 ladder" : "player climb (alone): the 154 ladder");
        seconds = s.snapshot().simulation_time_seconds - t0;
        top = s.snapshot().player_position.y;
    };
    double alone_s = 0, alone_y = 0, near_s = 0, near_y = 0;
    climb(false, alone_s, alone_y);
    climb(true, near_s, near_y);
    // And with her setting off up the stack, wanting the ladder he is at the
    // foot of: she gives way (stands off its foot until he is clear), and his
    // climb is the same as alone.
    {
        Simulation s(InitialSpawn::Deck154);
        require(s.advance_frame(1.0).accepted, "player climb: settle");
        require(s.debug_matron_travel(176.25), "player climb: she sets off up the stack");
        const double t0 = s.snapshot().simulation_time_seconds;
        require(climb_route_ladder(s), "player climb (her wanting the ladder): the 154 ladder");
        const double up_s = s.snapshot().simulation_time_seconds - t0;
        const double up_y = s.snapshot().player_position.y;
        const unsigned yields = s.matron_journey().yields;
        std::cout << "  player ladder climb with her wanting the same climb " << up_s << " s to " << up_y << ", her yield ticks "
                  << yields << "\n";
        require(std::abs(alone_s - up_s) <= 0.25 && std::abs(alone_y - up_y) <= 0.1,
                "player climbing is the same with her wanting the same ladder");
    }
    std::cout << "  player ladder climb alone " << alone_s << " s to " << alone_y << ", with her travelling " << near_s
              << " s to " << near_y << "\n";
    require(std::abs(alone_s - near_s) <= 0.25 && std::abs(alone_y - near_y) <= 0.1,
            "player climbing is the same with her travelling nearby");
}

void run_matron_climb_board() { test_narrow_board(); test_player_unchanged(); }

// One deck box, something laid across its top. A curb within a step (and a
// block on part of it) leaves it one floor; a wall taller than a step across
// it splits it in two, with nothing joining the halves but over the wall.
void test_floor_split() {
    const auto regions_on = [](const float across_h, const bool full, const float thick = 0.5F, const float at_x = 0.0F) {
        ClimbWorld w;
        w.box(Vec3(3.0F, 0.25F, 3.0F), RVec3(0, 0.25F, -10));                       // the deck, top 0.5
        w.box(Vec3(0.5F * thick, 0.5F * across_h, full ? 3.0F : 1.0F), RVec3(at_x, 0.5F + 0.5F * across_h, -10));
        w.spawn(RVec3(0, 0, 3));
        traversal::AffordanceMap map;
        map.scan(w.physics, AABox(Vec3(-6, -1, -15), Vec3(6, 4, 6)), {w.her, BodyID()});
        const int west = map.region_at(Vec3(-2.0F, 0.5F, -10));
        const int east = map.region_at(Vec3(2.0F, 0.5F, -10));
        return std::make_pair(west, east);
    };
    const auto curb = regions_on(traversal::Limits{}.step_max_m * 0.6F, true);
    std::cout << "  matron floor split: curb " << curb.first << "/" << curb.second;
    require(curb.first >= 0 && curb.first == curb.second, "matron floor: a curb within a step does not split the deck");
    const auto block = regions_on(1.0F, false);
    std::cout << ", block on part " << block.first << "/" << block.second;
    require(block.first >= 0 && block.first == block.second, "matron floor: a block on part of a deck leaves it one floor");
    const auto wall = regions_on(1.0F, true);
    std::cout << ", wall across " << wall.first << "/" << wall.second << "\n";
    require(wall.first >= 0 && wall.second >= 0 && wall.first != wall.second,
            "matron floor: a wall taller than a step across the deck splits it");
    // A thin wall (0.2 m, the thinnest she must see) wherever it stands on
    // the reading grid: split every time.
    int thin_split = 0;
    for (int k = 0; k < 5; ++k) {
        const auto thin = regions_on(1.0F, true, 0.2F, 0.037F * float(k));
        thin_split += thin.first >= 0 && thin.second >= 0 && thin.first != thin.second ? 1 : 0;
    }
    std::cout << "  matron floor split: thin wall at 5 grid offsets split " << thin_split << "/5\n";
    require(thin_split == 5, "matron floor: a 0.2 m wall splits the deck wherever it stands");
}

void run_matron_climb_scan() {
    test_floor_split();
    test_tower_scan();
    test_tower_journeys();
}

// The whole tower, as her own scan reads it: every deck (a region of 6 m2 or
// more, the largest at its height) and every pair of them her scan joins
// with no other deck between, she travels up and back down, from wherever
// the last leg left her. Each leg reports its outcome, its worst step a tick
// and her gravity. Nothing here names a deck, a ladder or a route.
void run_matron_tower_proof() {
    Simulation s(InitialSpawn::Deck154);
    (void)s.advance_frame(0.1);
    const traversal::AffordanceMap &map = s.matron_affordances();
    const auto &regions = map.regions();
    std::vector<int> decks;
    for (const traversal::Region &r : regions) {
        if (r.area >= 6.0F && map.region_near_top(r.top, 0.3F) == r.id) decks.push_back(r.id);
    }
    // Only decks her scan joins to where she stands at the start (the
    // tower's; the world's other ground she has no way to).
    const auto p0 = s.matron_state().position;
    int home = map.region_at(Vec3(float(p0.x), float(p0.y), float(p0.z)));
    if (home < 0) home = map.region_at(Vec3(float(p0.x), float(p0.y) - Matron::kStandCentre, float(p0.z)));
    require(home >= 0, "matron tower proof: she stands on a region her scan read");
    decks.erase(std::remove_if(decks.begin(), decks.end(), [&](const int d) {
        return d != home && (map.plan(home, d).empty() || map.plan(d, home).empty());
    }), decks.end());
    const auto is_deck = [&](const int id) { return std::find(decks.begin(), decks.end(), id) != decks.end(); };
    struct Pair { int lo, hi; };
    std::vector<Pair> pairs;
    for (const int a : decks) {
        for (const int b : decks) {
            if (regions[std::size_t(a)].top >= regions[std::size_t(b)].top - 0.5F) continue;
            const std::vector<int> up = map.plan(a, b);
            const std::vector<int> down = map.plan(b, a);
            if (up.empty() || down.empty()) continue;
            bool direct = true;
            for (std::size_t k = 0; k + 1 < up.size(); ++k) direct = direct && !is_deck(map.edges()[std::size_t(up[k])].to);
            if (direct) pairs.push_back({a, b});
        }
    }
    std::cout << "  matron tower proof: " << decks.size() << " decks, " << pairs.size() << " deck pairs her scan joins\n";
    const auto leg = [&](const int goal, double &worst, double &gravity, double &secs) {
        worst = 0.0;
        gravity = 1.0;
        secs = 0.0;
        const unsigned failed0 = s.matron_journey().moves_failed;
        if (std::getenv("SCRAPERX_TOWER_DEBUG") != nullptr) {
            const Simulation::MatronJourney j0 = s.matron_journey();
            for (const int k : map.plan(j0.region, goal)) {
                const traversal::Edge &e = map.edges()[std::size_t(k)];
                std::cout << "    plan edge " << k << " " << e.from << "->" << e.to << " " << traversal::move_name(e.kind)
                          << " start " << e.spec.start.GetX() << "," << e.spec.start.GetY() << "," << e.spec.start.GetZ()
                          << " line " << e.spec.line.GetX() << "," << e.spec.line.GetZ() << " " << e.reason << "\n";
            }
        }
        if (!s.debug_matron_travel_region(goal)) return std::string("no plan");
        auto last = s.matron_state().position;
        Simulation::MatronJourney j = s.matron_journey();
        const double cap = 60.0 + 60.0 * double(s.matron_journey().steps);   // a minute a move
        while (j.active && secs < cap) {
            (void)s.advance_frame(1.0 / 60.0);
            secs += 1.0 / 60.0;
            const auto p = s.matron_state().position;
            if (std::hypot(p.x - last.x, p.y - last.y, p.z - last.z) > 2.0 && std::getenv("SCRAPERX_TOWER_DEBUG") != nullptr) {
                std::cout << "    jump from " << last.x << "," << last.y << "," << last.z << " move " << int(j.move)
                          << " step " << j.step << "/" << j.steps << " region " << j.region << "\n";
            }
            worst = std::max(worst, std::hypot(p.x - last.x, p.y - last.y, p.z - last.z));
            last = p;
            j = s.matron_journey();
            gravity = std::min(gravity, j.gravity_factor);
        }
        const bool there = !j.active && j.region >= 0 &&
                           j.region == goal;
        if (!there) {
            return "did not arrive (at region " + std::to_string(j.region) + " of goal " + std::to_string(goal) +
                   ", failure " + std::to_string(int(j.last_failure)) + ", moves " + std::to_string(j.moves_done) +
                   " failed " + std::to_string(j.moves_failed - failed0) + " replans " + std::to_string(j.replans) + " yields " + std::to_string(j.yields) + " state " + std::to_string(int(s.matron_state().state)) + ", at " + std::to_string(last.x) + "," + std::to_string(last.y) + "," +
                   std::to_string(last.z) + ")";
        }
        if (std::getenv("SCRAPERX_TOWER_DEBUG") != nullptr) std::cout << "    ended at " << last.x << "," << last.y << "," << last.z << "\n";
        return std::string(j.moves_failed == failed0 ? "arrived clean" : "arrived after a replan");
    };
    int clean = 0, passed = 0;
    std::vector<std::string> failed_legs;
    const char *only_lo = std::getenv("SCRAPERX_TOWER_LO");   // debugging: one pair's lower deck height
    for (const Pair &p : pairs) {
        if (only_lo != nullptr && std::abs(regions[std::size_t(p.lo)].top - std::atof(only_lo)) > 0.5F) continue;
        if (only_lo != nullptr) {
            for (const int k : map.plan(p.lo, p.hi)) {
                const traversal::Edge &e = map.edges()[std::size_t(k)];
                std::cout << "    up edge " << k << " " << traversal::move_name(e.kind) << " start " << e.spec.start.GetX() << "," << e.spec.start.GetY() << "," << e.spec.start.GetZ() << " goal " << e.spec.goal.GetX() << "," << e.spec.goal.GetY() << "," << e.spec.goal.GetZ() << " " << e.reason << "\n";
            }
            for (const int k : map.plan(p.hi, p.lo)) {
                const traversal::Edge &e = map.edges()[std::size_t(k)];
                std::cout << "    down edge " << k << " " << traversal::move_name(e.kind) << " start " << e.spec.start.GetX() << "," << e.spec.start.GetY() << "," << e.spec.start.GetZ() << " goal " << e.spec.goal.GetX() << "," << e.spec.goal.GetY() << "," << e.spec.goal.GetZ() << " " << e.reason << "\n";
            }
            if (std::getenv("SCRAPERX_TOWER_PATCHES")) {
                const int want = std::atoi(std::getenv("SCRAPERX_TOWER_PATCHES"));
                for (const traversal::Patch &q : map.patches()) {
                    if (q.region == want) std::cout << "    patch top " << q.top << " x " << q.min_x << ".." << q.max_x << " z " << q.min_z << ".." << q.max_z << " narrow " << q.narrow << " split " << q.split << "\n";
                }
            }
            // Debugging only: put her on that pair's upper deck's own spot (a fixture placement).
            const traversal::Edge *e0 = nullptr;
            for (const int k : map.plan(p.hi, p.lo)) { e0 = &map.edges()[std::size_t(k)]; break; }
            if (const char *from = std::getenv("SCRAPERX_TOWER_FROM"); from != nullptr) {
                double fx = 0, fy = 0, fz = 0;
                if (std::sscanf(from, "%lf,%lf,%lf", &fx, &fy, &fz) == 3) s.debug_place_matron(Vector3{fx, fy, fz});
            } else if (e0 != nullptr) {
                s.debug_place_matron(Vector3{e0->spec.start.GetX(), e0->spec.start.GetY() + 0.05, e0->spec.start.GetZ()});
            }
            for (int k = 0; k < 120; ++k) (void)s.advance_frame(1.0 / 60.0);
        }
        double w = 0.0, g = 1.0, t = 0.0;
        const std::string pre = std::getenv("SCRAPERX_TOWER_FROM") != nullptr ? std::string("arrived") : leg(p.lo, w, g, t);   // to the pair's lower deck first (not judged)
        if (pre.rfind("arrived", 0) != 0) std::cout << "    (to " << regions[std::size_t(p.lo)].top << " first: " << pre << ")\n";
        for (const bool up : {true, false}) {
            const int goal = up ? p.hi : p.lo;
            const std::string out = leg(goal, w, g, t);
            std::cout << "  | " << regions[std::size_t(p.lo)].top << " <-> " << regions[std::size_t(p.hi)].top << " | "
                      << (up ? "up" : "down") << " | " << out << " | " << t << " s | worst step " << w << " m | gravity "
                      << g << " |\n";
            // Every leg reported: the verdict comes after the whole tower.
            const bool ok = out.rfind("arrived", 0) == 0 && w <= 0.20 && g == 1.0;
            if (ok) ++passed; else failed_legs.push_back(std::to_string(regions[std::size_t(p.lo)].top) + (up ? " up" : " down"));
            clean += out == "arrived clean" ? 1 : 0;
        }
    }
    std::cout << "  matron tower proof: " << passed << " of " << 2 * pairs.size() << " legs passed";
    for (const std::string &f : failed_legs) std::cout << "; failed " << f;
    std::cout << "\n";
    require(!pairs.empty(), "matron tower proof: her scan joins decks");
    require(failed_legs.empty(), "matron tower proof: every deck pair her scan joins, up and down, arrived, no tick over 0.20 m, gravity on");
    std::cout << "PASS scraperx_sim matron tower proof: " << 2 * pairs.size() << " legs over " << pairs.size()
              << " deck pairs, " << clean << " clean\n";
}

// Every ladder in the tower, as her own scan reads it: for each one her scan
// gives a climb up (on the rungs, or a jump for the bottom rung), she goes up
// it from where that climb starts and back down it. On the rungs she keeps to
// the ladder's line (no drift along the rungs beyond kLadderDriftM, the fault
// that once walked her off the first ladder's top), tops out onto the deck,
// never loses a hold and never moves more than 0.20 m a tick. Her climbing
// names no ladder. This test names only the ones her scan refuses today, each
// for a reason in the world's steel (an answer key for the test, never read
// by her): every refusal is printed with the scan's own reasons, and a refusal
// outside that list fails, so the refused set cannot grow unseen.
void run_matron_ladders() {
    constexpr double kLadderDriftM = 0.30;
    struct Refused {
        float x, z;
        const char *why;   // what in the world stops her, as measured 2026-10-27
    };
    static constexpr Refused kRefusedToday[] = {
        {-8.21F, -145.6F, "foot: the 0.30 m plank (band_plate_shop.cpp) ends 0.46 m past the rungs, she needs 0.50 m; "
                          "the wall side is under the 484 ring's deck"},
        {8.7F, -161.8F, "the cradle's west rail (band_stack.cpp) 0.14 m inside her body on the rungs; "
                        "the 82 m walkway starts 1.1 m along the rungs from the top"},
        {25.8F, -123.8F, "the rungs are inside a 1.6 m tower column (entity 11, 22-33 m)"},
        {2.4F, -122.45F, "under the solid tier 3 gantry slab, 21.88-22.00 m: no hatch"},
        {2.4F, -120.1F, "under the solid tier 1 catwalk slab, 7.38-7.50 m: no hatch"},
        {4.5F, -119.25F, "under the solid tier 2 slab, 14.68-14.80 m: no hatch; no floor at the foot on the open side"},
    };
    Simulation s(InitialSpawn::Deck154);
    (void)s.advance_frame(0.1);
    const traversal::AffordanceMap &map = s.matron_affordances();
    const auto &edges = map.edges();
    int tried = 0, passed = 0, refused = 0;
    std::vector<std::string> failed, unexpected;
    for (const traversal::Ladder &l : map.ladders()) {
        const auto near_line = [&](const traversal::Edge &e) {
            return std::abs(e.spec.line.GetX() - l.x) < 0.6F && std::abs(e.spec.line.GetZ() - l.z) < 0.6F;
        };
        const auto on_line = [&](const traversal::Edge &e, MoveKind kind) {
            return e.usable && e.kind == kind && near_line(e);
        };
        int up = -1;
        for (const MoveKind kind : {MoveKind::RungUp, MoveKind::JumpCatch}) {
            for (std::size_t k = 0; k < edges.size() && up < 0; ++k) if (on_line(edges[k], kind)) up = int(k);
        }
        std::cout << "  matron ladder at (" << l.x << ", " << l.z << ") " << l.bottom << "->" << l.top << " m, " << l.rungs
                  << " rungs: ";
        if (up < 0) {
            ++refused;
            const Refused *known = nullptr;
            for (const Refused &r : kRefusedToday) {
                if (std::abs(r.x - l.x) < 0.05F && std::abs(r.z - l.z) < 0.05F) known = &r;
            }
            std::cout << "refused by her scan (" << (known != nullptr ? known->why : "NOT A KNOWN REFUSAL") << ")\n";
            int ways = 0;
            for (const traversal::Edge &e : edges) {
                if (!near_line(e) || (e.kind != MoveKind::RungUp && e.kind != MoveKind::JumpCatch && e.kind != MoveKind::RungDown)) continue;
                ++ways;
                std::cout << "    " << traversal::move_name(e.kind) << " " << e.from << "->" << e.to << ": " << e.reason << "\n";
            }
            if (ways == 0) std::cout << "    no climb on its line at all: no floor at its foot or head in her reach\n";
            if (known == nullptr) {
                std::ostringstream o;
                o << "(" << l.x << ", " << l.z << ")";
                unexpected.push_back(o.str());
            }
            continue;
        }
        const traversal::Edge eu = edges[std::size_t(up)];
        ++tried;
        const auto leg = [&](const int goal, const char *way) {
            auto last = s.matron_state().position;
            double worst = 0.0, drift = 0.0, gravity = 1.0, secs = 0.0;
            const auto j0 = s.matron_journey();
            if (!s.debug_matron_travel_region(goal)) return std::string(way) + " no plan FAILED";
            Simulation::MatronJourney j = s.matron_journey();
            while (j.active && secs < 120.0) {
                (void)s.advance_frame(1.0 / 60.0);
                secs += 1.0 / 60.0;
                const auto p = s.matron_state().position;
                worst = std::max(worst, std::hypot(p.x - last.x, p.y - last.y, p.z - last.z));
                last = p;
                const double feet = p.y - Matron::kStandCentre;
                const double across = l.along_x ? p.x - l.x : p.z - l.z;
                const double off = l.along_x ? p.z - l.z : p.x - l.x;
                if (feet > l.bottom + 0.5 && feet < l.top - 0.3 && std::abs(off) < 1.2) drift = std::max(drift, std::abs(across));
                j = s.matron_journey();
                gravity = std::min(gravity, j.gravity_factor);
            }
            std::ostringstream o;
            o << way << " drift " << drift << " m worst step " << worst << " m slips " << (j.slips - j0.slips) << " misses "
              << (j.misses - j0.misses) << " failed " << (j.moves_failed - j0.moves_failed);
            const bool ok = !j.active && j.region == goal && drift <= kLadderDriftM && worst <= 0.20 && gravity == 1.0 &&
                            j.slips == j0.slips && j.misses == j0.misses && j.moves_failed == j0.moves_failed;
            return o.str() + (ok ? " ok" : " FAILED");
        };
        const auto st = eu.spec.start;
        s.debug_place_matron(Vector3{double(st.GetX()), double(st.GetY()), double(st.GetZ())});
        for (int i = 0; i < 30; ++i) (void)s.advance_frame(1.0 / 60.0);
        const std::string a = leg(eu.to, eu.kind == MoveKind::JumpCatch ? "up (jump catch)" : "up");
        // Back down to where the climb began, by whatever way her scan gives.
        const std::string b = leg(eu.from, "down");
        std::cout << a << "; " << b << "\n";
        const bool ok = a.find("FAILED") == std::string::npos && b.find("FAILED") == std::string::npos;
        passed += ok ? 1 : 0;
        if (!ok) failed.push_back(a + "; " + b);
    }
    std::cout << "  matron ladders: " << passed << "/" << tried << " ladders up and down clean, " << refused
              << " refused by her scan (" << unexpected.size() << " not known)\n";
    for (const std::string &u : unexpected) std::cout << "  matron ladders: refused, not a known refusal: " << u << "\n";
    require(tried > 0 && passed == tried, "matron ladders: up and down every ladder, on its line, no hold lost");
    require(unexpected.empty(), "matron ladders: her scan refuses no ladder beyond the six refused for the world's steel");
    std::cout << "PASS scraperx_sim matron ladders: " << passed << "/" << tried << ", refused " << refused << "\n";
}
