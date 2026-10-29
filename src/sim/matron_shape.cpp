// Her collision body follows her stance (issue #3, M2 "the body you can hit
// is the body you see"). Her drawn body is a 1.80 m rig that stoops forward
// of her middle, hunts low on all fours, hangs from her hands on a climb and
// hugs her pipe; her old body was one upright 2.10 m capsule about her middle
// in every pose, so her head and shoulders went through rails 0.4 m ahead of
// it and its empty top, half a metre over her head, struck the player.
//
// Now each stance family has its own capsule, set in her body frame from her
// drawn body as the probe measured it (tools/matron_probe/shape_fit.py):
//  - which family is read from her simulated state alone (the state machine,
//    her speed and stage, the way that has her), the same facts the view
//    poses her from, never from the view;
//  - each capsule turns with her yaw (kStanceYawBins shapes made once, a
//    switch between them is a pointer swap and a broadphase update);
//  - every capsule's centre of mass is put back at her body's origin, her
//    middle, so everything that reads her middle (her floor rays, her held
//    stock's hands, her journey's support probe) reads what it did, and her
//    standing families keep their lowest point at her soles (kStandCentre
//    under her middle);
//  - a switch is made only where the new capsule fits (shape_fits, a collide
//    query at her middle with the steel and every other body but hers and
//    the player's); otherwise she keeps the shape she has and her own
//    movement takes her to where it fits. Nothing here moves her: no force,
//    no velocity, no position is written.
// Down (lie_down, stand_up) keeps its own lying and kneeling shapes.

#include "sim/matron.hpp"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#include <chrono>
#include <cmath>

