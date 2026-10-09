#include "sim/deformable_plank.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

namespace {
using namespace JPH;
using scraperx::sim::DeformablePlank;
using scraperx::sim::kit::Kit;
using scraperx::sim::kit::Part;
using Axis = SixDOFConstraintSettings::EAxis;
constexpr float kDt = 1.0F / 90.0F;
constexpr double kRelativeTolerance = .02;
constexpr double kEI = 7819.26; // E*b*t^3/12, independently derived in SI units.
constexpr double kJoint = 39096.3;
const char *scenario = "initialization";
std::uint32_t chain_velocity_steps = 0;
std::uint32_t chain_position_steps = 0;
bool chain_warm_start = true;
int collision_steps = 4; // Existing production Slingshot world; input remains90Hz.
float projection_factor = .2F;

void require(bool condition, const char *message) {
    if (condition) return;
    std::cerr << "FAIL DEFORMABLE_PLANK scenario=" << scenario << " reason=" << message << '\n';
    std::exit(EXIT_FAILURE);
}

bool finite(Vec3 v) {
    return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) && std::isfinite(v.GetZ());
}

struct BroadPhase final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override { return BroadPhaseLayer(layer); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "plank"; }
};
struct BroadPhaseFilter final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override {
        return a != 0 || b.GetValue() != 0;
    }
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
    double update_seconds = 0;
    std::uint32_t update_ticks = 0;
    Fixture() {
        system.Init(64, 0, 128, 256, broad_phase, broad_phase_filter, layer_filter);
        system.SetGravity(Vec3::sZero());
    }
    void tick() {
        const auto started = std::chrono::steady_clock::now();
        require(system.Update(kDt, collision_steps, &allocator, &jobs) == EPhysicsUpdateError::None,
                "native90Hz update has no capacity or simulation errors");
        update_seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        ++update_ticks;
    }
};

struct Motion final {
    double linear = 0, angular = 0;
};

template <typename Bodies>
Motion motion(Fixture &f, const Bodies &ids) {
    Motion result;
    const auto &bodies = f.system.GetBodyInterface();
    for (const auto id : ids) {
        const auto position = bodies.GetPosition(id);
        const auto rotation = bodies.GetRotation(id);
        const auto linear = bodies.GetLinearVelocity(id);
        const auto angular = bodies.GetAngularVelocity(id);
        require(finite(Vec3(position)) && finite(linear) && finite(angular) &&
                    std::isfinite(rotation.GetW()) &&
                    finite(Vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ())),
                "actual body pose and velocity remain finite");
        result.linear = std::max(result.linear, double(linear.Length()));
        result.angular = std::max(result.angular, double(angular.Length()));
    }
    return result;
}

// No pose reset, velocity assignment or artificial static body is used to
// settle a load. Require a stable observable response and small native motion.
template <typename Bodies, typename Apply, typename Measure>
double settle(Fixture &f, const Bodies &ids, Apply apply, Measure measure, const char *event) {
    double previous = measure();
    double quiet_min = previous, quiet_max = previous;
    int quiet = 0;
    Motion speed;
    for (int i = 0; i < 2700; ++i) {
        apply();
        f.tick();
        const double value = measure();
        speed = motion(f, ids);
        if (i < 20 || i % 90 == 0) {
            std::cout << "PLANK_TRANSIENT," << scenario << ',' << event << ",tick=" << i
                      << ",response=" << value << ",linear=" << speed.linear
                      << ",angular=" << speed.angular << '\n';
        }
        if (!std::isfinite(value) || std::abs(value) >= .5) {
            for (const auto id : ids) {
                const auto p = f.system.GetBodyInterface().GetPosition(id);
                const auto q = f.system.GetBodyInterface().GetRotation(id);
                std::cerr << "PLANK_BODY," << id.GetIndexAndSequenceNumber() << ','
                          << p.GetX() << ',' << p.GetY() << ',' << p.GetZ() << ','
                          << q.GetX() << ',' << q.GetY() << ',' << q.GetZ() << ',' << q.GetW() << '\n';
            }
        }
        require(std::isfinite(value) && std::abs(value) < .5, "elastic response stays finite and bounded");
        // Meter-scale single-precision anchors with298MN/m axial stiffness
        // retain micrometer solver jitter. Rest is below2mm/s,2mrad/s,
        // and0.1mm response range for a full second; static deflection, seam and
        // passive-energy tolerances remain independent and unchanged.
        if (i >= 270 && speed.linear < 2.0e-3 && speed.angular < 2.0e-3) {
            if (quiet == 0) quiet_min = quiet_max = value;
            quiet_min = std::min(quiet_min, value);
            quiet_max = std::max(quiet_max, value);
            if (quiet_max - quiet_min <= 1.0e-4) ++quiet;
            else quiet = 0;
        } else quiet = 0;
        if (quiet >= 90) {
            std::cout << "PLANK_SETTLE," << scenario << ',' << event << ",ticks=" << i + 1
                      << ",response=" << value << ",linear_mps=" << speed.linear
                      << ",angular_radps=" << speed.angular << '\n';
            return value;
        }
        previous = value;
    }
    std::cerr << "PLANK_UNSETTLED," << scenario << ',' << event << ",response=" << previous
              << ",linear_mps=" << speed.linear << ",angular_radps=" << speed.angular << '\n';
    require(false, "physical spring did not settle within30 native seconds");
    return previous;
}

