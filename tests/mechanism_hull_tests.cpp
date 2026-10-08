#include "sim/mechanism_kit.hpp"
#include "sim/gravity_wheel_geometry.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/RegisterTypes.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using namespace JPH;
using scraperx::sim::kit::BodyIndex;
using scraperx::sim::kit::Kit;
using scraperx::sim::kit::Material;
using scraperx::sim::kit::Part;
using scraperx::sim::kit::WeldIndex;

void require(const bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL mechanism hull: " << message << '\n';
        std::exit(1);
    }
}

struct BroadPhase final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(const ObjectLayer layer) const override {
        return BroadPhaseLayer(layer);
    }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "hull"; }
};
struct BroadPhaseFilter final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(const ObjectLayer a, const BroadPhaseLayer b) const override {
        return a != 0 || b.GetValue() != 0;
    }
};
struct LayerFilter final : ObjectLayerPairFilter {
    bool ShouldCollide(const ObjectLayer a, const ObjectLayer b) const override {
        return a != 0 || b != 0;
    }
};
struct Fixture final {
    BroadPhase broad_phase;
    BroadPhaseFilter broad_phase_filter;
    LayerFilter layer_filter;
    PhysicsSystem system;
    Fixture() { system.Init(32, 0, 64, 128, broad_phase, broad_phase_filter, layer_filter); }
};

Part tetrahedron() {
    Part part;
    part.shape = Part::Shape::ConvexHull;
    part.material = Material::Refractory;
    part.mass_kg = 12.0F;
    // Axis lengths 2, 3, 4 give volume 4 and COM (2.5, -.25, 4).
    // Translation and an interior point catch accidental COM-relative output
    // and drawing the input cloud instead of the actual cooked hull.
    part.points = {Vec3(2, -1, 3), Vec3(4, -1, 3), Vec3(2, 2, 3),
                   Vec3(2, -1, 7), Vec3(2.2F, -0.7F, 3.4F)};
    return part;
}

void verify_tetrahedron_mesh(const std::vector<Vec3> &mesh) {
    const std::array<Vec3, 4> corners{Vec3(2, -1, 3), Vec3(4, -1, 3),
                                      Vec3(2, 2, 3), Vec3(2, -1, 7)};
    require(mesh.size() == 12, "four native tetrahedron faces produce four triangles");
    std::array<bool, 4> seen{};
    double volume = 0;
    for (std::size_t i = 0; i < mesh.size(); i += 3) {
        const auto a = mesh[i], b = mesh[i + 1], c = mesh[i + 2];
        const auto normal = (b - a).Cross(c - a);
        const auto centroid = (a + b + c) / 3.0F;
        require(normal.Dot(centroid - Vec3(2.5F, -.25F, 4)) > 0,
                "native triangles have nondegenerate outward CCW winding");
        volume += double(a.Dot(b.Cross(c))) / 6.0;
        for (const auto vertex : {a, b, c}) {
            bool found = false;
            for (std::size_t j = 0; j < corners.size(); ++j) {
                if (vertex.IsClose(corners[j], 1.0e-8F)) {
                    found = true;
                    seen[j] = true;
                }
            }
            require(found, "mesh stays in authored Part frame and omits interior points");
        }
    }
    require(std::abs(volume - 4.0) < 1.0e-5, "closed outward mesh has the actual hull volume");
    for (const bool corner_seen : seen) require(corner_seen, "all cooked corners are rendered");
}

