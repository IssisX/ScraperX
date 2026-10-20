#include "sim/warehouse_forklift_yard.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>

using namespace JPH;
using namespace scraperx::sim;
namespace {
constexpr float dt = 1.F / 90;
constexpr std::uint64_t load_entity = 2499;
void require(bool condition, const char *why) {
    if (!condition) { std::cerr << "FAIL load stability: " << why << '\n'; std::exit(1); }
}
struct Layers final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override { return BroadPhaseLayer(layer); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "load stability"; }
};
struct BroadFilter final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override { return a || b.GetValue(); }
};
struct PairFilter final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override { return a || b; }
};
struct Contacts final : ContactListener {
    bool fork_load = false;
    std::set<SubShapeIDPair> load_ground;
    void record(const Body &a, const Body &b, const ContactManifold &manifold) {
        if ((a.GetUserData() == WarehouseForkliftYard::kForkEntity && b.GetUserData() == load_entity) ||
            (b.GetUserData() == WarehouseForkliftYard::kForkEntity && a.GetUserData() == load_entity)) fork_load = true;
        if ((a.GetUserData() == 1399 && b.GetUserData() == load_entity) ||
            (b.GetUserData() == 1399 && a.GetUserData() == load_entity))
            load_ground.emplace(a.GetID(), manifold.mSubShapeID1, b.GetID(), manifold.mSubShapeID2);
    }
    void OnContactAdded(const Body &a, const Body &b, const ContactManifold &m, ContactSettings &) override { record(a, b, m); }
    void OnContactPersisted(const Body &a, const Body &b, const ContactManifold &m, ContactSettings &) override { record(a, b, m); }
    void OnContactRemoved(const SubShapeIDPair &pair) override { load_ground.erase(pair); }
};
kit::Part box(Vec3 half, float mass) {
    kit::Part p; p.half = half; p.mass_kg = mass; p.convex_radius = .005F; p.material = kit::Material::Steel; return p;
}
struct Fixture {
    Layers layers; BroadFilter broad; PairFilter pairs; PhysicsSystem world;
    TempAllocatorImpl temp{32 * 1024 * 1024}; JobSystemSingleThreaded jobs{1024}; Contacts contacts;
    std::unique_ptr<kit::Kit> kit;
    WarehouseForkliftYard yard;
    std::unique_ptr<WarehouseForklift> truck;
    kit::BodyIndex load;
    explicit Fixture(float center_z) {
        world.Init(128, 0, 512, 2048, layers, broad, pairs);
        world.SetGravity(Vec3(0, -9.81F, 0)); world.SetContactListener(&contacts);
        kit = std::make_unique<kit::Kit>(world, 0, 1);
        kit->add_body(1399, {box(Vec3(30, .1F, 50), 0)}, RVec3(0, -.1, 0), Quat::sIdentity(), 0, .8F);
        // Initial authoring frame only. All four actual production bodies,
        // geometry, mass properties and actuator configuration remain intact.
        yard = build_warehouse_forklift_yard(*kit, RVec3(0, 0, -14));
        truck = std::make_unique<WarehouseForklift>(world, *kit, yard.truck);
        constexpr float mass = 1700, density = 7800;
        const Vec3 half(.5F, .4F, mass / (8 * .5F * .4F * density));
        load = kit->add_body(load_entity, {box(half, mass)}, RVec3(0, .62, center_z), Quat::sIdentity(), mass, .85F);
        kit->set_continuous_collision(load);
        world.OptimizeBroadPhase();
    }
    ~Fixture() { truck.reset(); kit.reset(); world.SetContactListener(nullptr); }
    void tick(const WarehouseForklift::Command &command = {}) {
        const auto previous_ground = contacts.load_ground;
        contacts.load_ground.clear();
        kit->pre_step(dt); truck->pre_step(command);
        require(world.Update(dt, 4, &temp, &jobs) == EPhysicsUpdateError::None, "native update");
        // Jolt also emits OnContactRemoved when a body sleeps. This fixed
        // floor never moves: retain its last solved contact while the load
        // is inactive, and require fresh contact again whenever it wakes.
        if (!world.GetBodyInterface().IsActive(kit->body_id(load)) && contacts.load_ground.empty())
            contacts.load_ground = previous_ground;
        truck->post_step(); kit->post_step(dt); kit->finish_weld_steps();
    }
    float pitch() const {
        const auto up = kit->body_rotation(yard.truck.chassis) * Vec3::sAxisY();
        return std::atan2(up.GetZ(), up.GetY()); // positive means truck nose down
    }
    float native_mass(kit::BodyIndex index) const {
        BodyLockRead lock(world.GetBodyLockInterface(), kit->body_id(index));
        require(lock.Succeeded(), "native mass body lock");
        return 1.F / lock.GetBody().GetMotionProperties()->GetInverseMass();
    }
    void observe(const char *label, unsigned tick_index) const {
        const auto state = truck->state(); const auto wheels = truck->wheels();
        const auto cp = kit->body_center_of_mass_position(yard.truck.chassis);
        const auto lp = kit->body_center_of_mass_position(load);
        std::cout << label << " tick=" << tick_index << " pitch=" << pitch()
                  << " rear_contact=" << wheels[2].contact << " rear_N=" << wheels[2].normal_force_n
                  << " front_N=" << wheels[0].normal_force_n + wheels[1].normal_force_n
                  << " front_z=" << wheels[0].contact_position.GetZ()
                  << " chassis_com=" << cp.GetX() << ',' << cp.GetY() << ',' << cp.GetZ()
                  << " load_com=" << lp.GetX() << ',' << lp.GetY() << ',' << lp.GetZ()
                  << " load_ground=" << !contacts.load_ground.empty()
                  << " lift=" << state.lift_position_m << " force_N=" << state.lift_force_n
                  << " tilt_rad=" << state.tilt_angle_rad << " tilt_Nm=" << state.tilt_torque_nm
                  << " work_J=" << state.hydraulic_positive_work_j
                  << " battery_J=" << state.battery_j << " excess_J=" << state.reservation_excess_j << '\n';
    }
};
void load_center_case(float center_z, bool expect_tip) {
    Fixture f(center_z);
    double restoring = 0, truck_mass = 0;
    for (const auto body : {f.yard.truck.chassis, f.yard.truck.mast, f.yard.truck.carriage, f.yard.truck.fork_frame}) {
        const float mass = f.native_mass(body); const auto com = f.kit->body_center_of_mass_position(body);
        truck_mass += mass; restoring += mass * (.75 - com.GetZ());
        std::cout << "MASS entity=" << f.kit->body_entity(body) << " kg=" << mass
                  << " com=" << com.GetX() << ',' << com.GetY() << ',' << com.GetZ() << '\n';
    }
    const float load_mass = f.native_mass(f.load);
    std::cout << "CASE center_z=" << center_z << " truck_kg=" << truck_mass << " load_kg=" << load_mass
              << " restoring_kgm=" << restoring << " load_kgm=" << load_mass * (center_z - .75F)
              << " predicted_tip=" << expect_tip << '\n';
    require((load_mass * (center_z - .75F) > restoring) == expect_tip, "actual initial COM predicts opposite static stability");
    for (unsigned i = 0; i < 180; ++i) f.tick();
    f.observe("SETTLED", 180);
    require(std::abs(f.pitch()) < .08F, "initial truck settles on native wheels");
    require(f.contacts.fork_load, "real steel block touches actual production forks");
    const double start_y = f.kit->body_center_of_mass_position(f.load).GetY();
    const double start_battery = f.truck->state().battery_j;
    const auto &config = f.truck->config();
    const float axle_span = config.wheel_positions[0].GetZ() - config.wheel_positions[2].GetZ();
    const float clearance_angle = std::asin((config.suspension_max_m - config.suspension_min_m) / axle_span);
    std::cout << "CLEARANCE axle_span_m=" << axle_span << " suspension_angle_rad=" << clearance_angle
              << " tip_guard_rad=" << 2 * clearance_angle << '\n';
    float max_pitch = f.pitch(), min_rear = f.truck->wheels()[2].normal_force_n;
    double max_load_y = start_y;
    bool unloaded = false, first_unload = false;
    WarehouseForklift::Command command; command.lift = 1;
    for (unsigned i = 0; i < 720; ++i) {
        f.tick(command);
        const auto state = f.truck->state(); const auto wheels = f.truck->wheels();
        max_pitch = std::max(max_pitch, f.pitch()); min_rear = std::min(min_rear, wheels[2].normal_force_n);
        max_load_y = std::max(max_load_y, double(f.kit->body_center_of_mass_position(f.load).GetY()));
        require(std::abs(state.lift_force_n) <= 18005 && std::abs(state.tilt_torque_nm) <= 17005,
                "unchanged finite lift force and neutral tilt brake torque caps");
        require(state.reservation_excess_j == 0, "no energy reservation excess");
        if (!wheels[2].contact && wheels[2].normal_force_n < 10 && f.pitch() > .04F) {
            unloaded = true;
            if (!first_unload) { f.observe("FIRST_REAR_UNLOADED", i + 181); first_unload = true; }
        }
        if (i % 90 == 0) f.observe(expect_tip ? "FAR_LIFT" : "NEAR_LIFT", i + 181);
    }
    const auto final = f.truck->state(); f.observe("FINAL", 900);
    std::cout << "RESULT center_z=" << center_z << " max_pitch=" << max_pitch << " min_rear_N=" << min_rear
              << " max_load_rise=" << max_load_y - start_y << " source_debit_J=" << start_battery - final.battery_j << '\n';
    require(final.hydraulic_positive_work_j > 5000 && final.battery_j < start_battery, "heavy lift spends finite native source work");
    if (expect_tip) {
        // A tip need not retain its cargo or complete a successful lift. Native
        // rear contact must disappear and chassis pitch exceed wheel travel.
        require(unloaded && max_pitch > 2 * clearance_angle,
                "far load loses rear contact and tips beyond available suspension travel");
        const auto load_end = f.kit->body_center_of_mass_position(f.load);
        require(load_end.GetZ() > center_z + .5F && load_end.GetY() < .25F && !f.contacts.load_ground.empty(),
                "independent load sheds forward and comes to rest on actual ground");
        require(f.truck->wheels()[2].contact && f.truck->wheels()[2].normal_force_n > 500,
                "truck recovers real rear wheel support after shedding overload");
    } else {
        require(max_load_y > start_y + .35, "finite actuator actually raises independent heavy load");
        require(max_pitch < .10F && f.truck->wheels()[2].normal_force_n > 500,
                "near load remains stable with actual rear support");
    }
}
} // namespace
int main() {
    std::cout << std::unitbuf;
    RegisterDefaultAllocator(); Factory::sInstance = new Factory; RegisterTypes();
    load_center_case(2.3F, false); load_center_case(3.3F, true);
    UnregisterTypes(); delete Factory::sInstance; Factory::sInstance = nullptr;
    std::cout << "PASS production forklift1700kg near stable/far physical forward tip through load center only\n";
}
