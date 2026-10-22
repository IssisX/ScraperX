#include <array>
#include <chrono>
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
    void tick(Climber &climber) {
        climber.step(dt);
        peak_force = std::max(peak_force, climber.view().force_n);
        require(climber.view().force_n <= climber.limits().total_force_n + 0.5F, "matron climb: every tick's force inside her one budget");
        gravity_always = gravity_always && physics.GetBodyInterface().GetGravityFactor(her) == 1.0F;
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
    // A missing rung: she climbs to the gap and can go no higher; she lets go and falls.
    {
        ClimbWorld w;
        w.deck(5.0F);
        w.rungs(0.3F, 1.8F);
        w.rungs(3.3F, 4.9F);
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
    // A blocked mantle: a cabinet where she would stand over the head.
    {
        ClimbWorld w;
        w.deck(4.0F);
        w.rungs(0.3F, 3.9F);
        w.box(Vec3(1.5F, 1.2F, 0.8F), RVec3(0, 4.0F + 1.2F, -2.2F));   // a cabinet 0.4 m behind the lip
        w.spawn(RVec3(0, 0, 1.0F));
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        climber.begin(ladder_up(4.0F, 0.3F, 3.9F));
        float max_feet = 0.0F;
        const auto st = w.run(climber, 30.0F, [&](int) { max_feet = std::max(max_feet, w.feet()); });
        const float landed = w.settle();
        std::cout << "  matron climb blocked mantle: " << traversal::failure_name(climber.failure()) << " highest feet " << max_feet << "\n";
        require(st == Climber::Status::Failed && climber.failure() == Failure::BlockedSweep,
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
    // Out of range: a bar higher than her legs can put her hands.
    {
        ClimbWorld w;
        w.deck(7.0F);
        w.rungs(5.2F, 6.9F);
        w.spawn(RVec3(0, 0, 1.0F));
        Climber climber(w.physics, w.her, {w.her, BodyID()});
        MoveSpec far = ladder_up(7.0F, 5.2F, 6.9F);
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
            if (blocked) w.box(Vec3(1.5F, 1.0F, 0.8F), RVec3(0, 1.6F + 1.0F, -2.4F));   // a cabinet 0.6 m behind the lip
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
                require(st == Climber::Status::Failed && climber.failure() == Failure::BlockedSweep && highest < 0.1F,
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
void test_power_budget() {
    using traversal::Budget;
    Budget b{5600.0F, 0.0F, 9000.0F, 0.0F, Vec3(0, 3.0F, 0)};
    const Vec3 up = b.take(Vec3(0, 5000.0F, 0));   // 15 kW asked
    require(std::abs(up.GetY() - 3000.0F) < 1.0F && b.power_left < 1.0F, "matron power: 9 kW at 3 m/s is 3000 N up");
    const Vec3 side = b.take(Vec3(2000.0F, 0, 0));
    require(std::abs(side.GetX() - 2000.0F) < 1.0F, "matron power: force across her motion costs no power");
    const Vec3 brake = b.take(Vec3(0, -500.0F, 0));
    require(std::abs(brake.GetY() + 500.0F) < 1.0F, "matron power: braking is not power-limited");
    require(b.used <= 5600.0F + 1.0F, "matron power: force total still capped");
    Budget still{5600.0F, 0.0F, 9000.0F, 0.0F, Vec3::sZero()};
    require(std::abs(still.take(Vec3(0, 5000.0F, 0)).GetY() - 5000.0F) < 1.0F, "matron power: at rest, force cap only");
    // Riding a rising support: her world velocity is the lift's, her
    // velocity against it near zero, so power is not spent on the ride.
    {
        ClimbWorld w;
        const BodyID lift = w.box(Vec3(1.5F, 0.2F, 1.5F), RVec3(0, 1.0F, 6.0F), false, EMotionType::Kinematic);
        w.spawn(RVec3(0, 1.2F, 6.0F));
        Climber climber(w.physics, w.her, traversal::Ignore{w.her, BodyID()}, traversal::Limits{});
        for (int i = 0; i < 90; ++i) w.physics.Update(w.dt, 1, &w.temp, &w.jobs);
        w.physics.GetBodyInterface().SetLinearVelocity(lift, Vec3(0, 2.0F, 0));
        for (int i = 0; i < 90; ++i) {
            climber.walk_between(w.dt, Vec3(0, 0, 6.0F), 0.0F);
            w.physics.Update(w.dt, 1, &w.temp, &w.jobs);
        }
        const float world_vy = w.physics.GetBodyInterface().GetLinearVelocity(w.her).GetY();
        const float rel = climber.relative_velocity().Length();
        std::cout << "  matron power on a lift rising 2 m/s: world vy " << world_vy << ", relative " << rel << " m/s\n";
        require(world_vy > 1.8F && rel < 0.2F, "matron power: charged on velocity against the moving support, not the world");
        Budget riding{5600.0F, 0.0F, 9000.0F, 0.0F, climber.relative_velocity()};
        require(riding.take(Vec3(0, 5000.0F, 0)).GetY() > 4900.0F, "matron power: riding a lift does not eat her power");
    }
    std::cout << "  matron power: 9 kW cap clamps along-velocity force (3000 N at 3 m/s); sideways/braking free\n";
}

// A mantle onto a ledge that is lifted under her hands (a deck on a hoist):
// the braking rule. Power is charged only on positive work relative to the
// hold; when the lip jerks up away from her she brakes against it (force up,
// her velocity relative to it down), and that is capped by force alone. A
// 2.5 m/s jerk takes her 220 kg well past 9 kW of braking to stay on, which
// the power cap would have refused. She keeps her grip, arrives standing on
// the moving deck, and her positive power never exceeds 9 kW.
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
            const bool lifting = lift_ticks <= 18;   // 0.2 s at 2.5 m/s: the lip 0.5 m up
            bodies.SetLinearVelocity(deck, Vec3(0, lifting ? 2.5F : 0.0F, 0));
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
    require(peak_power <= 9000.0F + 1.0F && peak_force <= 5600.0F + 1.0F,
            "matron climb moving ledge: positive power within 9 kW, force within 5600 N");
    require(peak_brake > 9000.0F, "matron climb moving ledge: braking against the lifted lip went past 9 kW (force-capped only)");
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
        require(r.worst_step <= 0.20F && r.peak_force <= 5600.0F + 1.0F, "matron narrow ledge: no snapping, within her budget");
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

// Gripping a moving bar: power is charged on her velocity relative to the
// bar, so holding still on a bar that carries her (a lift, a swinging boom)
// costs force but no power, while pulling up along a bar that falls away
// from her is charged for the speed she makes on it, not over the ground.
void test_power_moving_bar() {
    using traversal::Budget;
    const Vec3 bar(0, 5.0F, 0);   // the bar rising at 5 m/s
    const float weight = Matron::kMassKg * 9.81F;
    // Carried by it, still on it: her whole weight held, no power spent.
    Budget carried{5600.0F, 0.0F, 9000.0F, 0.0F, bar - bar};
    const Vec3 hold = carried.take(Vec3(0, weight, 0));
    require(std::abs(hold.GetY() - weight) < 1.0F && carried.power_used < 1.0F,
            "matron power: holding still on a bar that carries her costs no power");
    // Charged over the ground instead, the same hold would be cut to 1800 N:
    // the cap would drop her off any moving support.
    Budget ground{5600.0F, 0.0F, 9000.0F, 0.0F, bar};
    require(ground.take(Vec3(0, weight, 0)).GetY() < weight - 300.0F,
            "matron power: (control) charged over the ground the hold would be cut");
    // Climbing it at 2 m/s relative while it rises at 5: charged at 2 m/s.
    Budget climbing{5600.0F, 0.0F, 9000.0F, 0.0F, Vec3(0, 7.0F, 0) - bar};
    const Vec3 pull = climbing.take(Vec3(0, 5000.0F, 0));
    require(std::abs(pull.GetY() - 4500.0F) < 1.0F && climbing.power_used <= 9000.0F + 1.0F,
            "matron power: climbing a moving bar is charged on her speed along it (9 kW at 2 m/s is 4500 N)");
    // A bar dropping away under her hands: braking against it is free.
    Budget falling{5600.0F, 0.0F, 9000.0F, 0.0F, Vec3(0, 0, 0) - Vec3(0, -3.0F, 0)};
    const Vec3 brake = falling.take(Vec3(0, -2000.0F, 0));
    require(std::abs(brake.GetY() + 2000.0F) < 1.0F && falling.power_used < 1.0F,
            "matron power: slowing herself against a falling bar is not power-limited");
    std::cout << "PASS scraperx_sim matron power moving bar: hold_n=" << hold.GetY() << " climb_n=" << pull.GetY() << "\n";
}

namespace {

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

} // namespace

void run_matron_climb() {
    test_power_budget();
    test_power_moving_bar();
    test_rungs();
    test_rung_failures();
    test_pole();
    test_jump();
    test_drops_and_mantle();
    test_dynamic_support();
    test_mantle_moving_ledge();
    test_bump_mid_lower();
    test_wall_across_step();
    test_rung_gaps();
    test_pole_head_clearance();
    test_narrow_ledges();
    test_stand_on_board();
    test_knocked_onto_board();
}

namespace {
} // namespace

void run_matron_climb_board() { test_narrow_board(); }