void asymmetric_geometry_and_inertia() {
    Fixture fixture;
    Kit kit(fixture.system, 0, 1);
    const auto body = kit.add_body(2000, {tetrahedron()}, RVec3(10, 5, -20),
                                   Quat::sIdentity(), 12, .7F);
    verify_tetrahedron_mesh(kit.body_part_mesh(body, 0));
    BodyLockRead lock(fixture.system.GetBodyLockInterface(), kit.body_id(body));
    require(lock.Succeeded(), "native hull body exists");
    const auto &native = lock.GetBody();
    require(native.GetShape()->GetSubType() == EShapeSubType::ConvexHull,
            "collision is a cooked convex hull rather than a proxy box");
    const auto &hull = static_cast<const ConvexHullShape &>(*native.GetShape());
    require(hull.GetConvexRadius() == 0, "authored facets own the exact contact surface");
    require(std::abs(hull.GetVolume() - 4) < 1.0e-5F, "native hull has authored volume");
    require(hull.GetCenterOfMass().IsClose(Vec3(2.5F, -.25F, 4), 1.0e-8F),
            "native hull retains its asymmetric authored COM");
    require(Vec3(native.GetCenterOfMassPosition() - RVec3(12.5, 4.75, -16)).Length() < 1.0e-5F,
            "body origin and native center of mass remain distinct");
    require(std::abs(hull.GetMassProperties().mMass - 12) < 1.0e-5F &&
                std::abs(1.0F / native.GetMotionProperties()->GetInverseMass() - 12) < 1.0e-5F,
            "part density and actual solver mass honor authored kilograms");
    // Closed-form uniform tetrahedron inertia about COM: diagonal
    // 3*m*(b*b+c*c)/80 and products m*a*b/80, independently of Jolt.
    const float expected[3][3]{{11.25F, .9F, 1.2F}, {.9F, 9.0F, 1.8F},
                              {1.2F, 1.8F, 5.85F}};
    const auto inertia = native.GetMotionProperties()->GetLocalSpaceInverseInertia().Inversed3x3();
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            require(std::abs(inertia(row, column) - expected[row][column]) < 2.0e-4F,
                    "actual solver uses integrated hull inertia including products of inertia");
        }
    }
}

void polygon_faces_are_completely_triangulated() {
    Fixture fixture;
    Kit kit(fixture.system, 0, 1);
    Part prism;
    prism.shape = Part::Shape::ConvexHull;
    prism.points = {Vec3(0, 0, -.5F), Vec3(2, 0, -.5F), Vec3(0, 3, -.5F),
                    Vec3(0, 0, .5F), Vec3(2, 0, .5F), Vec3(0, 3, .5F)};
    const auto body = kit.add_body(1000, {prism}, RVec3::sZero(), Quat::sIdentity(), 0, .7F);
    const auto &mesh = kit.body_part_mesh(body, 0);
    require(mesh.size() == 24, "two triangle and three quad prism faces produce eight triangles");
    double volume = 0;
    for (std::size_t i = 0; i < mesh.size(); i += 3) {
        const auto a = mesh[i], b = mesh[i + 1], c = mesh[i + 2];
        require((b - a).Cross(c - a).Dot((a + b + c) / 3.0F - Vec3(2.0F / 3.0F, 1, 0)) > 0,
                "polygon triangle fans retain outward winding");
        volume += double(a.Dot(b.Cross(c))) / 6.0;
    }
    require(std::abs(volume - 3.0) < 1.0e-5, "polygon fan closes the entire native prism volume");
}

void compound_frames_and_contact_queries() {
    Fixture fixture;
    Kit kit(fixture.system, 0, 1);
    Part hull = tetrahedron();
    hull.offset = Vec3(5, 1, -2);
    hull.rotation = Quat::sRotation(Vec3::sAxisZ(), .5F * JPH_PI);
    Part box;
    box.offset = Vec3(-8, 0, 0);
    box.mass_kg = 8;
    const RVec3 origin(20, 7, -12);
    const auto rotation = Quat::sRotation(Vec3::sAxisY(), .37F);
    const auto body = kit.add_body(2000, {hull, box}, origin, rotation, 20, .7F);
    verify_tetrahedron_mesh(kit.body_part_mesh(body, 0));
    require(kit.body_part_mesh(body, 1).empty() && kit.body_part_mesh(body, 2).empty() &&
                kit.body_part_mesh(BodyIndex{}, 0).empty(),
            "nonhull and invalid part/body lookups return no hull geometry");
    // The transformed tetrahedron COM is (5.25, 3.5, 2), weighted 12:8
    // with the box COM (-8, 0, 0); body rotation acts only after that sum.
    const auto expected_com = origin + RVec3(rotation * Vec3(-.05F, 2.1F, 1.2F));
    require(Vec3(kit.body_center_of_mass_position(body) - expected_com).Length() < 2.0e-5F,
            "native compound weights per-part masses and applies each frame once");
    const auto world_point = [&](const Vec3 point) {
        return origin + RVec3(rotation * (hull.offset + hull.rotation * point));
    };
    const auto direction = rotation * (hull.rotation * Vec3(0, 0, 6));
    RayCastResult hit;
    require(fixture.system.GetNarrowPhaseQuery().CastRay(
                RRayCast(world_point(Vec3(2.25F, -.5F, 2)), direction), hit) &&
                hit.mBodyID == kit.body_id(body) && std::abs(hit.mFraction - 1.0F / 6.0F) < 1.0e-5F,
            "real native query hits the transformed authored hull face");
    RayCastResult miss;
    require(!fixture.system.GetNarrowPhaseQuery().CastRay(
                RRayCast(world_point(Vec3(3.5F, 1.25F, 2)), direction), miss),
            "empty tetrahedron corner remains empty collision space");
}

