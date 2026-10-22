// The climbing lab's exam on unseen structure: see matron_exam.hpp.

#include "matron_exam.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace scraperx::sim::exam {
namespace {

// splitmix64: the same numbers on every compiler and platform (the standard
// distributions are not), so a seed printed by CI repeats anywhere.
class Rng final {
public:
    explicit Rng(const std::uint64_t seed) noexcept : state_(seed) {}
    std::uint64_t next() noexcept {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31U);
    }
    float uniform(const float lo, const float hi) noexcept {
        const double unit = static_cast<double>(next() >> 11U) * 0x1.0p-53;
        return lo + static_cast<float>(unit) * (hi - lo);
    }
    int below(const int n) noexcept { return static_cast<int>(next() % static_cast<std::uint64_t>(n)); }
    bool chance(const float p) noexcept { return uniform(0.0F, 1.0F) < p; }

private:
    std::uint64_t state_;
};

// A box child: its convex radius kept under its thinnest half extent.
void add_box(JPH::StaticCompoundShapeSettings &compound, const JPH::Vec3 centre, const JPH::Vec3 half) {
    const float thinnest = std::min({half.GetX(), half.GetY(), half.GetZ()});
    compound.AddShape(centre, JPH::Quat::sIdentity(), new JPH::BoxShape(half, std::min(0.05F, 0.5F * thinnest)));
}

JPH::BodyID add_static(JPH::PhysicsSystem &physics, const JPH::StaticCompoundShapeSettings &compound,
                       const JPH::ObjectLayer layer, const float base_y, Structure &out) {
    const JPH::ShapeSettings::ShapeResult shape = compound.Create();
    if (shape.HasError()) {
        throw std::runtime_error(std::string("exam: a structure's shape failed: ") + shape.GetError().c_str());
    }
    JPH::BodyCreationSettings settings(shape.Get(), JPH::RVec3(0.0, base_y, 0.0), JPH::Quat::sIdentity(),
                                       JPH::EMotionType::Static, layer);
    settings.mFriction = 0.8F;
    const JPH::BodyID id = physics.GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::DontActivate);
    if (id.IsInvalid()) {
        throw std::runtime_error("exam: out of bodies");
    }
    out.bodies.push_back(id);
    return id;
}

void hash_bytes(std::uint64_t &h, const void *data, const std::size_t size) noexcept {
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t i = 0; i < size; ++i) {
        h = (h ^ bytes[i]) * 0x100000001B3ULL;   // FNV-1a
    }
}

} // namespace

const char *kind_name(const Kind kind) noexcept {
    switch (kind) {
    case Kind::Ladder:
        return "ladder";
    case Kind::Pole:
        return "pole";
    case Kind::Panel:
        return "panel";
    case Kind::LedgeStack:
        return "ledge stack";
    }
    return "?";
}

