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
} // namespace

void run_matron_climb_board() { test_narrow_board(); }