void response_matches(double measured, double expected, const char *message) {
    std::cout << "PLANK_RESPONSE," << scenario << ",measured=" << measured
              << ",expected=" << expected << ",relative_error=" << measured / expected - 1 << '\n';
    require(measured * expected > 0 &&
                std::abs(measured - expected) <= std::abs(expected) * kRelativeTolerance, message);
}

struct RegisteredJoint final {
    PhysicsSystem &system;
    Ref<SixDOFConstraint> joint;
    RegisteredJoint(PhysicsSystem &s, Ref<SixDOFConstraint> j) : system(s), joint(j) {
        require(joint.GetPtr() != nullptr, "production joint factory creates an actual constraint");
        system.AddConstraint(joint.GetPtr());
    }
    ~RegisteredJoint() { system.RemoveConstraint(joint.GetPtr()); }
};

void axis_response(Axis selected, double stiffness, double load, const char *name,
                   Quat basis = Quat::sIdentity(), bool fixed_endpoint = false) {
    scenario = name;
    Fixture f;
    Kit kit(f.system, 0, 1);
    Part part;
    part.half = Vec3(.1F, .019F, .095F);
    part.mass_kg = .722F;
    const auto fixed = kit.add_body(8000, {part}, RVec3::sZero(), basis, 0, .5F);
    const auto moving = kit.add_body(8001, {part}, RVec3::sZero(), basis, .722F, .5F);
    kit.disable_collision(fixed, moving);
    kit.set_damping(moving, 0, 0);
    {
        BodyLockWrite lock(f.system.GetBodyLockInterface(), kit.body_id(moving));
        lock.GetBody().GetMotionProperties()->SetMaxAngularVelocity(500.0F);
    }
    const std::array<BodyID, 2> pair{kit.body_id(fixed), kit.body_id(moving)};
    const std::array<BodyID, 1> dynamic{pair[1]};
    auto settings = DeformablePlank::joint_settings(RVec3::sZero(), basis, fixed_endpoint, 80, 8);
    const auto &spring = settings.mMotorSettings[selected].mSpringSettings;
    require(spring.mMode == ESpringMode::StiffnessAndDamping &&
                std::abs(double(spring.mStiffness) - stiffness) < stiffness * 1.0e-6,
            "real joint uses the independently derived physical stiffness rather than frequency");
    for (int i = 0; i < Axis::Num; ++i) settings.MakeFixedAxis(static_cast<Axis>(i));
    settings.MakeFreeAxis(selected);
    Ref<SixDOFConstraint> created;
    {
        BodyLockMultiWrite lock(f.system.GetBodyLockInterface(), pair.data(), 2);
        require(lock.GetBody(0) && lock.GetBody(1), "axis fixture bodies exist under write locks");
        created = DeformablePlank::create_joint(*lock.GetBody(0), *lock.GetBody(1), settings);
    }
    RegisteredJoint constraint(f.system, created);
    for (int i = 0; i < Axis::Num; ++i)
        require(created->GetMotorState(static_cast<Axis>(i)) ==
                    (i == int(selected) ? EMotorState::Position : EMotorState::Off),
                "only selected elastic axis has a Position motor");
    require(created->GetTargetPositionCS().Length() < 1.0e-7F &&
                std::abs(created->GetTargetOrientationCS().GetW()) > .999999F,
            "real joint has zero extension and identity rest orientation");
    const bool translation = selected == Axis::TranslationX;
    const int component = translation ? 0 : int(selected) - int(Axis::RotationX);
    const Vec3 local_axis = component == 0 ? Vec3::sAxisX() :
                            component == 1 ? Vec3::sAxisY() : Vec3::sAxisZ();
    const auto measure = [&]() {
        const auto &bodies = f.system.GetBodyInterface();
        if (translation)
            return double((basis.Conjugated() * Vec3(bodies.GetPosition(pair[1]))).GetX());
        return double((basis.Conjugated() * bodies.GetRotation(pair[1])).GetEulerAngles()[component]);
    };
    for (const double sign : {1.0, -1.0}) {
        const auto apply = [&]() {
            auto &bodies = f.system.GetBodyInterface();
            const Vec3 value = basis * (local_axis * float(sign * load));
            if (translation) bodies.AddForce(pair[1], value);
            else bodies.AddTorque(pair[1], value);
        };
        const double observed = settle(f, dynamic, apply, measure, sign > 0 ? "positive" : "negative");
        response_matches(observed, sign * load / stiffness,
                         "load sign and physical stiffness produce correct local response within2percent");
        const double unloaded = settle(f, dynamic, []() {}, measure, "unloaded");
        require(std::abs(unloaded) < .02 * load / stiffness,
                "stopping external load restores the physical elastic rest");
    }
}

