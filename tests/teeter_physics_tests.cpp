#include <Jolt/Jolt.h>

#include "sim/teeter_rise.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>

// This is a binding test of the production assembly against the compiler's
// one-coordinate beam model. Its rigid 85 kg compound fixture is NOT the upright,
// freely contacting, actively controlled gameplay player. teeter_route_tests
// separately exercises that player, landings, departures and traversal.
namespace {
using namespace JPH;
using namespace scraperx::sim;
constexpr double kGravity = 9.81;
constexpr double kPivotY = 65.8;
constexpr double kBeamInertia = 1928.9654666666666;
constexpr double kLoadedInertia = kBeamInertia + 85.0 * 4.5 * 4.5;

struct BP final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override {
        return BroadPhaseLayer(layer);
    }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "probe"; }
};
struct BV final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override {
        return a != 0 || b.GetValue() != 0;
    }
};
struct LP final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override {
        return a != 0 || b != 0;
    }
};
double angle(QuatArg q) { return 2.0 * std::atan2(q.GetZ(), q.GetW()); }

// Independent closed-form gravitational potential, relative to pivot height.
double potential(double a, bool loaded) {
    const double mass_x = 160.0 * 1.6 - 369.0 * 1.4 + (loaded ? 85.0 * 4.5 : 0.0);
    return kGravity * (mass_x * std::sin(a) - 369.0 * 0.6 * std::cos(a));
}

struct StopContacts final : ContactListener {
    std::uint64_t target = 1802;
    bool hit = false;
    double impact_angle = 0, impact_speed = 0;
    void OnContactAdded(const Body &a, const Body &b, const ContactManifold &,
                        ContactSettings &) override {
        observe(a, b);
    }
    void OnContactPersisted(const Body &a, const Body &b, const ContactManifold &,
                            ContactSettings &) override {
        observe(a, b);
    }
    void observe(const Body &a, const Body &b) {
        const Body *beam = a.GetUserData() == 2800 ? &a :
                           b.GetUserData() == 2800 ? &b : nullptr;
        if (hit || beam == nullptr ||
            (a.GetUserData() != target && b.GetUserData() != target)) return;
        hit = true;
        impact_angle = angle(beam->GetRotation());
        impact_speed = std::abs(beam->GetAngularVelocity().GetZ());
    }
};

struct Result {
    bool loaded = false, contact = false, mass_ok = false, damping_ok = false;
    int hz = 0;
    double friction = 0, first_motion = 0, peak_speed = 0, impact_angle = 0;
    double impact_speed = 0, predicted_speed = 0, rest_angle = 0, rest_speed = 0;
    double max_residual = 0, released = 0, bearing_work = 0, elapsed = 0;
    double minimum_residual = 0, maximum_residual = 0;
    double sample_speed = 0, sample_time = 0;
    double rest_stop_overlap = 0, penetration_slop = 0;
    double mass = 0, com_x = 0, com_y = 0, pivot_inertia = 0;
};

