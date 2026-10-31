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
    // A journey was scanned and is moved for the tall capsule (capsule_free,
    // the found-climb probe). A shorter body walks into gaps that scan never
    // offered her, and the move then has nothing to hold. While that journey
    // runs she keeps the tall capsule, where it still fits. Off a journey the
    // stance capsule is hers, a shorter one taken at once.
    if (travel_.active && want != Stance::Down) {
        if (!on_planning_shape()) {
            const RVec3 at = bodies.GetPosition(body_);
            if (stance_fits(shape_.GetPtr(), at)) {
                bodies.SetShape(body_, shape_, false, EActivation::Activate);
            }
        }
    } else if (want != Stance::Down) {
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
        const RVec3 at = bodies.GetPosition(body_);
        // A placement puts her somewhere. Physics does not move her a metre
        // in one tick (kTunnelM is 0.15 m; a metre is a test's SetPosition).
        // The static bodies she already overlaps there are the setup. A later
        // walk into other steel is not.
        const float jumped = have_shape_pos_ ? float((at - shape_pos_).Length()) : 1.0e9F;
        if (!have_shape_pos_ || jumped > 1.0F) {
            note_setup_overlaps();
        }
        shape_pos_ = at;
        have_shape_pos_ = true;
        g_shape_audit.upkeep_s +=
            std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        ++g_shape_audit.ticks;
        ++g_shape_audit.per_stance[static_cast<std::size_t>(stance_)];
        float depth = steel_depth();
        if (on_planning_shape()) {
            // A journey may retain the planning capsule. Count its samples,
            // then apply the same audit to the body actually installed.
            ++g_shape_audit.planning_ticks;
            g_shape_audit.planning_worst_m = std::max(g_shape_audit.planning_worst_m, depth);
        }
        float &worst = g_shape_audit.worst_depth_by_stance[static_cast<std::size_t>(stance_)];
        worst = std::max(worst, depth);
        // Clear of steel ends the placement's exemption. Until then, only the
        // bodies recorded at that placement, and no deeper, are put there.
        if (depth <= kInsideSteelM) {
            setup_live_ = false;
            steel_ticks_ = 0;
        } else if (placed_by_test(depth)) {
            ++g_shape_audit.placed_ticks;
            g_shape_audit.placed_worst_m = std::max(g_shape_audit.placed_worst_m, depth);
            steel_ticks_ = 0;
        } else {
            if (depth >= g_shape_audit.worst_depth_m) {
                const float tall = steel_depth(shape_.GetPtr());
                g_shape_audit.worst_tall_m = tall;
                g_shape_audit.worst_x = float(at.GetX());
                g_shape_audit.worst_y = float(at.GetY());
                g_shape_audit.worst_z = float(at.GetZ());
                g_shape_audit.worst_stance = static_cast<int>(stance_);
                g_shape_audit.worst_state = static_cast<int>(state_);
            }
            g_shape_audit.worst_depth_m = std::max(g_shape_audit.worst_depth_m, depth);
            ++g_shape_audit.inside_steel;
            if (++steel_ticks_ > kInsideSteelTicks) {
                ++g_shape_audit.sitting;
            }
        }
        g_shape_audit.query_s +=
            std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    }
}

// A taller capsule's room: free of everything but her own body (sensors
// aside), resting on a floor allowed.
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