void axes() {
    // Keep isolated loads below Jolt's pre-solve free velocity caps; material
    // stiffness and the2% response acceptance remain unchanged.
    axis_response(Axis::TranslationX, 297825000, 3000, "axial_X");
    axis_response(Axis::RotationX, 52570.5, 5.25705, "torsion_X_assumed_nu0.30");
    // Resolve a1e-4rad response above Jolt's angular-integration dead zone.
    axis_response(Axis::RotationY, 977407.5, 97.74075, "strong_bending_Y");
    axis_response(Axis::RotationZ, 39096.3, 3.90963, "weak_bending_Z");
    const Quat rotated = Quat::sRotation(Vec3::sAxisY(), .71F) *
                         Quat::sRotation(Vec3::sAxisX(), -.43F);
    axis_response(Axis::RotationZ, 39096.3, 3.90963, "rotated_common_frame_weak_Z", rotated);
    axis_response(Axis::RotationZ, 78192.6, 7.81926, "fixed_endpoint_half_cell_Z",
                  Quat::sIdentity(), true);
}

RVec3 endpoint(Fixture &f, BodyID body, float local_x) {
    const auto &bodies = f.system.GetBodyInterface();
    return bodies.GetPosition(body) + RVec3(bodies.GetRotation(body) * Vec3(local_x, 0, 0));
}

void verify_member(Fixture &f, Kit &kit, const DeformablePlank &plank) {
    require(kit.body_count() == 12 && f.system.GetNumBodies() == 12,
            "member consists of twelve actual bodies without proxy support bodies");
    require(plank.logical_member_for_body(BodyID{}) == 0, "invalid body is not credited as this member");
    for (std::size_t i = 0; i < 12; ++i) {
        const auto id = plank.body_ids()[i];
        require(id == kit.body_id(plank.segment_indices()[i]) &&
                    plank.logical_member_for_body(id) == 1962 &&
                    f.system.GetBodyInterface().GetUserData(id) == 2880 + i,
                "logical member maps each actual segment BodyID without replacing physical identity");
        for (std::size_t j = 0; j < i; ++j)
            require(id != plank.body_ids()[j], "each segment has a distinct native body");
        BodyLockRead lock(f.system.GetBodyLockInterface(), id);
        require(lock.Succeeded(), "actual segment body can be read");
        const auto &body = lock.GetBody();
        require(body.GetMotionType() == EMotionType::Dynamic &&
                    std::abs(1.0 / body.GetMotionProperties()->GetInverseMass() - .722) < 1.0e-5,
                "native segment mass is rho*b*t*h=.722kg");
        require(std::abs(body.GetPosition().GetX() - (.1 + .2 * double(i))) < 1.0e-5 &&
                    std::abs(body.GetPosition().GetY()) < 1.0e-6,
                "actual segment rests at its authored physical centre");
    }
}