void invalid_hulls_are_rejected_before_body_creation() {
    Fixture fixture;
    Kit kit(fixture.system, 0, 1);
    const auto rejected = [&](Part part, const float body_mass = 12.0F) {
        bool diagnosed = false;
        try {
            kit.add_body(2000, {Part{}, part}, RVec3::sZero(), Quat::sIdentity(), body_mass, .7F);
        } catch (const std::invalid_argument &error) {
            diagnosed = std::string(error.what()).find("part") != std::string::npos;
        }
        require(diagnosed, "invalid shape has a clear authoring diagnostic");
        require(kit.body_count() == 0 && fixture.system.GetNumBodies() == 0,
                "rejected hull leaves no fake or partial native body");
    };
    Part hull = tetrahedron();
    hull.points.clear(); rejected(hull);
    hull.points = {Vec3::sZero(), Vec3::sAxisX(), Vec3::sAxisY()}; rejected(hull);
    hull.points = {Vec3::sZero(), Vec3::sZero(), Vec3::sZero(), Vec3::sZero()}; rejected(hull);
    hull.points = {Vec3(0, 0, 0), Vec3(1, 0, 0), Vec3(2, 0, 0), Vec3(3, 0, 0)}; rejected(hull);
    hull.points = {Vec3(0, 0, 0), Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(1, 1, 0)}; rejected(hull);
    hull = tetrahedron(); hull.points[0] = Vec3(std::numeric_limits<float>::quiet_NaN(), 0, 0); rejected(hull);
    hull = tetrahedron(); hull.points[0] = Vec3(0, std::numeric_limits<float>::infinity(), 0); rejected(hull);
    hull = tetrahedron(); hull.mass_kg = -1; rejected(hull);
    hull = tetrahedron(); hull.mass_kg = std::numeric_limits<float>::quiet_NaN(); rejected(hull);
    // A mass that would make Jolt choose its unit-sphere inertia fallback is
    // invalid solid authoring, even when the hull geometry itself cooks.
    hull = tetrahedron(); rejected(hull, 1.0e-30F);
    hull = tetrahedron(); rejected(hull, std::numeric_limits<float>::quiet_NaN());
    hull = tetrahedron(); rejected(hull, -1);
    hull = tetrahedron(); hull.shape = static_cast<Part::Shape>(255); rejected(hull);
}

struct Contacts final : ContactListener {
    bool supported = false;
    void OnContactAdded(const Body &a, const Body &b, const ContactManifold &,
                        ContactSettings &) override {
        if ((a.GetUserData() == 1000 && b.GetUserData() == 2000) ||
            (a.GetUserData() == 2000 && b.GetUserData() == 1000)) supported = true;
    }
};

void falling_hull_earns_support_through_contact() {
    Fixture fixture;
    fixture.system.SetGravity(Vec3(0, -9.81F, 0));
    Contacts contacts;
    fixture.system.SetContactListener(&contacts);
    {
        Kit kit(fixture.system, 0, 1);
        Part floor;
        floor.half = Vec3(2, .15F, 2);
        kit.add_body(1000, {floor}, RVec3(0, -.15, 0), Quat::sIdentity(), 0, .7F);
        Part fragment;
        fragment.shape = Part::Shape::ConvexHull;
        fragment.points = {Vec3::sZero(), Vec3(.4F, 0, 0), Vec3(0, .3F, 0), Vec3(0, 0, .5F)};
        fragment.mass_kg = 2.4F;
        const auto body = kit.add_body(2000, {fragment}, RVec3(0, 2, 0), Quat::sIdentity(), 2.4F, .7F);
        TempAllocatorImpl temporary(8 * 1024 * 1024);
        JobSystemSingleThreaded jobs(1024);
        for (int tick = 0; tick < 360; ++tick) {
            require(fixture.system.Update(1.0F / 90.0F, 1, &temporary, &jobs) == EPhysicsUpdateError::None,
                    "falling hull native update");
        }
        require(contacts.supported && kit.body_center_of_mass_position(body).GetY() > .01 &&
                    kit.body_center_of_mass_position(body).GetY() < .5 &&
                    kit.body_velocity(body).Length() < .2F,
                "gravity-driven fragment impacts the real floor and settles on contact");
    }
    fixture.system.SetContactListener(nullptr);
}

