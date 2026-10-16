#include "sim/yard_crane.hpp"
#include "sim/warehouse_forklift_yard.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace scraperx::sim;
using namespace JPH;
namespace {
void require(bool ok, const char *why) {
    if (!ok) { std::cerr << "FAIL yard crane: " << why << std::endl; std::exit(1); }
}
struct BP final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer v) const override { return BroadPhaseLayer(v); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "crane"; }
};
struct BF final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override { return a || b.GetValue(); }
};
struct LF final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override { return a || b; }
};
struct Fixture {
    BP bp; BF bf; LF lf;
    PhysicsSystem world;
    TempAllocatorImpl temp{64 * 1024 * 1024};
    JobSystemSingleThreaded jobs{2048};
    std::unique_ptr<kit::Kit> kit;
    std::unique_ptr<WarehouseForklift> truck;
    std::unique_ptr<YardCrane> crane;
    struct WeldSteps final : PhysicsStepListener {
        kit::Kit *kit = nullptr;
        void OnStep(const PhysicsStepListenerContext &c) override { kit->begin_weld_step(c.mDeltaTime); }
    } weld_steps;
    Fixture() {
        world.Init(256, 0, 2048, 16384, bp, bf, lf);
        world.SetGravity(Vec3(0, -9.81F, 0));
        kit = std::make_unique<kit::Kit>(world, 0, 1);
        kit::Part ground; ground.half = Vec3(100, .25F, 100);
        kit->add_body(1400, {ground}, RVec3(-65, -.25, -150), Quat::sIdentity(), 0, .8F);
        // Authored initial assembly for the mechanism fixture. The full world
        // must drive its existing truck here; no operation below writes poses.
        const auto yaw = Quat::sRotation(Vec3::sAxisY(), JPH_PI * .5F);
        const auto yard = build_warehouse_forklift_yard(*kit, RVec3(-94, 0, -150), yaw);
        truck = std::make_unique<WarehouseForklift>(world, *kit, yard.truck);
        crane = std::make_unique<YardCrane>(world, *kit, yard.truck);
        weld_steps.kit = kit.get(); world.AddStepListener(&weld_steps);
        world.OptimizeBroadPhase();
    }
    ~Fixture() {
        world.RemoveStepListener(&weld_steps); crane.reset(); truck.reset(); kit.reset();
    }
    void tick(YardCrane::Command command = {}, unsigned n = 1,
              WarehouseForklift::Command drive = {}) {
        while (n--) {
            kit->pre_step(1.F / 90); truck->pre_step(drive); crane->pre_step(command);
            require(world.Update(1.F / 90, 4, &temp, &jobs) == EPhysicsUpdateError::None, "native update");
            kit->finish_weld_steps(); truck->post_step(); crane->post_step(); kit->post_step(1.F / 90);
        }
    }
    double y() { return kit->body_position(truck->config().chassis).GetY(); }
    void stow_transport() {
        // Raise while the mast is neutral so forward tilt cannot ground
        // the long tips and lift the front tires off their support.
        WarehouseForklift::Command raise; raise.lift = 1;
        for (unsigned i = 0; i < 360 && truck->state().lift_position_m < .35F; ++i) tick({}, 1, raise);
        require(truck->state().lift_position_m >= .35F, "finite hydraulics raise actual carriage for tine clearance");
        tick({}, 30);
        WarehouseForklift::Command stow; stow.tilt = -1;
        const auto ready = [&] {
            const auto s = truck->state();
            return std::abs(s.tilt_angle_rad - truck->config().tilt_min_rad) < .002F;
        };
        for (unsigned i = 0; i < 360 && !ready(); ++i) tick({}, 1, stow);
        require(ready(), "finite truck hydraulics reach actual forward transport stop with raised forks");
        tick({}, 90);
    }
    void align_empty_rig(bool first_leg_only = false) {
        // Guide the actual empty rig through its finite drives. The free ends
        // remain the vertically hanging ends of the authored fixed stock.
        const auto eyes = WarehouseForkliftYard::lifting_points();
        const auto &g = crane->geometry();
        const Vec3 initial_axis = kit->body_rotation(g.boom) * Vec3::sAxisZ();
        const auto initial = crane->state();
        float target_trolley = initial.trolley_position_m;
        const auto truck_frame = world.GetBodyInterface().GetWorldTransform(kit->body_id(truck->config().chassis));
        for (unsigned leg = 0; leg < eyes.size(); ++leg)
            target_trolley += Vec3(truck_frame * eyes[leg] - initial.loose_ends[leg]).Dot(initial_axis) / float(eyes.size());
        bool aligned = false;
        for (unsigned i = 0; i < 1800; ++i) {
            const auto s = crane->state();
            const auto *chassis = world.GetBodyLockInterfaceNoLock().TryGetBody(kit->body_id(truck->config().chassis));
            const auto *spreader = world.GetBodyLockInterfaceNoLock().TryGetBody(kit->body_id(g.spreader));
            const Vec3 axis = kit->body_rotation(g.boom) * Vec3::sAxisZ();
            float excess = -100;
            bool fits = true;
            for (unsigned leg = 0; leg < eyes.size(); ++leg) {
                const auto eye = chassis->GetWorldTransform() * eyes[leg];
                const auto top = spreader->GetWorldTransform() * g.spreader_eyes[leg];
                const Vec3 error(eye - s.loose_ends[leg]);
                const Vec3 speed(chassis->GetPointVelocity(eye) - spreader->GetPointVelocity(s.loose_ends[leg]));
                excess = std::max(excess, float((eye-top).Length()) - g.sling_lengths[leg]);
                fits = fits && error.LengthSq() <= .06F * .06F &&
                       (eye-top).Length() <= g.sling_lengths[leg] + .0001F && speed.Length() <= .15F;
            }
            bool supported = chassis->GetLinearVelocity().Length() < .10F &&
                             chassis->GetAngularVelocity().Length() < .05F;
            for (const auto &wheel : truck->wheels())
                supported = supported && wheel.contact && wheel.contact_normal.GetY() >= .7F && wheel.normal_force_n > 0;
            if (fits && supported) {
                tick({}, 90);
                if (first_leg_only ? crane->try_attach_leg(0, *truck) : crane->try_attach(*truck)) { aligned = true; break; }
                continue;
            }
            YardCrane::Command guide;
            // Move the finite carriage to one measured pickup target. Chasing
            // the long suspended pendulum directly would excite its swing.
            const auto *trolley = world.GetBodyLockInterfaceNoLock().TryGetBody(kit->body_id(g.trolley));
            const float carriage_error = target_trolley - s.trolley_position_m;
            if (std::abs(carriage_error) > .001F)
                guide.trolley = std::clamp(4 * carriage_error - 4 * trolley->GetLinearVelocity().Dot(axis), -.2F, .2F);
            // Pay out only until every actual endpoint is inside fixed stock;
            // the passive neutral brake then holds that attained target.
            if (excess > -.001F) guide.hoist = -.02F;
            tick(guide);
            if (crane->state().failure != YardCrane::Failure::None) break;
        }
        if (!aligned) trace("ALIGNMENT_FAILURE");
        require(aligned, "finite trolley and hoist align supported truck with real fixed-stock rig");
    }
    static void vector(std::ostream &out, Vec3Arg v) {
        out << v.GetX() << ',' << v.GetY() << ',' << v.GetZ();
    }
    void first_loss_geometry() {
        const auto &cfg = truck->config(); const auto &g = crane->geometry();
        for (const auto index : {cfg.chassis, cfg.mast, cfg.carriage, cfg.fork_frame,
                                 g.spreader, g.trolley, g.boom}) {
            const auto *b = world.GetBodyLockInterfaceNoLock().TryGetBody(kit->body_id(index));
            const auto q = b->GetRotation();
            std::cout << "FIRSTLOSS_BODY id=" << b->GetID().GetIndexAndSequenceNumber() << " origin=";
            vector(std::cout, Vec3(b->GetPosition())); std::cout << " com=";
            vector(std::cout, Vec3(b->GetCenterOfMassPosition())); std::cout << " shape_com=";
            vector(std::cout, b->GetShape()->GetCenterOfMass());
            std::cout << " rotation=" << q.GetX() << ',' << q.GetY() << ',' << q.GetZ() << ',' << q.GetW()
                      << " mass=" << (b->IsDynamic() ? 1.F / b->GetMotionProperties()->GetInverseMass() : 0)
                      << " velocity=";
            vector(std::cout, b->GetLinearVelocity()); std::cout << " omega=";
            vector(std::cout, b->GetAngularVelocity()); std::cout << std::endl;
        }
        const auto *mast = world.GetBodyLockInterfaceNoLock().TryGetBody(kit->body_id(cfg.mast));
        const auto inverse_mast = mast->GetWorldTransform().InversedRotationTranslation();
        unsigned pi = 0;
        for (const auto &part : kit->body_parts(cfg.mast)) {
            std::cout << "FIRSTLOSS_MAST_PART index=" << pi++ << " center=";
            vector(std::cout, part.offset); std::cout << " half="; vector(std::cout, part.half);
            std::cout << std::endl;
        }
        const auto points = WarehouseForkliftYard::lifting_points();
        for (unsigned i = 0; i < points.size(); ++i) {
            const auto top = world.GetBodyInterface().GetWorldTransform(kit->body_id(g.spreader)) * g.spreader_eyes[i];
            const auto bottom = world.GetBodyInterface().GetWorldTransform(kit->body_id(cfg.chassis)) * points[i];
            std::cout << "FIRSTLOSS_SEGMENT leg=" << i << " top_world=";
            vector(std::cout, Vec3(top)); std::cout << " bottom_world="; vector(std::cout, Vec3(bottom));
            std::cout << " top_mast_local="; vector(std::cout, Vec3(inverse_mast * top));
            std::cout << " bottom_mast_local="; vector(std::cout, Vec3(inverse_mast * bottom));
            std::cout << std::endl;
            IgnoreMultipleBodiesFilter filter;
            filter.IgnoreBody(kit->body_id(g.spreader)); filter.IgnoreBody(kit->body_id(cfg.chassis));
            RefConst<Shape> probe = new SphereShape(.025F);
            const RShapeCast cast(probe.GetPtr(), Vec3::sReplicate(1), RMat44::sTranslation(top), Vec3(bottom-top));
            for (const auto base : {top, RVec3::sZero()}) {
                ClosestHitCollisionCollector<CastShapeCollector> hit;
                world.GetNarrowPhaseQuery().CastShape(cast, ShapeCastSettings(), base, hit, {}, {}, filter);
                std::cout << "FIRSTLOSS_CAST leg=" << i << " base="; vector(std::cout, Vec3(base));
                std::cout << " hit=" << hit.HadHit();
                if (hit.HadHit()) {
                    std::cout << " body=" << hit.mHit.mBodyID2.GetIndexAndSequenceNumber()
                              << " subshape=" << hit.mHit.mSubShapeID2.GetValue() << " fraction=" << hit.mHit.mFraction
                              << " contact_world=";
                    const auto contact = base + hit.mHit.mContactPointOn2;
                    vector(std::cout, Vec3(contact)); std::cout << " contact_mast_local=";
                    vector(std::cout, Vec3(inverse_mast * contact));
                    std::cout << " sphere_center_mast_local=";
                    vector(std::cout, Vec3(inverse_mast * cast.GetPointOnRay(hit.mHit.mFraction)));
                }
                std::cout << std::endl;
            }
        }
    }
    void trace(const char *phase) {
        const auto s = crane->state(); const auto p = kit->body_position(truck->config().chassis);
        std::cout << phase << " truck=" << p.GetX() << ',' << p.GetY() << ',' << p.GetZ()
                  << " failure=" << unsigned(s.failure) << " rig=" << unsigned(s.attached_mask)
                  << " hoist=" << s.hoist_connected << " force=" << s.hoist.tensile_force_n
                  << " source=" << s.hoist.source_energy_used_j << " rest=" << s.hoist.rest_length_m
                  << " actual=" << s.hoist.actual_length_m
                  << " truck_lift=" << truck->state().lift_position_m
                  << " truck_tilt=" << truck->state().tilt_angle_rad << std::endl;
        const auto *chassis = world.GetBodyLockInterfaceNoLock().TryGetBody(kit->body_id(truck->config().chassis));
        std::cout << " SUPPORT speed=" << chassis->GetLinearVelocity().Length()
                  << " angular_speed=" << chassis->GetAngularVelocity().Length() << std::endl;
        const auto wheels = truck->wheels();
        const auto plane_origin = wheels[0].contact_position;
        Vec3 plane_normal = Vec3(wheels[1].contact_position-plane_origin).Cross(Vec3(wheels[2].contact_position-plane_origin));
        if (plane_normal.LengthSq() > 1e-8F) {
            plane_normal = plane_normal.Normalized();
            if (plane_normal.GetY() < 0) plane_normal = -plane_normal;
            float clearance = 1000;
            const auto frame = world.GetBodyInterface().GetWorldTransform(kit->body_id(truck->config().fork_frame));
            for (const auto &part : kit->body_parts(truck->config().fork_frame))
                for (float x : {-1.F,1.F}) for (float y : {-1.F,1.F}) for (float z : {-1.F,1.F}) {
                    const auto vertex = frame * (part.offset + part.rotation * Vec3(x*part.half.GetX(),y*part.half.GetY(),z*part.half.GetZ()));
                    clearance = std::min(clearance, Vec3(vertex-plane_origin).Dot(plane_normal));
                }
            std::cout << " FORK min_plane_clearance=" << clearance << std::endl;
        }
        unsigned wheel_index = 0;
        for (const auto &wheel : truck->wheels())
            std::cout << " WHEEL " << wheel_index++ << " contact=" << wheel.contact
                      << " normal_y=" << wheel.contact_normal.GetY() << " force=" << wheel.normal_force_n << std::endl;
        const auto points = WarehouseForkliftYard::lifting_points();
        for (unsigned i = 0; i < points.size(); ++i) {
            const auto eye = world.GetBodyInterface().GetWorldTransform(kit->body_id(truck->config().chassis)) * points[i];
            const Vec3 delta(eye - s.loose_ends[i]);
            std::cout << " EYE " << i << " delta=" << delta.GetX() << ',' << delta.GetY() << ',' << delta.GetZ()
                      << " tension=" << s.slings[i].tensile_force_n;
            const auto *spreader = world.GetBodyLockInterfaceNoLock().TryGetBody(kit->body_id(crane->geometry().spreader));
            std::cout << " relative_speed=" << (chassis->GetPointVelocity(eye) - spreader->GetPointVelocity(s.loose_ends[i])).Length();
            const auto top = world.GetBodyInterface().GetWorldTransform(kit->body_id(crane->geometry().spreader)) * crane->geometry().spreader_eyes[i];
            std::cout << " length=" << (eye-top).Length() << " stock=" << crane->geometry().sling_lengths[i];
            IgnoreMultipleBodiesFilter filter;
            filter.IgnoreBody(kit->body_id(crane->geometry().spreader)); filter.IgnoreBody(kit->body_id(truck->config().chassis));
            ClosestHitCollisionCollector<CastShapeCollector> hit;
            RefConst<Shape> probe = new SphereShape(.025F);
            const RShapeCast cast(probe.GetPtr(), Vec3::sReplicate(1), RMat44::sTranslation(top), Vec3(eye-top));
            world.GetNarrowPhaseQuery().CastShape(cast, ShapeCastSettings(), top, hit, {}, {}, filter);
            if (hit.HadHit()) std::cout << " obstruction_body=" << hit.mHit.mBodyID2.GetIndexAndSequenceNumber();
            std::cout << std::endl;
        }
    }
};
void individual_legs() {
    Fixture f; f.tick({}, 180); f.stow_transport();
    const auto constraints = f.world.GetConstraints().size();
    require(!f.crane->try_attach_leg(4, *f.truck) && !f.crane->try_detach_leg(4, *f.truck),
            "invalid leg transactions reject without mutation");
    f.align_empty_rig(true);
    require(f.crane->state().attached_mask == 1 && f.world.GetConstraints().size() == constraints + 1,
            "first real leg alone creates exactly one joint");
    require(!f.crane->try_attach_leg(0, *f.truck) && !f.crane->try_attach(*f.truck),
            "duplicate leg and all-four replacement reject existing partial rig");
    require(f.crane->try_attach_leg(3, *f.truck) && f.crane->state().attached_mask == 9 &&
            f.crane->try_attach_leg(1, *f.truck) && f.crane->state().attached_mask == 11 &&
            f.crane->try_attach_leg(2, *f.truck) && f.crane->state().attached_mask == 15,
            "remaining real legs attach individually in arbitrary order");
    require(f.world.GetConstraints().size() == constraints + 4 && !f.crane->try_detach_leg(2, *f.truck),
            "individual release refuses insufficient slack without removing joint");
    YardCrane::Command down; down.hoist = -.1F;
    f.tick(down, 90); f.tick({}, 90);
    for (unsigned leg : {2U, 0U, 3U, 1U}) {
        const auto before = f.crane->state().attached_mask;
        require(f.crane->try_detach_leg(leg, *f.truck) &&
                f.crane->state().attached_mask == (before & ~(1U << leg)),
                "supported slack individual release removes only selected real leg");
        require(!f.crane->try_detach_leg(leg, *f.truck), "already released leg rejects duplicate release");
    }
    require(f.world.GetConstraints().size() == constraints, "individual lifecycle restores original joint count");
    require(f.crane->try_attach(*f.truck), "atomic four-leg pickup remains available after individual release");
    require(f.crane->try_detach(*f.truck) && f.crane->state().attached_mask == 0,
            "atomic slack four-leg release remains available");
    std::cout << "PASS actual individual four-leg assembly slack release and atomic compatibility\n";
}
void restore_slack_hoist() {
    Fixture f;
    YardCrane::Command up; up.hoist = 1;
    f.tick(up, 9);
    const auto &g = f.crane->geometry();
    const auto trolley = f.kit->body_id(g.trolley);
    const auto spreader = f.kit->body_id(g.spreader);
    auto &bodies = f.world.GetBodyInterface();
    const auto support = bodies.GetWorldTransform(trolley) * g.hoist_support_eye;
    const auto load = bodies.GetWorldTransform(spreader) * g.hoist_load_eye;
    // Author a body snapshot with slack cable. Endpoint separation is not the
    // stored stock length and may legitimately fall below the payout minimum.
    bodies.SetPosition(spreader, bodies.GetPosition(spreader) + (support - load) - Vec3(0, 2, 0),
                       EActivation::Activate);
    f.crane->post_step();
    const auto saved = f.crane->state();
    const auto position = bodies.GetPosition(spreader);
    const auto rotation = bodies.GetRotation(spreader);
    const auto velocity = bodies.GetLinearVelocity(spreader);
    const auto constraints = f.world.GetConstraints().size();
    require(saved.hoist_connected && saved.hoist.actual_length_m < 4 &&
            saved.hoist.rest_length_m >= 4 && saved.hoist.source_energy_used_j > 0,
            "snapshot contains connected slack hoist with valid rest and spent finite inventory");
    try { f.crane->restore(saved); }
    catch (const std::exception &e) {
        std::cerr << "RESTORE_EXCEPTION " << e.what() << std::endl;
        require(false, "valid slack hoist snapshot restores without constructor rejection");
    }
    const auto restored = f.crane->state();
    require(restored.hoist_connected && restored.hoist.rest_length_m == saved.hoist.rest_length_m &&
            restored.hoist.energy_j == saved.hoist.energy_j &&
            restored.hoist.source_energy_used_j == saved.hoist.source_energy_used_j &&
            restored.hoist.positive_motor_work_j == saved.hoist.positive_motor_work_j &&
            restored.hoist.command_work_bound_j == saved.hoist.command_work_bound_j &&
            restored.hoist.actual_length_m == saved.hoist.actual_length_m,
            "restore preserves saved cable stock, work and real slack endpoint geometry");
    require(f.kit->body_id(g.trolley) == trolley && f.kit->body_id(g.spreader) == spreader &&
            bodies.GetPosition(spreader) == position && bodies.GetRotation(spreader) == rotation &&
            bodies.GetLinearVelocity(spreader) == velocity &&
            f.world.GetConstraints().size() == constraints,
            "restore preserves native bodies and pose with exactly one hoist constraint");
    require(restored.hoist.target_speed_mps == 0 && restored.hoist.tensile_force_n == 0 &&
            restored.hoist.braking, "restored hoist resets transient command and solver force");
    std::cout << "PASS yard crane valid slack hoist restoration preserves stock, finite inventory and bodies\n";
}
void supported_slack_leg_release() {
    Fixture f; f.tick({}, 180); f.stow_transport(); f.align_empty_rig();
    YardCrane::Command down; down.hoist = -1;
    f.tick(down, 90); f.tick({}, 90);
    const auto before = f.crane->state();
    const auto target = f.world.GetBodyInterface().GetWorldTransform(f.kit->body_id(f.truck->config().chassis)) *
                        WarehouseForkliftYard::lifting_points()[2];
    const double free_end_gap = (target - before.loose_ends[2]).Length();
    std::cout << "SLACK_RELEASE free_end_gap=" << free_end_gap << " force=" << before.slings[2].tensile_force_n
              << " slack=" << before.slings[2].rest_length_m - before.slings[2].actual_length_m << std::endl;
    require(before.attached_mask == 15 && free_end_gap > .06 && before.slings[2].tensile_force_n <= 150,
            "real finite payout creates unloaded attached leg beyond loose-end pickup capture");
    const auto position = f.kit->body_position(f.truck->config().chassis);
    const auto velocity = f.kit->body_velocity(f.truck->config().chassis);
    require(f.crane->try_detach_leg(2, *f.truck), "supported actual slack leg releases without loose-end pickup alignment");
    require(f.crane->state().attached_mask == 11 &&
            f.kit->body_position(f.truck->config().chassis) == position &&
            f.kit->body_velocity(f.truck->config().chassis) == velocity,
            "only reached leg releases without changing actual truck pose or momentum");
    std::cout << "PASS supported slack actual leg releases beyond pickup capture\n";
}
void slow_controls() {
    Fixture f; f.tick({}, 180);
    const auto before = f.crane->state();
    YardCrane::Command command; command.slew = .005F; command.trolley = .002F;
    f.tick(command, 900);
    const auto moved = f.crane->state();
    const double angle = std::remainder(double(moved.slew_angle_rad - before.slew_angle_rad), 2 * JPH_PI);
    const double travel = moved.trolley_position_m - before.trolley_position_m;
    std::cout << "SLOW_CONTROLS angle=" << angle << " trolley=" << travel
              << " target_omega=" << moved.slew_target_rad_s
              << " target_speed=" << moved.trolley_target_mps
              << " energy=" << moved.transport_energy_j << std::endl;
    require(moved.failure == YardCrane::Failure::None, "slow actual control retains real cable clearance");
    require(angle > .001 && angle < .005, "small finite slew integrates actual boom rotation without a numerical input floor");
    require(travel > .0025 && travel < .01, "small finite trolley input integrates actual world displacement");
    require(moved.transport_energy_j < before.transport_energy_j && moved.transport_positive_work_j > 0,
            "small actual motion consumes existing finite transport source");
    std::cout << "PASS slow real crane controls integrate below ordinary rigid rounding thresholds\n";
}
void loaded_cycle() {
    Fixture f; f.tick({}, 180); f.trace("SETTLED");
    require(!f.crane->try_attach(*f.truck), "unstowed pickup is rejected");
    f.stow_transport(); f.trace("STOWED");
    kit::Kit::Checkpoint bodies; f.kit->capture(bodies);
    WarehouseForklift::Checkpoint truck_before; f.truck->capture(truck_before);
    const auto crane_before = f.crane->state();
    const auto id = f.kit->body_id(f.truck->config().chassis);
    const auto constraints = f.world.GetConstraints().size();
    f.align_empty_rig(); f.trace("ALIGNED");
    require(f.crane->state().attached_mask == 15, "real four-leg hardware aligns at pickup");
    const double start = f.y();
    YardCrane::Command up; up.hoist = 1;
    for (unsigned i = 0; i < 360; ++i) {
        f.tick(up);
        if (f.crane->state().failure != YardCrane::Failure::None) {
            std::cout << "FIRST_FAILURE lift_tick=" << i + 1 << std::endl;
            f.trace("FIRST_FAILURE"); f.first_loss_geometry(); break;
        }
    }
    f.trace("LIFTED");
    const auto lifted = f.crane->state();
    require(lifted.failure == YardCrane::Failure::None && lifted.attached_mask == 15,
            "full real rig remains connected during loaded lift");
    require(f.y() > start + 1, "finite hoist raises actual production forklift more than one meter");
    const auto *chassis = f.world.GetBodyLockInterfaceNoLock().TryGetBody(id);
    const double chassis_mass = 1. / chassis->GetMotionProperties()->GetInverseMass();
    require(lifted.hoist.source_energy_used_j > chassis_mass * 9.81 * (f.y() - start),
            "loaded ascent spends finite source against actual chassis mass and rise");
    require(!f.crane->try_detach(*f.truck), "suspended truck refuses rig release");
    require(!f.crane->try_detach_leg(2, *f.truck), "suspended actual leg refuses individual release");
    const auto held = f.y(); f.tick({}, 180); f.trace("HELD");
    require(std::abs(f.y() - held) < .03, "neutral articulated crane holds within30mm");
    require(f.crane->state().hoist.source_energy_used_j == lifted.hoist.source_energy_used_j,
            "neutral hold draws no hoist source");
    require(f.crane->state().hoist.positive_motor_work_j == lifted.hoist.positive_motor_work_j,
            "neutral hold supplies no positive motor event work");
    YardCrane::Command down; down.hoist = -1; bool detached = false;
    for (unsigned i = 0; i < 540; ++i) {
        f.tick(down);
        if (f.crane->try_detach(*f.truck)) { detached = true; break; }
    }
    f.trace("LANDED");
    require(detached && f.crane->state().attached_mask == 0 && std::abs(f.y() - start) < .03,
            "actual wheels support truck and slack rig releases without pose writes");
    require(f.kit->body_id(f.truck->config().chassis) == id, "same truck native identity survives crane transaction");
    f.tick({}, 30); const auto before_drive = f.kit->body_position(f.truck->config().chassis);
    WarehouseForklift::Command drive; drive.drive = .3F; drive.brake = 0;
    f.tick({}, 90, drive);
    require(f.kit->body_position(f.truck->config().chassis).GetX() > before_drive.GetX() + .2,
            "released production truck drives on actual support");
    f.kit->restore(bodies); f.truck->restore(truck_before); f.crane->restore(crane_before); f.tick({}, 30);
    f.align_empty_rig();
    require(f.world.GetConstraints().size() == constraints + 4 && f.crane->state().attached_mask == 15,
            "explicit retry restores owner inventory without duplicated joints");
    f.trace("RETRY");
}
}
int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    RegisterDefaultAllocator(); Factory::sInstance = new Factory; RegisterTypes();
    if (argc > 1 && std::string(argv[1]) == "--individual-legs") individual_legs();
    else if (argc > 1 && std::string(argv[1]) == "--restore-slack") restore_slack_hoist();
    else if (argc > 1 && std::string(argv[1]) == "--slow-controls") slow_controls();
    else if (argc > 1 && std::string(argv[1]) == "--loaded-cycle") loaded_cycle();
    else if (argc > 1 && std::string(argv[1]) == "--slack-leg-release") supported_slack_leg_release();
    else {
        supported_slack_leg_release();
        slow_controls();
        restore_slack_hoist();
        loaded_cycle();
        std::cout << "PASS yard crane assembled native lift hold supported release drive and retry; full world transfer pending\n";
    }
    UnregisterTypes(); delete Factory::sInstance; Factory::sInstance = nullptr;
}