double expected_discrete(bool cantilever) {
    // Independent virtual work. Internal hinge positions are j*.2;
    // fixed endpoint has half-cell spring2k, not a welded first segment.
    double compliance = cantilever ? 2.4 * 2.4 / (2 * kJoint) : 0;
    for (int j = 1; j < 12; ++j) {
        const double x = .2 * j;
        const double m = cantilever ? 2.4 - x : std::min(x, 2.4 - x) / 2;
        compliance += m * m / kJoint;
    }
    return compliance * (cantilever ? 100 : 1000);
}

void benchmark(bool cantilever) {
    scenario = cantilever ? "cantilever100N_tip2.4" : "pin_roller1000N_joint1.2";
    Fixture f;
    Kit kit(f.system, 0, 1);
    auto physics_settings = f.system.GetPhysicsSettings();
    physics_settings.mConstraintWarmStart = chain_warm_start;
    physics_settings.mBaumgarte = projection_factor;
    f.system.SetPhysicsSettings(physics_settings);
    DeformablePlank plank(f.system, kit, RVec3::sZero(),
                          cantilever ? DeformablePlank::Support::Cantilever :
                                       DeformablePlank::Support::PinRoller,
                          Quat::sIdentity(), chain_velocity_steps, chain_position_steps);
    verify_member(f, kit, plank);
    const auto &ids = plank.body_ids();
    const auto centre = [&]() { return (endpoint(f, ids[5], .1F) + endpoint(f, ids[6], -.1F)) * .5; };
    double max_energy_excess = 0;
    double supplied_work = 0;
    bool loading = true;
    const auto measure = [&]() {
        const auto &bodies = f.system.GetBodyInterface();
        const double displacement = -double((cantilever ? endpoint(f, ids[11], .1F) : centre()).GetY());
        if (loading) supplied_work = (cantilever ? 100 : 1000) * displacement;
        double stored_partial = 0;
        for (std::size_t i = 0; i < ids.size(); ++i) {
            stored_partial += kit.body_kinetic_energy(plank.segment_indices()[i]);
            const double angle = bodies.GetRotation(ids[i]).GetEulerAngles().GetZ();
            if (i == 0 && cantilever) stored_partial += 8 * kJoint * (1 - std::cos(angle * .5));
            if (i == 0) continue;
            const double preceding = bodies.GetRotation(ids[i - 1]).GetEulerAngles().GetZ();
            stored_partial += 4 * kJoint * (1 - std::cos((angle - preceding) * .5));
            const auto gap = Vec3(endpoint(f, ids[i], -.1F) - endpoint(f, ids[i - 1], .1F));
            const double axial = gap.Dot(bodies.GetRotation(ids[i - 1]) * Vec3::sAxisX());
            stored_partial += .5 * DeformablePlank::kAxialStiffnessNPerM * axial * axial;
        }
        // Planar quaternion bending + axial + actual kinetic energy is a
        // partial energy falsifier, not complete three-axis energy closure.
        // Conservative dead-load work is exactly F*material-point descent.
        max_energy_excess = std::max(max_energy_excess, stored_partial - supplied_work);
        return displacement;
    };
    const auto apply = [&]() {
        auto &bodies = f.system.GetBodyInterface();
        if (cantilever) {
            bodies.AddForce(ids[11], Vec3(0, -100, 0), endpoint(f, ids[11], .1F));
        } else {
            // Both forces act at the shared physical x1.2 joint, not COM1.1/1.3.
            bodies.AddForce(ids[5], Vec3(0, -500, 0), endpoint(f, ids[5], .1F));
            bodies.AddForce(ids[6], Vec3(0, -500, 0), endpoint(f, ids[6], -.1F));
        }
    };
    const double observed = settle(f, ids, apply, measure, "loaded");
    const double discrete = expected_discrete(cantilever);
    const double continuum = cantilever ? 100 * std::pow(2.4, 3) / (3 * kEI) :
                                         1000 * std::pow(2.4, 3) / (48 * kEI);
    const double literal = cantilever ? .059136030775290756 : .0373436872542926;
    require(std::abs(discrete - literal) < 1.0e-12, "independent discrete calculation matches settled benchmark");
    std::cout << "PLANK_BENCHMARK," << scenario << ",jolt_mm=" << observed * 1000
              << ",discrete_mm=" << discrete * 1000 << ",continuum_mm=" << continuum * 1000
              << ",jolt_vs_discrete=" << observed / discrete - 1
              << ",discrete_vs_continuum=" << discrete / continuum - 1 << '\n';
    response_matches(observed, discrete,
                     "actual loaded plank agrees with discrete compliance within2percent");
    std::cout << "PLANK_ENERGY," << scenario << ",max_partial_excess_j=" << max_energy_excess
              << ",load_work_j=" << supplied_work << '\n';
    require(max_energy_excess < std::max(.1, .02 * std::abs(supplied_work)),
            "passive chain does not manufacture material stored/kinetic energy");
    const auto left = endpoint(f, ids[0], -.1F);
    require(Vec3(left).Length() < .0002F, "left endpoint stays pinned under actual point load");
    if (!cantilever) {
        const auto right = endpoint(f, ids[11], .1F);
        require(std::abs(right.GetY()) < .0002 && std::abs(right.GetZ()) < .0002,
                "roller locks only transverse endpoint movement");
        require(right.GetX() < 2.4 - .00001,
                "roller releases axial translation needed by finite bending geometry");
    }
    for (std::size_t i = 0; i + 1 < ids.size(); ++i)
        require(Vec3(endpoint(f, ids[i], .1F) - endpoint(f, ids[i + 1], -.1F)).Length() < .0002F,
                "loaded internal transverse joint anchors remain connected");
    loading = false;
    const double unloaded = settle(f, ids, []() {}, measure, "unloaded");
    require(max_energy_excess < std::max(.1, .02 * std::abs(supplied_work)),
            "elastic recoil does not manufacture material stored/kinetic energy");
    require(std::abs(unloaded) < .02 * discrete, "removing applied force restores plank without pose reset");
    std::cout << "PASS DEFORMABLE_PLANK " << scenario
              << " gravity=0 iterations=" << plank.velocity_iterations() << '/' << plank.position_iterations()
              << " update_us_per_tick=" << f.update_seconds * 1.0e6 / f.update_ticks
              << " material_E=9e9 benchmark_only=1 energy_closure=unverified\n";
}
struct FractureSteps final : PhysicsStepListener {
    Fixture &fixture;
    DeformablePlank &plank;
    std::uint32_t callbacks = 0;
    bool broke_inside_update = false;
    FractureSteps(Fixture &f, DeformablePlank &p) : fixture(f), plank(p) {
        fixture.system.AddStepListener(this);
    }
    ~FractureSteps() { fixture.system.RemoveStepListener(this); }
    void OnStep(const PhysicsStepListenerContext &context) override {
        plank.begin_collision_step(context.mDeltaTime);
        ++callbacks;
        broke_inside_update |= plank.capture().broken_mask != 0;
    }
};