struct WeldStepper final : PhysicsStepListener {
    PhysicsSystem &system;
    Kit &kit;
    TempAllocatorImpl temporary{8 * 1024 * 1024};
    JobSystemSingleThreaded jobs{1024};
    WeldIndex observed;
    BodyID impulse_body;
    Vec3 impulse = Vec3::sZero(), angular_impulse = Vec3::sZero();
    bool saw_queued = false, broke_inside_update = false;
    WeldStepper(PhysicsSystem &world, Kit &owner) : system(world), kit(owner) {
        system.SetGravity(Vec3::sZero());
        system.AddStepListener(this);
    }
    ~WeldStepper() override { system.RemoveStepListener(this); }
    void OnStep(const PhysicsStepListenerContext &context) override {
        kit.begin_weld_step(context.mDeltaTime);
        if (observed.valid()) {
            const auto state = kit.weld_state(observed);
            saw_queued = saw_queued || state.break_queued;
            broke_inside_update = broke_inside_update || state.broken;
        }
        if (context.mIsFirstStep && !impulse_body.IsInvalid()) {
            auto &bodies = system.GetBodyInterfaceNoLock();
            if (!impulse.IsNearZero()) bodies.AddImpulse(impulse_body, impulse);
            if (!angular_impulse.IsNearZero()) bodies.AddAngularImpulse(impulse_body, angular_impulse);
        }
    }
    void update(const float dt, const int substeps, const bool finish = true) {
        require(system.Update(dt, substeps, &temporary, &jobs) == EPhysicsUpdateError::None,
                "rated weld native update");
        if (finish) kit.finish_weld_steps();
    }
};

bool pair_collides(PhysicsSystem &system, Kit &kit, const BodyIndex first, const BodyIndex second) {
    BodyLockRead a(system.GetBodyLockInterface(), kit.body_id(first));
    BodyLockRead b(system.GetBodyLockInterface(), kit.body_id(second));
    require(a.Succeeded() && b.Succeeded(), "weld collision endpoints exist");
    return a.GetBody().GetCollisionGroup().CanCollide(b.GetBody().GetCollisionGroup());
}

struct WeldContacts final : ContactListener {
    unsigned pairs = 0;
    void OnContactAdded(const Body &a, const Body &b, const ContactManifold &,
                        ContactSettings &) override {
        if ((a.GetUserData() == 2000 && b.GetUserData() == 2001) ||
            (a.GetUserData() == 2001 && b.GetUserData() == 2000)) ++pairs;
    }
};