namespace scraperx::sim {
using namespace JPH;

namespace {
constexpr float kTwoPi = 6.28318530718F;
Matron::ShapeAudit g_shape_audit{};
bool g_shape_audit_on = false;
} // namespace

void Matron::audit_shapes(const bool on) noexcept {
    g_shape_audit_on = on;
    g_shape_audit = ShapeAudit{};
}

const Matron::ShapeAudit &Matron::shape_audit() noexcept { return g_shape_audit; }

// Her capsule in each stance, in her body frame (y up from her middle, z
// forward along her yaw): the end points of its segment and its radius.
// Fitted to her drawn body (her limb volumes as tools/matron_probe recorded
// them in the touch scenarios of her walk, hunt, ladder, pipe and holds:
// tests/data/matron_drawn_limbs.inc): the capsule that least exceeds the
// larger of how far her body (trunk, head, legs) reaches out of it and how
// far it reaches past her drawn body, no wider than the 0.30-0.32 m her ways
// plan her through gaps with (kRadius), so a gap planned clear stays clear.
Matron::StanceCapsule Matron::stance_capsule(const Stance stance) noexcept {
    switch (stance) {
    case Stance::Low:
        // Hunting low, on all fours or crouched: hips high over her soles,
        // her chest, shoulders and head down and out ahead of her.
        return {-1.05F + 0.32F, -0.01F, 0.02F, 0.32F, 0.32F};
    case Stance::Climb:
        // Hung from her hands (rungs, a line, a mantle, a cage's frame):
        // under her hands, leaning in to the steel she climbs.
        return {-1.05F + 0.32F, 0.06F, 0.33F, 0.33F, 0.32F};
    case Stance::Pipe:
        // On her pipe, hugging the riser ahead of her, her knees drawn up to
        // her feet's brace on it (head first or feet first, the same body).
        return {-0.73F, 0.22F, 0.31F, 0.34F, 0.30F};
    case Stance::Hold:
        // On her feet with the player in her hand: stooped over him, her
        // shoulders out over her arm.
        return {-1.05F + 0.31F, -0.08F, 0.24F, 0.26F, 0.31F};
    case Stance::Upright:
    case Stance::Down:
    default:
        // Up and stooped: her trunk tipped forward over her soles, her head
        // out ahead of her middle.
        return {-1.05F + 0.32F, -0.03F, 0.12F, 0.26F, 0.32F};
    }
}

void Matron::build_stance_shapes() {
    for (int s = 0; s < kStanceCount; ++s) {
        const StanceCapsule c = stance_capsule(static_cast<Stance>(s));
        const Vec3 a(0.0F, c.ay, c.az);
        const Vec3 b(0.0F, c.by, c.bz);
        for (int k = 0; k < kStanceYawBins; ++k) {
            const float yaw = kTwoPi * static_cast<float>(k) / static_cast<float>(kStanceYawBins);
            const Quat turn = Quat::sRotation(Vec3::sAxisY(), yaw);
            const Vec3 wa = turn * a;
            const Vec3 wb = turn * b;
            const Vec3 axis = wb - wa;
            const float length = axis.Length();
            // The capsule's own axis is y: turned onto the segment.
            const Quat onto = length > 1.0e-4F ? Quat::sFromTo(Vec3::sAxisY(), axis / length) : Quat::sIdentity();
            const Ref<Shape> placed =
                new RotatedTranslatedShape(0.5F * (wa + wb), onto, new CapsuleShape(0.5F * length, c.radius));
            // Her centre of mass stays at her middle, whatever the shape.
            stance_shapes_[static_cast<std::size_t>(s)][static_cast<std::size_t>(k)] =
                new OffsetCenterOfMassShape(placed, -placed->GetCenterOfMass());
        }
    }
}

int Matron::stance_bin(const float yaw) noexcept {
    const float turns = yaw / kTwoPi;
    const float frac = turns - std::floor(turns);
    return static_cast<int>(std::lround(frac * static_cast<float>(kStanceYawBins))) % kStanceYawBins;
}

// Her stance family, from her simulated state: what the view poses her from
// (matron_view.gd _pose), read here from the same facts.
Matron::Stance Matron::wanted_stance() const {
    if (lying_) {
        return Stance::Down;
    }
    if (holding_state(state_)) {
        return Stance::Hold;
    }
    if (view_.on_pipe) {
        return Stance::Pipe;
    }
    if (view_.climbing || riding_ || on_line_ || view_.mantling) {
        return Stance::Climb;
    }
    // On the hunt she is low (on all fours, or crouched stopped) but for a
    // pest hurrying after the player upright (over 1 m/s, stage 0).
    if (state_ == State::Hunting && (stage_ != Stage::Pest || speed_ <= kLowHuntMps)) {
        return Stance::Low;
    }
    return Stance::Upright;
}

const Shape *Matron::stance_shape(const Stance stance, const float yaw) const {
    return stance_shapes_[static_cast<std::size_t>(stance)][static_cast<std::size_t>(stance_bin(yaw))].GetPtr();
}

void Matron::step_stance(const float dt) {
    if (!body_in_world_ || dead_) {
        return;
    }
    const auto started = std::chrono::steady_clock::now();
    const Stance want = wanted_stance();
    stance_want_s_ = want == stance_want_ ? stance_want_s_ + dt : 0.0F;
    stance_want_ = want;
    BodyInterface &bodies = physics_.GetBodyInterface();
    if (want != Stance::Down) {
        const Shape *target = stance_shape(want, yaw_);
        const RefConst<Shape> have = bodies.GetShape(body_);
        // A new family is taken once it has been wanted kStanceDwellS (her
        // drawn body takes longer than that to go over); a new turn of the
        // same one at once.
        const bool family_change = want != stance_;
        if (have.GetPtr() != target && (!family_change || stance_want_s_ >= kStanceDwellS || on_planning_shape())) {
            const RVec3 at = bodies.GetPosition(body_);
            if (shape_fits(target, at)) {
                bodies.SetShape(body_, target, false, EActivation::Activate);
                if (family_change) {
                    stance_ = want;
                    ++stance_switches_;
                    if (g_shape_audit_on) {
                        ++g_shape_audit.switches;
                    }
                }
            } else if (g_shape_audit_on) {
                ++g_shape_audit.refused;
            }
        }
    } else {
        stance_ = Stance::Down;
    }
    if (g_shape_audit_on) {
        g_shape_audit.upkeep_s +=
            std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        ++g_shape_audit.ticks;
        ++g_shape_audit.per_stance[static_cast<std::size_t>(stance_)];
        const float depth = steel_depth();
        g_shape_audit.worst_depth_m = std::max(g_shape_audit.worst_depth_m, depth);
        if (depth > kInsideSteelM) {
            ++g_shape_audit.inside_steel;
        }
        g_shape_audit.query_s +=
            std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    }
}

// She still has the capsule her ways plan for (shape_: just up off the steel,
// put back on her feet by a reset, or new): it is changed at once, where her
// stance's fits.
bool Matron::on_planning_shape() const {
    const RefConst<Shape> have = physics_.GetBodyInterface().GetShape(body_);
    return have.GetPtr() == shape_.GetPtr();
}

// How deep her body is in the steel this tick: the deepest penetration of
// her shape as it is into a body that does not move (the world's solids, a
// locker), sensors aside. Read-only; the test audit's, not her movement's.
float Matron::steel_depth() const {
    const BodyLockRead her(physics_.GetBodyLockInterface(), body_);
    if (!her.Succeeded()) {
        return 0.0F;
    }
    const Body &body = her.GetBody();
    AllHitCollisionCollector<CollideShapeCollector> hits;
    CollideShapeSettings settings;
    settings.mMaxSeparationDistance = 0.0F;
    IgnoreMultipleBodiesFilter not_us;
    not_us.Reserve(2);
    not_us.IgnoreBody(body_);
    not_us.IgnoreBody(player_);
    physics_.GetNarrowPhaseQuery().CollideShape(body.GetShape(), Vec3::sReplicate(1.0F),
                                                body.GetCenterOfMassTransform(), settings, body.GetPosition(), hits,
                                                {}, {}, not_us);
    float deepest = 0.0F;
    for (const CollideShapeResult &hit : hits.mHits) {
        const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mBodyID2);
        if (!lock.Succeeded() || lock.GetBody().IsSensor() || lock.GetBody().IsDynamic()) {
            continue;
        }
        deepest = std::max(deepest, hit.mPenetrationDepth);
    }
    return deepest;
}

void Matron::refresh_shape_view() {
    const StanceCapsule c = stance_capsule(stance_);
    view_.stance = static_cast<std::uint8_t>(stance_);
    const float yaw = kTwoPi * static_cast<float>(stance_bin(yaw_)) / static_cast<float>(kStanceYawBins);
    const float s = std::sin(yaw);
    const float co = std::cos(yaw);
    view_.shape_a = {c.az * s, c.ay, c.az * co};
    view_.shape_b = {c.bz * s, c.by, c.bz * co};
    view_.shape_r = c.radius;
    view_.shape_switches = stance_switches_;
}

} // namespace scraperx::sim