Structure build(JPH::PhysicsSystem &physics, const std::uint64_t seed, const JPH::ObjectLayer static_layer,
                const float base_y) {
    Rng rng(seed);
    Structure out;
    out.seed = seed;
    Params &p = out.params;
    p.kind = static_cast<Kind>(rng.below(4));

    // The floor: its top at y 0.
    {
        JPH::StaticCompoundShapeSettings floor;
        add_box(floor, JPH::Vec3(0.0F, -0.5F, 0.0F), JPH::Vec3(30.0F, 0.5F, 30.0F));
        out.floor = add_static(physics, floor, static_layer, base_y, out);
    }

    // The holds stand on the line z = 0, her side z < 0; the goal's face at
    // z = standoff, the goal running back from it.
    JPH::StaticCompoundShapeSettings holds;
    switch (p.kind) {
    case Kind::Ladder: {
        p.pitch_m = rng.uniform(kMinPitchM, kMaxPitchM);
        p.height_m = rng.uniform(3.0F, 12.0F);
        p.first_hold_m = rng.uniform(kMinFirstHoldM, 0.40F);
        p.section_m = rng.uniform(0.03F, 0.06F);
        p.standoff_m = rng.uniform(0.15F, 0.25F);
        const float width = rng.uniform(0.45F, 0.70F);
        const int rungs = 1 + static_cast<int>((p.height_m - 0.10F - p.first_hold_m) / p.pitch_m);
        p.missing = rng.chance(0.3F) && rungs > 4 ? 1 + rng.below(rungs - 3) : -1;
        for (const float side : {-1.0F, 1.0F}) {
            add_box(holds, JPH::Vec3(side * 0.5F * width, 0.5F * p.height_m, 0.0F),
                    JPH::Vec3(0.03F, 0.5F * p.height_m, 0.03F));
        }
        for (int k = 0; k < rungs; ++k) {
            if (k != p.missing) {
                add_box(holds, JPH::Vec3(0.0F, p.first_hold_m + float(k) * p.pitch_m, 0.0F),
                        JPH::Vec3(0.5F * width - 0.03F, 0.5F * p.section_m, 0.5F * p.section_m));
            }
        }
        break;
    }
    case Kind::Pole: {
        p.height_m = rng.uniform(3.0F, 10.0F);
        p.section_m = rng.uniform(0.06F, 0.14F);
        p.standoff_m = rng.uniform(0.30F, 0.50F);
        const float top = p.height_m + rng.uniform(0.0F, 1.0F);
        add_box(holds, JPH::Vec3(0.0F, 0.5F * top, 0.0F), JPH::Vec3(0.5F * p.section_m, 0.5F * top, 0.5F * p.section_m));
        break;
    }
    case Kind::Panel: {
        p.pitch_m = rng.uniform(0.30F, kMaxPitchM);
        p.height_m = rng.uniform(4.0F, 12.0F);
        p.first_hold_m = rng.uniform(1.60F, kMaxFirstHoldM);
        p.section_m = 0.05F;
        p.standoff_m = rng.uniform(0.10F, 0.25F);
        const float width = rng.uniform(1.5F, 2.5F);
        const float span = p.height_m - p.first_hold_m;
        for (float x = -0.5F * width; x <= 0.5F * width + 1.0e-3F; x += 0.5F) {
            add_box(holds, JPH::Vec3(x, p.first_hold_m + 0.5F * span, 0.0F), JPH::Vec3(0.025F, 0.5F * span, 0.025F));
        }
        const int bars = 1 + static_cast<int>((span - 0.05F) / p.pitch_m);
        p.missing = rng.chance(0.3F) && bars > 4 ? 1 + rng.below(bars - 3) : -1;
        for (int k = 0; k < bars; ++k) {
            if (k != p.missing) {
                add_box(holds, JPH::Vec3(0.0F, p.first_hold_m + float(k) * p.pitch_m, -0.05F),
                        JPH::Vec3(0.5F * width, 0.025F, 0.025F));
            }
        }
        break;
    }
    case Kind::LedgeStack: {
        const int steps = 2 + rng.below(4);
        p.pitch_m = rng.uniform(kMinRiseM, kMaxRiseM);
        p.height_m = float(steps) * p.pitch_m;
        p.standoff_m = 0.0F;
        const float depth = rng.uniform(0.8F, 1.4F);
        for (int k = 0; k + 1 < steps; ++k) {
            const float top = float(k + 1) * p.pitch_m;
            const float front = float(steps - 1 - k) * depth;
            add_box(holds, JPH::Vec3(0.0F, 0.5F * top, -0.5F * front), JPH::Vec3(1.5F, 0.5F * top, 0.5F * front));
        }
        break;
    }
    }
    out.holds = add_static(physics, holds, static_layer, base_y, out);

    // The goal: a platform on the floor, its rail along the front with a gap
    // over the holds.
    {
        p.opening_m = rng.uniform(kMinOpeningM, 1.30F);
        if (p.kind == Kind::Pole) {
            // A pole's head stands over the lip: at least 0.60 m clear between
            // it and each post of the rail's opening, room for her body past
            // the head (the same draw, so every other exam is unchanged).
            p.opening_m = std::max(p.opening_m, 2.0F * (0.60F + 0.03F + 0.5F * p.section_m));
        }
        const float h = p.height_m;
        const float s = p.standoff_m;
        JPH::StaticCompoundShapeSettings goal;
        add_box(goal, JPH::Vec3(0.0F, 0.5F * h, s + 1.5F), JPH::Vec3(2.5F, 0.5F * h, 1.5F));
        const float rail_half = 0.5F * (2.5F - 0.5F * p.opening_m);
        for (const float side : {-1.0F, 1.0F}) {
            const float mid = side * (0.5F * p.opening_m + rail_half);
            add_box(goal, JPH::Vec3(mid, h + 1.0F, s + 0.05F), JPH::Vec3(rail_half, 0.03F, 0.03F));
            add_box(goal, JPH::Vec3(side * 0.5F * p.opening_m, h + 0.5F, s + 0.05F), JPH::Vec3(0.03F, 0.5F, 0.03F));
        }
        out.goal = add_static(physics, goal, static_layer, base_y, out);
        out.goal_feet = JPH::AABox(JPH::Vec3(-2.15F, base_y + h - 0.06F, s + 0.45F),
                                   JPH::Vec3(2.15F, base_y + h + 0.06F, s + 2.65F));
    }

    // The start, out on her side, and maybe a wall across the straight way
    // to the foot (too high to climb: round its ends).
    const float start_x = rng.uniform(-4.0F, 4.0F);
    const float start_z = -rng.uniform(5.5F, 9.0F);
    out.start_at = JPH::RVec3(start_x, base_y, start_z);
    out.start_feet = JPH::AABox(JPH::Vec3(-6.0F, base_y - 0.06F, -10.0F), JPH::Vec3(6.0F, base_y + 0.06F, -0.6F));
    p.wall = rng.chance(0.3F);
    if (p.wall) {
        const float wall_z = -rng.uniform(2.0F, 3.0F);
        const float wall_x = start_x * (wall_z / start_z);
        JPH::StaticCompoundShapeSettings wall;
        add_box(wall, JPH::Vec3(wall_x, 1.5F, wall_z), JPH::Vec3(rng.uniform(2.0F, 3.0F), 1.5F, 0.1F));
        (void)add_static(physics, wall, static_layer, base_y, out);
    }

    // The fingerprint, of what was built (not of what was meant).
    std::uint64_t h = 0xCBF29CE484222325ULL;
    const JPH::BodyLockInterfaceNoLock &locks = physics.GetBodyLockInterfaceNoLock();
    for (const JPH::BodyID id : out.bodies) {
        const JPH::BodyLockRead lock(locks, id);
        if (!lock.Succeeded()) {
            continue;
        }
        const JPH::AABox bounds = lock.GetBody().GetWorldSpaceBounds();
        const float corners[6] = {bounds.mMin.GetX(), bounds.mMin.GetY(), bounds.mMin.GetZ(),
                                  bounds.mMax.GetX(), bounds.mMax.GetY(), bounds.mMax.GetZ()};
        hash_bytes(h, corners, sizeof corners);
        // (A compound of one child is made a single translated shape.)
        const JPH::Shape *shape = lock.GetBody().GetShape();
        const JPH::uint children = shape->GetType() == JPH::EShapeType::Compound
                                       ? static_cast<const JPH::CompoundShape *>(shape)->GetNumSubShapes()
                                       : 1U;
        hash_bytes(h, &children, sizeof children);
    }
    out.fingerprint = h;
    return out;
}