void rated_weld_load_contact_and_checkpoint() {
    Fixture fixture;
    WeldContacts contacts;
    fixture.system.SetContactListener(&contacts);
    {
        Kit kit(fixture.system, 0, 1);
        const auto first = kit.add_body(2000, {Part{}}, RVec3(-.5, 0, 0), Quat::sIdentity(), 2, .7F);
        const auto second = kit.add_body(2001, {Part{}}, RVec3(.5, 0, 0), Quat::sIdentity(), 3, .7F);
        const auto weld = kit.add_breakable_weld(first, second, 100, 1.0e6F);
        WeldStepper stepper(fixture.system, kit);
        stepper.observed = weld;
        require(!pair_collides(fixture.system, kit, first, second) &&
                    std::abs(kit.point_inverse_mass(first, kit.body_center_of_mass_position(first),
                                                    Vec3::sAxisX()) - .2F) < 1.0e-5F,
                "intact weld filters its pair and credits the actual five-kilogram cluster");
        require(kit.body_weld_material_key(first) == 2000 && kit.body_weld_material_key(second) == 2000,
                "complementary halves retain one body-local brick material seed");
        // Independent load expectation: accelerating the 5 kg cluster with
        // 50 N on its 3 kg half transmits 2/5 * 50 = 20 N through the weld.
        fixture.system.GetBodyInterface().AddForce(kit.body_id(second), Vec3(50, 0, 0));
        stepper.update(.02F, 2);
        const auto low = kit.weld_state(weld);
        require(low.enabled && !low.broken && low.sample_count == 2 &&
                    std::abs(low.force_n - 20) < .1F && low.overload_steps == 0 &&
                    kit.weld_break_serial() == 0 && contacts.pairs == 0,
                "actual below-rating load stays welded and counts both completed substeps");
        Kit::Checkpoint intact;
        kit.capture(intact);
        // One impulse only in the first of four substeps produces a brief
        // 2 Ns weld impulse / .01 s = 200 N. Later unloaded solves must not
        // erase this real transient failure cause before the host boundary.
        stepper.impulse_body = kit.body_id(second);
        stepper.impulse = Vec3(5, 0, 0);
        stepper.update(.04F, 4, false);
        const auto before_finish = kit.weld_state(weld);
        require(stepper.saw_queued && !stepper.broke_inside_update && before_finish.enabled &&
                    before_finish.break_queued && !before_finish.broken && contacts.pairs == 0 &&
                    !pair_collides(fixture.system, kit, first, second) && before_finish.sample_count == 5,
                "transient overload queues failure while the entire Update remains welded and filtered");
        const auto velocity_a = kit.body_velocity(first), velocity_b = kit.body_velocity(second);
        kit.finish_weld_steps();
        const auto broken = kit.weld_state(weld);
        const auto event = kit.weld_break_sample();
        require(broken.broken && !broken.enabled && broken.collision_enabled && !broken.break_queued &&
                    broken.sample_count == 6 && broken.overload_steps >= 1 &&
                    broken.peak_force_n > 100 && pair_collides(fixture.system, kit, first, second),
                "finish observes the final substep once and releases joint and collision filter together");
        require(kit.body_velocity(first).IsClose(velocity_a, 0) && kit.body_velocity(second).IsClose(velocity_b, 0),
                "brittle release preserves both actual linear velocities");
        require(event.valid && event.serial == 1 && event.weld == weld && event.first_entity == 2000 &&
                    event.second_entity == 2001 && event.force_n > 100 &&
                    Vec3(event.position - broken.break_position).Length() < 1.0e-6F,
                "native break event records one measured overload and its actual release position");
        require(std::abs(kit.point_inverse_mass(first, kit.body_center_of_mass_position(first),
                                               Vec3::sAxisX()) - .5F) < 1.0e-5F,
                "disabled weld stops crediting the other half to point mobility");
        kit.finish_weld_steps();
        require(kit.weld_state(weld).sample_count == 6 && kit.weld_break_serial() == 1,
                "repeated finish cannot resample or emit a second break");
        Kit::Checkpoint fractured;
        kit.capture(fractured);
        stepper.impulse = Vec3::sZero();
        stepper.update(.01F, 1);
        require(contacts.pairs > 0, "released halves establish actual native pair contact");
        // A marked-but-uncompleted solve is bookkeeping, not a load sample.
        kit.begin_weld_step(.003F);
        kit.restore(intact);
        kit.finish_weld_steps();
        const auto restored = kit.weld_state(weld);
        require(restored.enabled && !restored.broken && restored.sample_count == low.sample_count &&
                    restored.force_n == low.force_n && !pair_collides(fixture.system, kit, first, second) &&
                    !kit.weld_break_sample().valid && kit.weld_break_serial() == 1,
                "intact checkpoint restores topology/filter/load history without stale solve or event replay");
        stepper.update(.02F, 2);
        require(kit.weld_state(weld).enabled && kit.weld_state(weld).overload_steps == 0 &&
                    kit.weld_state(weld).sample_count == low.sample_count + 2,
                "restored warm-start impulses cannot produce an invented overload");
        stepper.impulse = Vec3(5, 0, 0);
        stepper.update(.04F, 4);
        require(kit.weld_break_serial() == 2 && kit.weld_break_sample().valid,
                "a real post-retry fracture has a new monotonic publication serial");
        kit.restore(fractured);
        kit.finish_weld_steps();
        require(kit.weld_state(weld).broken && !kit.weld_state(weld).enabled &&
                    kit.weld_state(weld).break_serial == 1 && pair_collides(fixture.system, kit, first, second) &&
                    kit.weld_break_serial() == 2 && !kit.weld_break_sample().valid,
                "broken checkpoint restores physical history and contact while clearing publication");
    }
    fixture.system.SetContactListener(nullptr);
}