struct FractureContacts final : ContactListener {
    std::uint16_t adjacent_contacts = 0;
    void observe(const Body &a, const Body &b) {
        const auto low = std::min(a.GetUserData(), b.GetUserData());
        const auto high = std::max(a.GetUserData(), b.GetUserData());
        if (low >= 2880 && low < 2891 && high == low + 1)
            adjacent_contacts |= std::uint16_t(1u << (low - 2880));
    }
    void OnContactAdded(const Body &a, const Body &b, const ContactManifold &, ContactSettings &) override {
        observe(a, b);
    }
    void OnContactPersisted(const Body &a, const Body &b, const ContactManifold &, ContactSettings &) override {
        observe(a, b);
    }
};

struct NativePose final {
    RVec3 position;
    Quat rotation;
    Vec3 linear, angular;
};

std::array<NativePose, 12> native_poses(Fixture &f, const DeformablePlank &plank) {
    std::array<NativePose, 12> result;
    auto &bodies = f.system.GetBodyInterface();
    for (std::size_t i = 0; i < result.size(); ++i) {
        const auto id = plank.body_ids()[i];
        result[i] = {bodies.GetPosition(id), bodies.GetRotation(id),
                     bodies.GetLinearVelocity(id), bodies.GetAngularVelocity(id)};
    }
    return result;
}

