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
//  - every capsule stands on her soles about her middle's axis, so a lower
//    one is inside the one she has and is taken at once; a taller one only
//    where it fits (stance_fits: a collide query at her middle with the steel
//    and every body, the player's too); otherwise she keeps the shape she has
//    and her own movement takes her to where it fits. Nothing here moves her: no force,
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
// Sized to her drawn body (her limb volumes as tools/matron_probe recorded
// them in the touch scenarios of her walk, hunt, ladder, pipe and holds:
// tests/data/matron_drawn_limbs.inc): its top at the top of her drawn trunk,
// head and upper arms (the 95th percentile of her frames in that stance;
// her forearms and hands are her reach, on a rung, a rail or his ankle), not
// 0.3 to 0.9 m over it. Each stays inside her old 2.1 m capsule (her middle's
// axis, kRadius, its bottom at her soles), so she meets nothing she did not
// meet before: her catch (kCatchM 0.85 m middle to middle), her strike and
// the player's contacts are tuned to a body 0.30 m deep. Her drawn trunk and
// head lean 0.4-0.7 m ahead of her middle; a capsule set forward there
// (tried: 0.25-0.35 m forward) stops her hand short of his ankle (her front
// 0.64 m out, his radius 0.35 m, past her 0.85 m catch), so her body moving
// forward waits on her catch and strike measured from her drawn front.
Matron::StanceCapsule Matron::stance_capsule(const Stance stance) noexcept {
    constexpr float kFoot = -kStandCentre + kRadius;   // her soles' hemisphere
    switch (stance) {
    case Stance::Low:
        // Hunting low, on all fours or crouched: 1.61 m over the steel.
        return {kFoot, 0.0F, 0.26F, 0.0F, kRadius};
    case Stance::Climb:
        // Hung from her hands on a climb: her shoulders under her hands,
        // 1.82 m over her soles.
        return {kFoot, 0.0F, 0.47F, 0.0F, kRadius};
    case Stance::Lift:
        // Carrying him, holding him out, tearing his arm: her shoulder and
        // upper arm raised with him (to 2.6 m carried): all her old capsule.
        return {kFoot, 0.0F, kHalfHeight, 0.0F, kRadius};
    case Stance::Hold:
    case Stance::Pipe:
    case Stance::Upright:
    case Stance::Down:
    default:
        // Up and stooped, on her pipe, or stooped to his ankle, him held out
        // or set down: her drawn head at most 1.79 m over her soles (her rig
        // is 1.80 m).
        return {kFoot, 0.0F, 0.44F, 0.0F, kRadius};
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
    // Him up in her hand (carried, held out over the drop, his arm torn):
    // her old height. Held out (Dangle) her drawn arm is level with her
    // shoulder, but his hanging body rests against that height, and it holds
    // his wrist out of her reach until her holds are staged at her real arm's
    // length (issue #3, the hold restaging); then Dangle goes to Hold.
    if (state_ == State::Carry || state_ == State::Dangle || state_ == State::ArmPlay) {
        return Stance::Lift;
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
            // Every stance capsule stands on her soles about her middle's
            // axis with her radius: one no taller than hers is inside it and
            // is taken at once; a taller one only where all of it is free of
            // the steel and of every body, the player's too (grown into him,
            // her body would throw him off her hand).
            const float have_top = on_planning_shape() ? kHalfHeight : stance_capsule(stance_).by;
            if (stance_capsule(want).by <= have_top + 1.0e-4F || stance_fits(target, at)) {
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
        if (on_planning_shape()) {
            // Not yet on a stance shape (none has fitted where she was put):
            // her old capsule, counted apart.
            ++g_shape_audit.planning_ticks;
            g_shape_audit.planning_worst_m = std::max(g_shape_audit.planning_worst_m, depth);
            steel_ticks_ = 0;
            g_shape_audit.query_s +=
                std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            return;
        }
        if (!placed_by_test(depth)) {
            g_shape_audit.worst_depth_m = std::max(g_shape_audit.worst_depth_m, depth);
        }
        float &worst = g_shape_audit.worst_depth_by_stance[static_cast<std::size_t>(stance_)];
        worst = std::max(worst, depth);
        // Steel her old capsule would be as deep in where she is (a test's
        // pole put through her, a blow's stock against her) is where she was
        // put, not where her shape took her: counted apart.
        const bool placed = depth > kInsideSteelM && steel_depth(shape_.GetPtr()) >= depth - 0.005F;
        if (placed) {
            ++g_shape_audit.placed_ticks;
            g_shape_audit.placed_worst_m = std::max(g_shape_audit.placed_worst_m, depth);
            steel_ticks_ = 0;
        } else if (depth > kInsideSteelM) {
            ++g_shape_audit.inside_steel;
            if (++steel_ticks_ > kInsideSteelTicks) {
                ++g_shape_audit.sitting;
            }
        } else {
            steel_ticks_ = 0;
        }
        g_shape_audit.query_s +=
            std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    }
}

// A taller capsule's room: free of everything but her own body (sensors
// aside), resting on a floor allowed.
bool Matron::placed_by_test(const float depth) const {
    return depth > kInsideSteelM && steel_depth(shape_.GetPtr()) >= depth - 0.005F;
}

bool Matron::stance_fits(const Shape *shape, const RVec3 at) const {
    AllHitCollisionCollector<CollideShapeCollector> hits;
    CollideShapeSettings settings;
    settings.mMaxSeparationDistance = 0.0F;
    const IgnoreSingleBodyFilter not_her(body_);
    physics_.GetNarrowPhaseQuery().CollideShape(shape, Vec3::sReplicate(1.0F),
                                                RMat44::sTranslation(at + RVec3(shape->GetCenterOfMass())), settings,
                                                at, hits, {}, {}, not_her);
    for (const CollideShapeResult &hit : hits.mHits) {
        if (hit.mPenetrationDepth <= 0.03F) {
            continue;
        }
        const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mBodyID2);
        if (lock.Succeeded() && lock.GetBody().IsSensor()) {
            continue;
        }
        return false;
    }
    return true;
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
// locker) and that her body collides with, sensors aside. Read-only; the test audit's, not her movement's.
float Matron::steel_depth(const Shape *as) const {
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
    // Only what her body meets (her layer's pairs): a body she passes
    // through by the world's own filters is not steel she is in.
    const ObjectLayer layer = body.GetObjectLayer();
    const Shape *shape = as != nullptr ? as : body.GetShape();
    physics_.GetNarrowPhaseQuery().CollideShape(shape, Vec3::sReplicate(1.0F),
                                                RMat44::sTranslation(body.GetPosition() + RVec3(shape->GetCenterOfMass())), settings, body.GetPosition(), hits,
                                                physics_.GetDefaultBroadPhaseLayerFilter(layer),
                                                physics_.GetDefaultLayerFilter(layer), not_us);
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