void torque_overload_and_final_substep() {
    Fixture fixture;
    Kit kit(fixture.system, 0, 1);
    const auto first = kit.add_body(2000, {Part{}}, RVec3(-.5, 0, 0), Quat::sIdentity(), 2, .7F);
    const auto second = kit.add_body(2001, {Part{}}, RVec3(.5, 0, 0), Quat::sIdentity(), 3, .7F);
    const auto weld = kit.add_breakable_weld(first, second, 1.0e8F, 20);
    WeldStepper stepper(fixture.system, kit);
    stepper.impulse_body = kit.body_id(second);
    stepper.angular_impulse = Vec3(2, 0, 0);
    stepper.update(.01F, 1, false);
    const auto constraint = fixture.system.GetConstraints().front();
    const auto *fixed = static_cast<const FixedConstraint *>(constraint.GetPtr());
    const float actual_torque = fixed->GetTotalLambdaRotation().Length() / .01F;
    const auto angular_a = fixture.system.GetBodyInterface().GetAngularVelocity(kit.body_id(first));
    const auto angular_b = fixture.system.GetBodyInterface().GetAngularVelocity(kit.body_id(second));
    require(kit.weld_state(weld).sample_count == 0 && actual_torque > 20,
            "one-substep Update leaves its actual angular load for the final observation");
    kit.finish_weld_steps();
    require(kit.weld_state(weld).broken && kit.weld_state(weld).sample_count == 1 &&
                std::abs(kit.weld_state(weld).torque_nm - actual_torque) < .01F &&
                fixture.system.GetBodyInterface().GetAngularVelocity(kit.body_id(first)).IsClose(angular_a, 0) &&
                fixture.system.GetBodyInterface().GetAngularVelocity(kit.body_id(second)).IsClose(angular_b, 0),
            "actual torque alone breaks on the final substep without replacing angular momentum");
}

void invalid_welds_and_filter_ownership() {
    Fixture fixture;
    Kit kit(fixture.system, 0, 1);
    const auto first = kit.add_body(2000, {Part{}}, RVec3(-2, 0, 0), Quat::sIdentity(), 2, .7F);
    const auto second = kit.add_body(2001, {Part{}}, RVec3(2, 0, 0), Quat::sIdentity(), 3, .7F);
    const auto third = kit.add_body(2002, {Part{}}, RVec3(0, 4, 0), Quat::sIdentity(), 1, .7F);
    const auto rejected = [&](BodyIndex a, BodyIndex b, float force, float torque) {
        const auto count = fixture.system.GetConstraints().size();
        bool diagnosed = false;
        try { (void)kit.add_breakable_weld(a, b, force, torque); }
        catch (const std::invalid_argument &) { diagnosed = true; }
        require(diagnosed && fixture.system.GetConstraints().size() == count,
                "invalid weld authoring leaves no native constraint or filter owner");
    };
    for (const float invalid : {0.0F, -1.0F, std::numeric_limits<float>::infinity(),
                                std::numeric_limits<float>::quiet_NaN()}) {
        rejected(first, second, invalid, 100);
        rejected(first, second, 100, invalid);
    }
    rejected({}, second, 100, 100);
    rejected(BodyIndex{42}, second, 100, 100);
    rejected(first, first, 100, 100);
    kit.disable_collision(first, third);
    rejected(first, third, 100, 100);
    require(!pair_collides(fixture.system, kit, first, third) && kit.weld_count() == 0,
            "existing collision exclusions survive rejected rated-pair ownership");
    kit.add_breakable_weld(first, second, 100, 100);
    rejected(second, first, 100, 100);
    const auto count = fixture.system.GetConstraints().size();
    bool filter_rejected = false, fixed_rejected = false;
    try { kit.disable_collision(second, first); }
    catch (const std::invalid_argument &) { filter_rejected = true; }
    try { kit.add_fixed_joint(first, second); }
    catch (const std::invalid_argument &) { fixed_rejected = true; }
    require(filter_rejected && fixed_rejected && fixture.system.GetConstraints().size() == count &&
                !pair_collides(fixture.system, kit, first, second) &&
                kit.body_weld_material_key(third) == 2002 && kit.body_weld_material_key({}) == 0,
            "rated pair keeps exclusive filtering and unpaired bodies keep their own material identity");
}