void unchanged_native(Fixture &f, const DeformablePlank &plank,
                      const std::array<NativePose, 12> &expected, const char *why) {
    const auto actual = native_poses(f, plank);
    for (std::size_t i = 0; i < actual.size(); ++i) {
        const auto &a = actual[i], &e = expected[i];
        require(Vec3(a.position - e.position).LengthSq() < 1.0e-14F &&
                    (a.linear - e.linear).LengthSq() < 1.0e-14F &&
                    (a.angular - e.angular).LengthSq() < 1.0e-14F &&
                    std::abs(a.rotation.GetX() - e.rotation.GetX()) < 1.0e-7F &&
                    std::abs(a.rotation.GetY() - e.rotation.GetY()) < 1.0e-7F &&
                    std::abs(a.rotation.GetZ() - e.rotation.GetZ()) < 1.0e-7F &&
                    std::abs(a.rotation.GetW() - e.rotation.GetW()) < 1.0e-7F, why);
    }
}

void finish_without_launch(Fixture &f, DeformablePlank &plank) {
    const auto before = native_poses(f, plank);
    plank.finish_collision_steps();
    unchanged_native(f, plank, before, "outside-Update fracture changes no native pose or momentum");
}

bool collision_pair(Fixture &f, BodyID a, BodyID b) {
    const BodyID ids[]{a, b};
    BodyLockMultiRead lock(f.system.GetBodyLockInterface(), ids, 2);
    require(lock.GetBody(0) && lock.GetBody(1), "fracture collision endpoints exist");
    return lock.GetBody(0)->GetCollisionGroup().CanCollide(lock.GetBody(1)->GetCollisionGroup());
}

void fragment_topology(Fixture &f, const DeformablePlank &plank) {
    const auto state = plank.capture();
    std::size_t first = 0;
    const auto &ids = plank.body_ids();
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i && (state.broken_mask & (1u << (i - 1)))) first = i;
        std::size_t last = first;
        while (last + 1 < ids.size() && !(state.broken_mask & (1u << last))) ++last;
        require(plank.fragment_for_body(ids[i]) == first &&
                    plank.fragment_supported(ids[i]) == (first == 0 || last == 11),
                "actual pin/roller contiguous components retain only their real endpoint support");
        if (i + 1 < ids.size())
            require(collision_pair(f, ids[i], ids[i + 1]) == bool(state.broken_mask & (1u << i)),
                    "only actual broken neighboring bodies have collision re-enabled");
    }
    require(plank.fragment_for_body(BodyID{}) == UINT8_MAX && !plank.fragment_supported(BodyID{}),
            "unknown native body has no fabricated fragment support");
}

struct OnlyTheseBodies final : BodyFilter {
    BodyID first, second;
    OnlyTheseBodies(BodyID a, BodyID b) : first(a), second(b) {}
    bool ShouldCollide(const BodyID &id) const override { return id == first || id == second; }
};