Result run(int hz, bool loaded, double friction) {
    BP bp;
    BV bv;
    LP lp;
    PhysicsSystem system;
    system.Init(64, 0, 128, 256, bp, bv, lp);
    system.SetGravity(Vec3(0, -float(kGravity), 0));
    TempAllocatorImpl temporary(8 * 1024 * 1024);
    JobSystemSingleThreaded jobs(1024);
    StopContacts contacts;
    contacts.target = loaded ? 1802 : 1801;
    system.SetContactListener(&contacts);
    Result r;
    r.loaded = loaded;
    r.hz = hz;
    r.friction = friction;
    r.penetration_slop = system.GetPhysicsSettings().mPenetrationSlop;
    {
        kit::Kit kit(system, 0, 1);
        TeeterRise assembly(kit);
        const auto beam = kit.body_for_entity(2800);
        auto &bodies = system.GetBodyInterface();
        {
            BodyLockRead lock(system.GetBodyLockInterface(), kit.body_id(beam));
            const auto &body = lock.GetBody();
            const auto *motion = body.GetMotionProperties();
            const auto com = body.GetShape()->GetCenterOfMass();
            r.mass = 1.0 / motion->GetInverseMass();
            r.com_x = com.GetX();
            r.com_y = com.GetY();
            const auto axis = motion->GetInertiaRotation().Conjugated() * Vec3::sAxisZ();
            const auto inverse = motion->GetInverseInertiaDiagonal();
            double inertia_com = 0;
            for (int i = 0; i < 3; ++i) inertia_com += axis[i] * axis[i] / inverse[i];
            r.pivot_inertia = inertia_com + r.mass * (r.com_x*r.com_x + r.com_y*r.com_y);
            r.mass_ok = std::abs(r.mass - 529.0) < 0.01 &&
                std::abs(r.com_x - (-260.6 / 529.0)) < 0.00001 &&
                std::abs(r.com_y - (-221.4 / 529.0)) < 0.00001 &&
                std::abs(r.pivot_inertia - kBeamInertia) < 0.02;
            r.damping_ok = motion->GetLinearDamping() == 0 && motion->GetAngularDamping() == 0;
        }
        // The engine has one Coulomb-bearing torque. Set it directly on the
        // production hinge for equal static/kinetic band endpoints and for the
        // zero-friction numerical-energy calibration; no production test hook.
        unsigned hinges = 0;
        for (const auto &constraint : system.GetConstraints()) {
            if (constraint->GetSubType() != EConstraintSubType::Hinge) continue;
            auto *hinge = static_cast<HingeConstraint *>(constraint.GetPtr());
            if (hinge->GetMaxFrictionTorque() != 90.0F)
                throw std::runtime_error("production bearing differs from compiled nominal torque");
            hinge->SetMaxFrictionTorque(float(friction));
            ++hinges;
        }
        if (hinges != 1) throw std::runtime_error("missing/ambiguous production hinge");
        const double initial_angle = loaded ? 0.1 : -0.4;
        if (!loaded) {
            // Initial condition of the reset experiment, before any stepping.
            bodies.SetPositionAndRotation(kit.body_id(beam), RVec3(28.5, kPivotY, -139),
                Quat::sRotation(Vec3::sAxisZ(), float(initial_angle)), EActivation::Activate);
        }
        if (loaded) {
            // Preserve the exact production shape as a child. A tiny mass
            // proxy inside the deck supplies the compiler's rigid point load
            // without adding a spurious stiff joint/independent coordinate.
            const auto original = bodies.GetShape(kit.body_id(beam));
            BoxShapeSettings proxy(Vec3::sReplicate(0.02F), 0.01F);
            proxy.mDensity = 85.0F / (0.04F * 0.04F * 0.04F);
            StaticCompoundShapeSettings compound;
            compound.AddShape(Vec3::sZero(), Quat::sIdentity(), original);
            compound.AddShape(Vec3(4.5F, 0, 0), Quat::sIdentity(), proxy.Create().Get());
            const auto loaded_shape = compound.Create().Get();
            const auto delta_com = loaded_shape->GetCenterOfMass() - original->GetCenterOfMass();
            bodies.SetShape(kit.body_id(beam), loaded_shape, true, EActivation::Activate);
            for (const auto &constraint : system.GetConstraints())
                constraint->NotifyShapeChanged(kit.body_id(beam), delta_com);
        }
        const auto energy = [&]() {
            BodyLockRead lock(system.GetBodyLockInterface(), kit.body_id(beam));
            const auto &body = lock.GetBody();
            const auto *motion = body.GetMotionProperties();
            const double mass = 1.0 / motion->GetInverseMass();
            const auto w = (body.GetRotation() * motion->GetInertiaRotation()).Conjugated() *
                           body.GetAngularVelocity();
            const auto inverse = motion->GetInverseInertiaDiagonal();
            double rotation_energy = 0;
            for (int i = 0; i < 3; ++i) rotation_energy += w[i] * w[i] / inverse[i];
            return mass * kGravity * (body.GetCenterOfMassPosition().GetY() - kPivotY) +
                   0.5 * (mass * body.GetLinearVelocity().LengthSq() + rotation_energy);
        };
        const double initial_energy = energy();
        double previous_angle = angle(kit.body_rotation(beam));
        double previous_speed = 0;
        const double sample_angle = loaded ? -0.40 : 0.05;
        const float dt = 1.0F / hz;
        for (int tick = 0; tick < 12 * hz; ++tick) {
            const bool before_contact = !contacts.hit;
            kit.pre_step(dt);
            system.Update(dt, 1, &temporary, &jobs);
            kit.post_step(dt);
            const double a = angle(kit.body_rotation(beam));
            const double omega = bodies.GetAngularVelocity(kit.body_id(beam)).GetZ();
            if (tick == 0) r.first_motion = omega;
            if (r.sample_speed == 0 && (loaded ? a <= sample_angle : a >= sample_angle)) {
                const double fraction = (sample_angle - previous_angle) / (a - previous_angle);
                r.sample_speed = previous_speed + fraction * (std::abs(omega) - previous_speed);
                r.sample_time = (double(tick) + fraction) / hz;
            }
            if (!contacts.hit) {
                r.bearing_work += friction * std::abs(a - previous_angle);
                const double residual = energy() - initial_energy + r.bearing_work;
                r.minimum_residual = std::min(r.minimum_residual, residual);
                r.maximum_residual = std::max(r.maximum_residual, residual);
                r.max_residual = std::max(r.max_residual, std::abs(residual));
                r.peak_speed = std::max(r.peak_speed, std::abs(omega));
            } else if (before_contact) {
                r.elapsed = double(tick) / hz;
                r.impact_angle = contacts.impact_angle;
                r.impact_speed = contacts.impact_speed;
                r.peak_speed = std::max(r.peak_speed, r.impact_speed);
                r.released = potential(initial_angle, loaded) - potential(r.impact_angle, loaded);
                const double work = friction * std::abs(r.impact_angle - initial_angle);
                // Tiny rigid proxy adds 0.022667 kg m² about its own centre.
                const double inertia = loaded ? kLoadedInertia + 85.0 * 0.04 * 0.04 / 6 : kBeamInertia;
                r.predicted_speed = std::sqrt(std::max(0.0, 2.0 * (r.released - work) / inertia));
            }
            previous_angle = a;
            previous_speed = std::abs(omega);
        }
        r.contact = contacts.hit;
        r.rest_angle = angle(kit.body_rotation(beam));
        r.rest_speed = bodies.GetAngularVelocity(kit.body_id(beam)).Length();
        // Distance of the stop's load-bearing top corner above the deck's
        // underside plane, in the beam's frame. Jolt permits penetration up
        // to its configured contact slop; compare metres rather than imposing
        // an angle smaller than that physical solver tolerance.
        const double stop_x = loaded ? 3.93 : -1.64;
        const double stop_y = loaded ? -2.14 : -0.29;
        r.rest_stop_overlap = -std::sin(r.rest_angle) * stop_x +
                             std::cos(r.rest_angle) * stop_y + 0.14;
    }
    system.SetContactListener(nullptr);
    std::cout << std::setprecision(10) << "TEETER_BINDING hz=" << hz
              << " loaded_fixture=" << loaded << " friction=" << friction
              << " mass=" << r.mass << " com=" << r.com_x << "," << r.com_y
              << " inertia=" << r.pivot_inertia << " damping_zero=" << r.damping_ok
              << " peak=" << r.peak_speed << " contact_angle=" << r.impact_angle
              << " sample_speed=" << r.sample_speed << " sample_time=" << r.sample_time
              << " impact=" << r.impact_speed << " predicted=" << r.predicted_speed
              << " rest=" << r.rest_angle << " rest_speed=" << r.rest_speed
              << " stop_overlap_m=" << r.rest_stop_overlap
              << " released_J=" << r.released << " bearing_J=" << r.bearing_work
              << " residual_J=" << r.max_residual
              << " signed_residual_J=" << r.minimum_residual << "," << r.maximum_residual
              << " contact_time=" << r.elapsed << '\n';
    return r;
}