Judge::Judge(const JPH::BodyID support, const JPH::AABox &feet, const float force_cap_n, const float stand_centre_m)
    : support_(support), feet_(feet), cap_n_(force_cap_n), stand_centre_m_(stand_centre_m) {
    verdict_.reason = "not yet";
}

void Judge::fail(const char *why) {
    if (!failed_ && !verdict_.arrived) {
        failed_ = true;
        verdict_.reason = why;
    }
}

void Judge::tick(const JPH::PhysicsSystem &physics, const JPH::BodyID body, const float commanded_force_n,
                 const bool gripping, const float dt) {
    if (done()) {
        return;
    }
    if (std::abs(dt - kTickSeconds) > 1.0e-6F) {
        fail("not the native tick");
        return;
    }
    const JPH::BodyInterface &bodies = physics.GetBodyInterfaceNoLock();
    const JPH::RVec3 at = bodies.GetPosition(body);
    const JPH::Vec3 velocity = bodies.GetLinearVelocity(body);
    if (started_) {
        verdict_.worst_step_m = std::max(verdict_.worst_step_m, float(JPH::Vec3(at - last_).Length()));
        // A sudden stop out of a fall: a landing, as hard as the fall was fast.
        if (last_vy_ < -1.0F && velocity.GetY() - last_vy_ > 0.5F * -last_vy_) {
            verdict_.worst_landing_mps = std::max(verdict_.worst_landing_mps, -last_vy_);
        }
    }
    started_ = true;
    last_ = at;
    last_vy_ = velocity.GetY();
    verdict_.peak_commanded_n = std::max(verdict_.peak_commanded_n, commanded_force_n);
    if (verdict_.worst_step_m > kMaxStepM) {
        fail("snapped: a move over 0.20 m in one tick");
    }
    if (commanded_force_n > cap_n_ + 0.1F) {
        fail("over the force cap");
    }
    if (bodies.GetGravityFactor(body) != 1.0F) {
        fail("gravity changed on her body");
    }
    if (verdict_.worst_landing_mps >= kLethalLandingMps) {
        verdict_.alive = false;
        fail("a lethal landing");
    }
    if (failed_) {
        return;
    }
    // Supported: her feet in the box, holding nothing, still, and her own
    // shape set 3 cm lower meeting the support itself with its contact
    // pushing her up (a real contact, not a point in a box).
    const JPH::RVec3 feet = at - JPH::Vec3(0.0F, stand_centre_m_, 0.0F);
    bool on_it = feet_.Contains(JPH::Vec3(feet)) && !gripping && velocity.Length() < kStillMps &&
                 std::abs(velocity.GetY()) < 0.1F;
    if (on_it) {
        on_it = false;
        const JPH::BodyLockRead lock(physics.GetBodyLockInterfaceNoLock(), body);
        if (lock.Succeeded()) {
            const JPH::Body &her = lock.GetBody();
            JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> hits;
            const JPH::IgnoreSingleBodyFilter not_her(body);
            physics.GetNarrowPhaseQuery().CollideShape(
                her.GetShape(), JPH::Vec3::sReplicate(1.0F),
                her.GetCenterOfMassTransform().PostTranslated(JPH::Vec3(0.0F, -0.03F, 0.0F)), JPH::CollideShapeSettings(),
                her.GetCenterOfMassPosition(), hits, {}, {}, not_her);
            for (const JPH::CollideShapeResult &hit : hits.mHits) {
                // The way to move the support off her: down, under her.
                if (hit.mBodyID2 == support_ && hit.mPenetrationAxis.NormalizedOr(JPH::Vec3::sZero()).GetY() <= -0.7F) {
                    on_it = true;
                    break;
                }
            }
        }
    }
    verdict_.supported_s = on_it ? verdict_.supported_s + dt : 0.0F;
    if (verdict_.supported_s >= kSupportedSeconds) {
        verdict_.arrived = true;
        verdict_.clean = verdict_.physical_failures == 0;
        verdict_.reason = verdict_.clean ? "arrived" : "arrived after a physical failure (a recovery)";
    }
}

} // namespace scraperx::sim::exam