// Static bodies her shape penetrates past kInsideSteelM. Same filters as
// steel_depth: what she collides with, sensors and moving bodies aside.
int Matron::collect_steel(const Shape *as, SetupOverlap *out, const int cap) const {
    if (out == nullptr || cap <= 0) {
        return 0;
    }
    const BodyLockRead her(physics_.GetBodyLockInterface(), body_);
    if (!her.Succeeded()) {
        return 0;
    }
    const Body &body = her.GetBody();
    AllHitCollisionCollector<CollideShapeCollector> hits;
    CollideShapeSettings settings;
    settings.mMaxSeparationDistance = 0.0F;
    IgnoreMultipleBodiesFilter not_us;
    not_us.Reserve(2);
    not_us.IgnoreBody(body_);
    not_us.IgnoreBody(player_);
    const ObjectLayer layer = body.GetObjectLayer();
    const Shape *shape = as != nullptr ? as : body.GetShape();
    physics_.GetNarrowPhaseQuery().CollideShape(shape, Vec3::sReplicate(1.0F),
                                                RMat44::sTranslation(body.GetPosition() + RVec3(shape->GetCenterOfMass())),
                                                settings, body.GetPosition(), hits,
                                                physics_.GetDefaultBroadPhaseLayerFilter(layer),
                                                physics_.GetDefaultLayerFilter(layer), not_us);
    int n = 0;
    for (const CollideShapeResult &hit : hits.mHits) {
        if (hit.mPenetrationDepth <= kInsideSteelM) {
            continue;
        }
        const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mBodyID2);
        if (!lock.Succeeded() || lock.GetBody().IsSensor() || lock.GetBody().IsDynamic()) {
            continue;
        }
        int slot = -1;
        for (int i = 0; i < n; ++i) {
            if (out[i].body == hit.mBodyID2) {
                slot = i;
                break;
            }
        }
        if (slot >= 0) {
            out[slot].depth = std::max(out[slot].depth, hit.mPenetrationDepth);
            continue;
        }
        if (n < cap) {
            out[n].body = hit.mBodyID2;
            out[n].depth = hit.mPenetrationDepth;
            ++n;
            continue;
        }
        int shallow = 0;
        for (int i = 1; i < n; ++i) {
            if (out[i].depth < out[shallow].depth) {
                shallow = i;
            }
        }
        if (hit.mPenetrationDepth > out[shallow].depth) {
            out[shallow].body = hit.mBodyID2;
            out[shallow].depth = hit.mPenetrationDepth;
        }
    }
    return n;
}

void Matron::note_setup_overlaps() {
    setup_overlap_n_ = collect_steel(nullptr, setup_overlaps_.data(), static_cast<int>(setup_overlaps_.size()));
    setup_live_ = setup_overlap_n_ > 0;
}

// True only while she is still in steel a placement recorded, and no deeper
// than that placement measured. The old tall capsule is not the test: every
// stance capsule sits inside it, so a walk into a wall used to look placed.
bool Matron::placed_by_test(const float depth) const {
    if (!setup_live_ || depth <= kInsideSteelM || setup_overlap_n_ == 0) {
        return false;
    }
    SetupOverlap hits[16];
    const int n = collect_steel(nullptr, hits, 16);
    if (n == 0) {
        return false;
    }
    for (int i = 0; i < n; ++i) {
        bool known = false;
        for (int k = 0; k < setup_overlap_n_; ++k) {
            const SetupOverlap &was = setup_overlaps_[static_cast<std::size_t>(k)];
            if (was.body == hits[i].body && hits[i].depth <= was.depth + 0.005F) {
                known = true;
                break;
            }
        }
        if (!known) {
            return false;
        }
    }
    return true;
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
    view_.shape_switches = stance_switches_;
    // lie_down installs its own capsule (along the fall, or the short kneel).
    // The upright stance is not that body.
    if (lying_ && down_shape_ != nullptr) {
        view_.stance = static_cast<std::uint8_t>(Stance::Down);
        view_.shape_a = down_a_;
        view_.shape_b = down_b_;
        view_.shape_r = down_r_;
        return;
    }
    if (body_in_world_ && on_planning_shape()) {
        // The journey's tall capsule, not the stance one she is waiting to take.
        view_.stance = static_cast<std::uint8_t>(stance_);
        view_.shape_a = {0.0F, -kHalfHeight, 0.0F};
        view_.shape_b = {0.0F, kHalfHeight, 0.0F};
        view_.shape_r = kRadius;
        return;
    }
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
