#include <array>
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
    std::cout << "  matron power: 9 kW cap clamps along-velocity force (3000 N at 3 m/s); sideways/braking free\n";
}

} // namespace

void run_matron_climb() {
    test_power_budget();
    test_rungs();
    test_rung_failures();
    test_pole();
    test_jump();
    test_drops_and_mantle();
    test_dynamic_support();
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
bool journey_leg(Simulation &s, const traversal::AffordanceMap &map, const double top, const double limit_s) {
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
}
} // namespace

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
    std::cout << "  player ladder climb alone " << alone_s << " s to " << alone_y << ", with her travelling " << near_s
              << " s to " << near_y << "\n";
    require(std::abs(alone_s - near_s) <= 0.25 && std::abs(alone_y - near_y) <= 0.1,
            "player climbing is the same with her travelling nearby");
}

void run_matron_climb_board() { test_narrow_board(); test_player_unchanged(); }

void run_matron_climb_scan() {
    test_tower_scan();
    test_tower_journeys();
}