bool valid(const Result &r, const Result &calibration) {
    // CMC numerical floor calibration: all safely isolated dissipation disabled
    // before the first stop contact, at the same timestep and initial condition.
    const double tolerance = std::max(0.02 * r.released, 3 * calibration.max_residual);
    // 0.1 mm allows float-coordinate representation of the declared corners.
    const bool rest = std::abs(r.rest_stop_overlap) <= r.penetration_slop + 0.0001;
    const bool direction = r.loaded ? r.first_motion < 0 : r.first_motion > 0;
    const bool pass = r.mass_ok && r.damping_ok && r.contact && rest && direction &&
        r.rest_speed < 0.01 && r.max_residual <= tolerance &&
        std::abs(r.impact_speed - r.predicted_speed) < 0.009 &&
        (!r.loaded || r.peak_speed < 0.5);
    std::cout << "TEETER_LEDGER hz=" << r.hz << " loaded_fixture=" << r.loaded
              << " friction=" << r.friction << " floor_J=" << calibration.max_residual
              << " tolerance_J=" << tolerance << " pass=" << pass << '\n';
    return pass;
}
} // namespace

int main() {
    RegisterDefaultAllocator();
    Factory::sInstance = new Factory();
    RegisterTypes();
    bool pass = true;
    try {
        for (const bool loaded : {false, true}) {
            const auto floor90 = run(90, loaded, 0);
            const auto floor360 = run(360, loaded, 0);
            for (const double friction : {60.0, 90.0, 120.0}) {
                const auto coarse = run(90, loaded, friction);
                const auto fine = run(360, loaded, friction);
                pass = valid(coarse, floor90) && valid(fine, floor360) && pass;
                // Worst predicted loaded arrival leaves ~0.03 rad/s to the
                // 0.5 rad/s requirement: 0.009 is less than one-third; this
                // refinement tolerance is less than one-tenth of that margin.
                const bool converged =
                    std::abs(coarse.sample_speed - fine.sample_speed) < (loaded ? 0.0025 : 0.009) &&
                    std::abs(coarse.rest_stop_overlap - fine.rest_stop_overlap) <= coarse.penetration_slop &&
                    std::abs(coarse.sample_time - fine.sample_time) < 0.025 &&
                    floor360.max_residual < 0.4 * floor90.max_residual;
                pass = converged && pass;
                std::cout << "TEETER_REFINEMENT loaded_fixture=" << loaded
                          << " friction=" << friction << " pass=" << converged << '\n';
            }
        }
    } catch (const std::exception &error) {
        std::cerr << "TEETER_BINDING exception=" << error.what() << '\n';
        pass = false;
    }
    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;
    if (!pass) {
        std::cerr << "FAIL teeter production assembly binding/energy/refinement\n";
        return 1;
    }
    std::cout << "PASS teeter production assembly binding, declared bearing loss and 90/360 Hz refinement\n";
}