void fracture() {
    scenario = "pin_roller_strength_and_checkpoint";
    Fixture f;
    require(collision_steps == 4, "fracture scenario observes all four production-style substeps");
    Kit kit(f.system, 0, 1);
    DeformablePlank plank(f.system, kit, RVec3::sZero(), DeformablePlank::Support::PinRoller);
    verify_member(f, kit, plank);
    const auto &ids = plank.body_ids();
    FractureSteps steps(f, plank);
    FractureContacts contacts;
    f.system.SetContactListener(&contacts);
    plank.enable_strength_failure();
    const auto centre_load = [&](float force) {
        auto &bodies = f.system.GetBodyInterface();
        bodies.AddForce(ids[5], Vec3(0, -.5F * force, 0), endpoint(f, ids[5], .1F));
        bodies.AddForce(ids[6], Vec3(0, -.5F * force, 0), endpoint(f, ids[6], -.1F));
    };
    for (int i = 0; i < 180; ++i) {
        centre_load(1000);
        f.tick();
        finish_without_launch(f, plank);
        require(plank.capture().broken_mask == 0 && plank.capture().fracture_serial == 0,
                "actual1000N centre benchmark stays below authored strength");
    }
    require(steps.callbacks == 180 * 4 && !steps.broke_inside_update &&
                plank.capture().peak_strength_ratio > 0 && plank.capture().peak_strength_ratio < 1 &&
                contacts.adjacent_contacts == 0,
            "all substeps are observed and intact adjoining segments remain filtered");
    for (int i = 0; i < 180; ++i) {
        f.tick();
        finish_without_launch(f, plank);
    }
    Kit::Checkpoint intact_bodies;
    kit.capture(intact_bodies);
    const auto intact_native = native_poses(f, plank);
    const auto intact = plank.capture();
    fragment_topology(f, plank);

    // A physical one-frame pulse, not a test-only forced break. Defer mutation
    // while the pulse disappears and the intact chain relaxes. A last-sample
    // implementation must not lose the earlier substep overload.
    centre_load(10000);
    f.tick();
    const auto overloaded = plank.capture();
    require(overloaded.broken_mask == 0 && overloaded.fracture_serial == 0 &&
                overloaded.peak_strength_ratio >= 1 && !steps.broke_inside_update,
            "short actual load queues a substep overload without changing topology inside Update");
    for (int i = 0; i < 360; ++i) f.tick(); // no force; finish is deliberately deferred
    const auto relaxed = motion(f, ids);
    require(relaxed.linear < .002 && relaxed.angular < .002 &&
                plank.capture().peak_strength_ratio >= overloaded.peak_strength_ratio &&
                plank.capture().broken_mask == 0,
            "earlier overload survives later unloaded relaxed substeps");
    finish_without_launch(f, plank);
    const auto broken = plank.capture();
    require(broken.broken_mask != 0 && std::isfinite(broken.discarded_strain_energy_j) &&
                broken.discarded_strain_energy_j >= 0,
            "queued interior fracture removes actual load rows and records dissipated strain");
    int count = 0;
    std::size_t selected = 11;
    for (std::size_t j = 0; j < 11; ++j) if (broken.broken_mask & (1u << j)) {
        ++count;
        if (std::abs(int(j) - 5) < std::abs(int(selected) - 5)) selected = j;
    }
    require(broken.fracture_serial == std::uint64_t(count), "each actual broken joint emits exactly one event");
    fragment_topology(f, plank);
    for (int i = 0; i < 5; ++i) { f.tick(); finish_without_launch(f, plank); }
    require((contacts.adjacent_contacts & broken.broken_mask) != 0,
            "re-enabled native fracture faces actually produce Jolt contact");
    bool separated = false;
    for (int i = 0; i < 60; ++i) {
        f.system.GetBodyInterface().AddForce(ids[selected], Vec3(0, -100, 0));
        f.system.GetBodyInterface().AddForce(ids[selected + 1], Vec3(0, 100, 0));
        f.tick();
        finish_without_launch(f, plank);
        motion(f, ids);
        if (Vec3(endpoint(f, ids[selected], .1F) - endpoint(f, ids[selected + 1], -.1F)).Length() > .1F) {
            separated = true;
            break;
        }
    }
    require(separated, "small real fragment forces produce separation at the failed joint");
    for (const auto id : {ids[selected], ids[selected + 1]}) {
        const auto &bodies = f.system.GetBodyInterface();
        const auto q = bodies.GetRotation(id);
        RayCastResult hit;
        OnlyTheseBodies filter(id, id);
        require(f.system.GetNarrowPhaseQuery().CastRay(
                    RRayCast(bodies.GetPosition(id) + RVec3(q * Vec3(0, 0, .4F)), q * Vec3(0, 0, -.8F)),
                    hit, {}, {}, filter) && hit.mBodyID == id,
                "query follows each actual separated fragment body collider");
    }
    const auto gap = (endpoint(f, ids[selected], .1F) + endpoint(f, ids[selected + 1], -.1F)) * .5;
    RayCastResult ghost;
    OnlyTheseBodies cut_filter(ids[selected], ids[selected + 1]);
    require(!f.system.GetNarrowPhaseQuery().CastRay(RRayCast(gap + RVec3(0, 0, .4), Vec3(0, 0, -.8F)),
                                                   ghost, {}, {}, cut_filter),
            "the separated cut has no surviving whole-member proxy collider");
    const auto saved_broken = plank.capture();
    require(saved_broken.broken_mask == broken.broken_mask && saved_broken.fracture_serial == broken.fracture_serial,
            "small separation forces do not create spurious fracture events");
    Kit::Checkpoint broken_bodies;
    kit.capture(broken_bodies);
    const auto broken_native = native_poses(f, plank);
    const auto restore_and_check = [&](const Kit::Checkpoint &body_state, const DeformablePlank::State &state,
                                       const std::array<NativePose, 12> &expected) {
        kit.restore(body_state);
        plank.restore(state);
        unchanged_native(f, plank, expected, "Kit then plank restore preserves actual poses and momentum");
        const auto restored = plank.capture();
        require(restored.broken_mask == state.broken_mask && restored.fracture_serial == state.fracture_serial &&
                    restored.peak_strength_ratio == state.peak_strength_ratio &&
                    restored.discarded_strain_energy_j == state.discarded_strain_energy_j,
                "restore retains full fracture history without manufacturing another event");
        fragment_topology(f, plank);
        finish_without_launch(f, plank);
        require(plank.capture().fracture_serial == state.fracture_serial &&
                    plank.capture().broken_mask == state.broken_mask,
                "restore clears no required topology and emits no duplicate fracture");
    };
    restore_and_check(intact_bodies, intact, intact_native);
    restore_and_check(broken_bodies, saved_broken, broken_native);
    f.system.SetContactListener(nullptr);
    std::cout << "PASS DEFORMABLE_PLANK fracture mask=" << saved_broken.broken_mask
              << " peak_ratio=" << saved_broken.peak_strength_ratio
              << " serial=" << saved_broken.fracture_serial
              << " discarded_strain_j=" << saved_broken.discarded_strain_energy_j
              << " substep_queue=1 finish_momentum_preserved=1 native_contact_query=1 checkpoint=1\n";
}
} // namespace

int main(int argc, char **argv) {
    const std::string mode = argc > 1 ? argv[1] : "both";
    if (argc > 2) {
        chain_velocity_steps = static_cast<std::uint32_t>(std::stoul(argv[2]));
        chain_position_steps = 32;
    }
    if (argc > 3) chain_warm_start = std::string(argv[3]) != "cold";
    if (argc > 4) projection_factor = std::stof(argv[4]);
    if (mode != "both" && mode != "axes" && mode != "elastic" && mode != "fracture") {
        std::cerr << "FAIL DEFORMABLE_PLANK unknown mode\n";
        return EXIT_FAILURE;
    }
    std::cout << std::fixed << std::setprecision(9);
    RegisterDefaultAllocator();
    Factory::sInstance = new Factory;
    RegisterTypes();
    if (mode == "fracture" || mode == "both") fracture();
    if (mode == "both" || mode == "axes") axes();
    if (mode == "both" || mode == "elastic") {
        benchmark(false);
        benchmark(true);
    }
    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;
    std::cout << "PASS DEFORMABLE_PLANK mode=" << mode << " player_integration=unverified\n";
    return EXIT_SUCCESS;
}