void complementary_half_mass_and_inertia() {
    Fixture fixture;
    Kit kit(fixture.system, 0, 1);
    Part whole;
    whole.shape = Part::Shape::ConvexHull;
    whole.material = Material::Refractory;
    whole.points = {Vec3(-.09F, -.04F, -.05F), Vec3(.16F, -.04F, -.05F),
                    Vec3(-.09F, .10F, -.05F), Vec3(-.09F, -.04F, .07F)};
    whole.offset = Vec3(.2F, -.1F, .3F);
    whole.rotation = Quat::sRotation(Vec3::sAxisY(), .37F);
    // Independently integrated tetrahedron: volume abc/6 at chosen2450kg/m3.
    whole.mass_kg = 2450.0F * .25F * .14F * .12F / 6.0F;
    const auto halves = scraperx::sim::split_refractory_fragment(whole);
    for (std::size_t side = 0; side < halves.size(); ++side) {
        require(halves[side].offset.IsClose(whole.offset, 0) &&
                    halves[side].rotation.IsClose(whole.rotation),
                "native halves retain the whole brick's authored Part frame");
        for (const auto point : halves[side].points)
            require(side == 0 ? point.GetX() <= 0 : point.GetX() >= 0,
                    "complementary native hulls stay on opposite sides of the cut");
    }
    require(std::abs(halves[0].mass_kg + halves[1].mass_kg - whole.mass_kg) < 1.0e-4F,
            "complementary cooked halves conserve independently integrated material mass");
    const RVec3 origin(3, 4, -2);
    const auto rotation = Quat::sRotation(Vec3::sAxisX(), -.22F);
    const auto solid = kit.add_body(2000, {whole}, origin, rotation, whole.mass_kg, .7F);
    const auto first = kit.add_body(2001, {halves[0]}, origin, rotation, halves[0].mass_kg, .7F);
    const auto second = kit.add_body(2002, {halves[1]}, origin, rotation, halves[1].mass_kg, .7F);
    const auto centre = kit.body_center_of_mass_position(solid);
    const float total_mass = kit.body_mass(first) + kit.body_mass(second);
    const auto weighted = (kit.body_mass(first) * Vec3(kit.body_center_of_mass_position(first) - centre) +
                           kit.body_mass(second) * Vec3(kit.body_center_of_mass_position(second) - centre)) / total_mass;
    require(weighted.Length() < 2.0e-5F,
            "actual two-body solver COM matches the original rotated asymmetric solid");
    const auto inertia_about_whole_com = [&](const BodyIndex body) {
        BodyLockRead lock(fixture.system.GetBodyLockInterface(), kit.body_id(body));
        require(lock.Succeeded(), "native half inertia body exists");
        MassProperties mass;
        mass.mMass = kit.body_mass(body);
        mass.mInertia = lock.GetBody().GetInverseInertia().Inversed3x3();
        mass.Translate(Vec3(lock.GetBody().GetCenterOfMassPosition() - centre));
        return mass.mInertia;
    };
    const auto expected = inertia_about_whole_com(solid);
    const auto aggregate = inertia_about_whole_com(first) + inertia_about_whole_com(second);
    for (int row = 0; row < 3; ++row) for (int column = 0; column < 3; ++column)
        require(std::abs(expected(row, column) - aggregate(row, column)) < 2.0e-5F,
                "cooked complementary halves conserve actual world inertia and its products");
    kit.add_breakable_weld(first, second, 6000, 150);
    const auto point = origin + Vec3(.7F, .5F, -.3F);
    for (const auto direction : {Vec3::sAxisX(), Vec3::sAxisY(), Vec3::sAxisZ()})
        require(std::abs(kit.point_inverse_mass(solid, point, direction) -
                         kit.point_inverse_mass(first, point, direction)) < .003F,
                "intact native half cluster retains the original solid's off-center impulse mobility");
}
} // namespace

int main() {
    RegisterDefaultAllocator();
    Factory::sInstance = new Factory;
    RegisterTypes();
    asymmetric_geometry_and_inertia();
    polygon_faces_are_completely_triangulated();
    compound_frames_and_contact_queries();
    invalid_hulls_are_rejected_before_body_creation();
    falling_hull_earns_support_through_contact();
    rated_weld_load_contact_and_checkpoint();
    torque_overload_and_final_substep();
    invalid_welds_and_filter_ownership();
    complementary_half_mass_and_inertia();
    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;
    std::cout << "PASS mechanism hull geometry, mass/inertia, contacts and rated weld checkpoint/failure\n";
}
