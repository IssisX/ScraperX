// Her parachute (Cory, 2026-10-27): a long fall is not the end of her. Falling
// past kChuteDeployMps with more than kChuteDropM of air under her, she opens
// a canopy like the player's (kParachuteDragCoefficient), steers it to a deck
// her own scan joins to the player's, lands and searches again. Every force of
// the canopy is drawn from her tick's one account through Budget::take
// (8,000 N / 22 kW; Cory 16:01 CT) and applied with AddForce before the step:
// nothing here writes her velocity.
//
// No chute when her fall began with the floor under her giving way (a dynamic
// floor falling with her): that is the catwalk's designed kill, and any other
// floor that fails the same way. A rule, not a place.
//
// The safety net (the last resort, logged loudly): off the world, a landing
// that would kill her, or landed where her scan cannot plan to the player's
// last standing region. It puts her back a deck or so below him, on a spot it
// has checked (floor there, room to stand) and that he cannot see; while her
// own spot is in his sight, or no hidden spot qualifies, it waits and tries
// again each second. It never pops her into his view.

#include "sim/matron.hpp"
#include "sim/trapeze.hpp"
#include "sim/physics_world_tuning.hpp"

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace scraperx::sim {
using namespace JPH;

namespace {
constexpr float kGlideRatio = 0.12F;    // horizontal meters per meter of drop she makes good
constexpr float kChuteFlyMps = 4.0F;    // horizontal speed she steers toward
constexpr float kLostSeconds = 20.0F;   // open this long without landing: the net
constexpr float kGaveWayWindowS = 1.0F; // a floor falling with her this soon after she left it gave way
constexpr float kRespawnBelowMinM = 3.0F, kRespawnBelowMaxM = 16.0F, kRespawnBelowWideM = 30.0F;
constexpr float kRespawnNearM = 8.0F, kRespawnFarM = 40.0F;
constexpr float kRetrySeconds = 1.0F;

int chute_region_under(const traversal::AffordanceMap &map, const RVec3 body_center) {
    for (const float down : {0.9F, 1.0F, 1.2F, 0.6F, 1.5F}) {
        const int r = map.region_at(Vec3(float(body_center.GetX()), float(body_center.GetY()) - down,
                                         float(body_center.GetZ())));
        if (r >= 0) return r;
    }
    return -1;
}
} // namespace

bool Matron::joined_to_player(const int region) const {
    if (scanned_ == nullptr || region < 0) return false;
    if (player_region_ < 0) return true;   // his region not yet known: no evidence she is cut off
    return region == player_region_ || !scanned_->plan(region, player_region_).empty();
}

void Matron::chute_reset() {
    chute_open_ = false;
    chute_has_goal_ = false;
    chute_s_ = 0.0F;
    floor_gave_way_ = false;
    stood_on_ = BodyID();
    respawn_pending_ = false;
    respawn_retry_s_ = 0.0F;
}

void Matron::chute_stand_up() {
    drop_stock();
    for (Mover *way : ways_) {
        if (way->status() == MoverStatus::Running) way->cancel();
    }
    if (lying_ && body_in_world_) {
        BodyInterface &bodies = physics_.GetBodyInterface();
        bodies.SetShape(body_, shape_, false, EActivation::DontActivate);
        bodies.SetFriction(body_, 0.0F);
    }
    lying_ = false;
    kneeling_ = false;
    down_shape_ = nullptr;
    stagger_s_ = 0.0F;
    down_s_ = 0.0F;
    down_for_s_ = 0.0F;
    turn_ = Turn::None;
    turn_s_ = 0.0F;
    riding_ = false;
    physics_.GetBodyInterface().SetGravityFactor(body_, 1.0F);
}

void Matron::chute_watch(const float dt) {
    if (dead_) {
        chute_reset();
        return;
    }
    BodyInterface &bodies = physics_.GetBodyInterface();
    if (scanned_ != nullptr) {
        // His last standing region: on a ladder, hanging, mid-jump or under his
        // own canopy he has none, and the last one stands (finding 3).
        const int r = chute_region_under(*scanned_, bodies.GetPosition(player_));
        if (r >= 0) player_region_ = r;
    }
    if (respawn_pending_) {
        respawn_retry_s_ -= dt;
        if (respawn_retry_s_ <= 0.0F && !chute_try_respawn()) {
            respawn_retry_s_ = kRetrySeconds;
            ++respawn_waits_;
        }
        return;
    }
    if (!body_in_world_) {
        chute_open_ = false;
        return;
    }
    const RVec3 at = bodies.GetPosition(body_);
    const Vec3 v = bodies.GetLinearVelocity(body_);
    // What is under her: her center and four points on her rim (a ring's
    // edge under one side catches her as surely as a deck under her middle).
    RayCastResult hit;
    bool any = false;
    float drop = std::numeric_limits<float>::infinity();
    for (const Vec3 off : {Vec3::sZero(), Vec3(kRadius, 0, 0), Vec3(-kRadius, 0, 0), Vec3(0, 0, kRadius),
                           Vec3(0, 0, -kRadius)}) {
        RayCastResult h;
        const RRayCast down(at + off, Vec3(0.0F, -80.0F, 0.0F));
        if (physics_.GetNarrowPhaseQuery().CastRay(down, h, {}, {}, IgnoreSingleBodyFilter(body_)) &&
            h.mFraction * 80.0F - kStandCentre < drop) {
            drop = h.mFraction * 80.0F - kStandCentre;
            hit = h;
            any = true;
        }
    }
    if (any && drop < 0.3F && std::abs(fall_vy_) < 2.0F && std::abs(v.GetY()) < 2.0F) {   // standing
        stood_on_ = hit.mBodyID;
        floor_gave_way_ = false;
        air_s_ = 0.0F;
    } else {
        air_s_ += dt;
        if (!chute_open_ && !floor_gave_way_ && air_s_ < kGaveWayWindowS && !stood_on_.IsInvalid()) {
            // The floor she last stood on, moving and falling with her: it gave way.
            const BodyLockRead lock(physics_.GetBodyLockInterface(), stood_on_);
            if (lock.Succeeded() && lock.GetBody().IsDynamic() && lock.GetBody().GetLinearVelocity().GetY() < -1.0F) {
                floor_gave_way_ = true;
            }
        }
    }
    const Mover *way = holder();
    if (riding_ || holding_state(state_) || (way != nullptr && (way->view().held || way->view().lands))) {
        chute_open_ = false;
        return;
    }
    if (!chute_open_) {
        if (-v.GetY() < kChuteDeployMps || drop <= kChuteDropM || floor_gave_way_) return;
        chute_open_ = true;
        chute_s_ = 0.0F;
        ++chutes_;
        chute_open_mps_ = std::max(chute_open_mps_, -float(v.GetY()));
        // Every way she was on ends: the canopy has her now (finding 2).
        for (Mover *w : ways_) {
            if (w->status() == MoverStatus::Running) w->cancel();
        }
        chute_has_goal_ = chute_pick(at);
        std::fprintf(stderr,
                     "[matron-chute] OPEN #%u at (%.2f, %.2f, %.2f) falling %.2f m/s, %.1f m of air; goal %s (%.2f, %.2f, %.2f)\n",
                     chutes_, double(at.GetX()), double(at.GetY()), double(at.GetZ()), double(-v.GetY()),
                     double(drop), chute_has_goal_ ? "joined" : "NONE", double(chute_x_), double(chute_top_),
                     double(chute_z_));
    }
    chute_s_ += dt;
    if (chute_has_goal_ && float(at.GetY()) - kStandCentre < chute_top_ - 0.3F) {
        chute_has_goal_ = chute_pick(at);   // missed it: the next deck down that joins his
        std::fprintf(stderr, "[matron-chute] missed her deck; new goal %s (%.2f, %.2f, %.2f)\n",
                     chute_has_goal_ ? "joined" : "NONE", double(chute_x_), double(chute_top_), double(chute_z_));
    }
    if (any && drop < 0.2F && chute_s_ > 0.2F) {
        chute_open_ = false;
        feet_y_ = float(at.GetY()) - kStandCentre;
        // The net only if her scan truly cannot plan from here to his last
        // standing region (finding 3): a landing off her goal is still a landing.
        const int here = scanned_ != nullptr ? chute_region_under(*scanned_, at) : -1;
        if (!chute_has_goal_ && !joined_to_player(here)) {
            chute_respawn("landed where no deck joins the player's");
            return;
        }
        // Her own mind takes it from here, from her last real evidence of him.
        if (!out(state_) || state_ == State::Shuffling) state_ = State::Searching;
        const float speed = -float(v.GetY());
        std::fprintf(stderr, "[matron-chute] LANDED at (%.2f, %.2f, %.2f) after %.1f s at %.1f m/s%s\n",
                     double(at.GetX()), double(feet_y_), double(at.GetZ()), double(chute_s_), double(speed),
                     speed >= kHardLandingMps ? " (hard: down a while, then searching)" : "; searching");
        return;
    }
    if (chute_s_ > kLostSeconds) {
        chute_open_ = false;
        chute_respawn("chute open too long without a landing");
    }
}

void Matron::chute_drive(const float dt, traversal::Budget &account) {
    (void)dt;
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    const Vec3 v = bodies.GetLinearVelocity(body_);
    account.velocity = v;   // the canopy's work is against the air: her velocity is the one that counts
    const float speed = v.Length();
    Vec3 force = Vec3::sZero();
    if (speed > 1.0e-4F) {
        const float drag = kMassKg * detail::kParachuteDragCoefficient * speed * speed;
        force += account.take(-(v / speed) * drag);
    }
    if (chute_has_goal_) {
        Vec3 to(chute_x_ - float(at.GetX()), 0.0F, chute_z_ - float(at.GetZ()));
        const float dist = to.Length();
        const Vec3 want = dist > 0.05F ? to / dist * std::min(kChuteFlyMps, dist) : Vec3::sZero();
        Vec3 steer = (want - Vec3(v.GetX(), 0.0F, v.GetZ())) * (kMassKg / 0.5F);
        const float most = kMassKg * kChuteSteerAccel;
        if (steer.Length() > most) steer = steer.Normalized() * most;
        force += account.take(steer);
    }
    if (force.LengthSq() > 0.0F) bodies.AddForce(body_, force);
    foot_force_n_ = account.used;
    foot_power_w_ = account.power_used;
    chute_peak_force_n_ = std::max(chute_peak_force_n_, account.used);
    chute_peak_power_w_ = std::max(chute_peak_power_w_, account.power_used);
}

bool Matron::chute_pick(const RVec3 at) {
    if (scanned_ == nullptr) return false;
    const traversal::AffordanceMap &map = *scanned_;
    const RVec3 p = physics_.GetBodyInterface().GetPosition(player_);
    float best = std::numeric_limits<float>::infinity();
    for (const traversal::Patch &patch : map.patches()) {
        if (patch.narrow || patch.top > float(at.GetY()) - kStandCentre - 1.0F) continue;
        if ((patch.max_x - patch.min_x) < 1.0F || (patch.max_z - patch.min_z) < 1.0F) continue;
        const float x = std::clamp(float(at.GetX()), patch.min_x + 0.5F, patch.max_x - 0.5F);
        const float z = std::clamp(float(at.GetZ()), patch.min_z + 0.5F, patch.max_z - 0.5F);
        const float fall = float(at.GetY()) - kStandCentre - patch.top;
        const float reach = std::hypot(x - float(at.GetX()), z - float(at.GetZ()));
        if (reach > kGlideRatio * fall + 1.0F) continue;
        if (!joined_to_player(patch.region)) continue;   // never an island: only decks that join his
        const float score = std::abs(patch.top - float(p.GetY())) + 0.2F * reach;
        if (score < best) {
            best = score;
            chute_x_ = x;
            chute_z_ = z;
            chute_top_ = patch.top;
        }
    }
    return std::isfinite(best);
}

bool Matron::chute_hidden_from_player(const RVec3 feet) const {
    const RVec3 eye = physics_.GetBodyInterface().GetPosition(player_) + Vec3(0.0F, 0.6F, 0.0F);
    Vec3 across = Vec3(feet - eye).Cross(Vec3::sAxisY());
    across = across.LengthSq() > 1.0e-6F ? across.Normalized() * 0.3F : Vec3(0.3F, 0.0F, 0.0F);
    IgnoreMultipleBodiesFilter not_us;
    not_us.Reserve(2);
    not_us.IgnoreBody(player_);
    not_us.IgnoreBody(body_);
    // Feet, middle, head, and either side of her head: a thin rail hides one
    // line, not all of her. Hidden only when every line is blocked.
    for (const Vec3 point : {Vec3(0.0F, 0.3F, 0.0F), Vec3(0.0F, kStandCentre, 0.0F), Vec3(0.0F, 1.6F, 0.0F),
                             Vec3(0.0F, 1.6F, 0.0F) + across, Vec3(0.0F, 1.6F, 0.0F) - across}) {
        RayCastResult block;
        const RRayCast sight(eye, Vec3(feet + point - eye));
        if (!physics_.GetNarrowPhaseQuery().CastRay(sight, block, {}, {}, not_us) || block.mFraction > 0.98F) {
            return false;
        }
    }
    return true;
}

void Matron::chute_respawn(const char *why) {
    if (respawn_pending_) return;
    respawn_pending_ = true;
    respawn_why_ = why;
    chute_open_ = false;
    chute_has_goal_ = false;
    chute_stand_up();   // the death path's teardown (finding 4): the stock let go, her ways ended, on her feet
    if (!chute_try_respawn()) {
        respawn_retry_s_ = kRetrySeconds;
        ++respawn_waits_;
    }
}

bool Matron::chute_try_respawn() {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 p = bodies.GetPosition(player_);
    RVec3 from = RVec3::sZero();
    if (body_in_world_) {
        from = bodies.GetPosition(body_);
        // She does not vanish before his eyes: while her own spot is in his
        // sight (and still in the world) the net waits.
        if (from.GetY() > kOffTheWorldY && !chute_hidden_from_player(from - Vec3(0.0F, kStandCentre, 0.0F))) {
            return false;
        }
        remove_body();   // out of the world while the net looks for a spot
    }
    if (scanned_ == nullptr || player_region_ < 0) return false;
    const traversal::AffordanceMap &map = *scanned_;
    // After ten seconds' wait the band below him widens.
    const float below_max = respawn_waits_ >= 10 ? kRespawnBelowWideM : kRespawnBelowMaxM;
    IgnoreMultipleBodiesFilter not_us;
    not_us.Reserve(2);
    not_us.IgnoreBody(player_);
    not_us.IgnoreBody(body_);
    float best = std::numeric_limits<float>::infinity();
    RVec3 to = RVec3::sZero();
    for (const traversal::Patch &patch : map.patches()) {
        if (patch.narrow || (patch.max_x - patch.min_x) < 1.2F || (patch.max_z - patch.min_z) < 1.2F) continue;
        const float below = float(p.GetY()) - patch.top;
        if (below < kRespawnBelowMinM || below > below_max) continue;
        if (patch.region != player_region_ && map.plan(patch.region, player_region_).empty()) continue;
        const float cx = 0.5F * (patch.min_x + patch.max_x), cz = 0.5F * (patch.min_z + patch.max_z);
        const float qx = 0.25F * (patch.max_x - patch.min_x), qz = 0.25F * (patch.max_z - patch.min_z);
        for (const Vec3 c : {Vec3(cx, 0, cz), Vec3(cx - qx, 0, cz - qz), Vec3(cx + qx, 0, cz - qz),
                             Vec3(cx - qx, 0, cz + qz), Vec3(cx + qx, 0, cz + qz)}) {
            const float x = c.GetX(), z = c.GetZ();
            const float d = std::hypot(x - float(p.GetX()), z - float(p.GetZ()));
            if (d < kRespawnNearM || d > kRespawnFarM) continue;
            const float score = std::abs(below - 11.0F) + 0.1F * d;
            if (score >= best) continue;
            // The spot itself, checked now (finding 7): floor there at the
            // patch's top (not a moved cage or a parked body), room to stand.
            RayCastResult floor;
            const RRayCast down(RVec3(x, patch.top + 1.0F, z), Vec3(0.0F, -2.0F, 0.0F));
            if (!physics_.GetNarrowPhaseQuery().CastRay(down, floor, {}, {}, not_us)) continue;
            const float floor_y = patch.top + 1.0F - 2.0F * floor.mFraction;
            if (std::abs(floor_y - patch.top) > 0.08F) continue;
            {
                const BodyLockRead lock(physics_.GetBodyLockInterface(), floor.mBodyID);
                if (!lock.Succeeded() || lock.GetBody().IsDynamic()) continue;
            }
            const RVec3 center(x, floor_y + kStandCentre + 0.02F, z);
            if (!room_to_stand(center)) continue;
            if (!chute_hidden_from_player(RVec3(x, floor_y, z))) continue;   // never in his sight
            best = score;
            to = center;
        }
    }
    if (!std::isfinite(best)) return false;
    spawn_body();
    bodies.SetPosition(body_, to, EActivation::Activate);
    bodies.SetLinearAndAngularVelocity(body_, Vec3::sZero(), Vec3::sZero());
    chute_stand_up();
    fall_vy_ = 0.0F;
    feet_y_ = float(to.GetY()) - kStandCentre - 0.02F;
    state_ = State::Searching;
    respawn_pending_ = false;
    floor_gave_way_ = false;
    stood_on_ = BodyID();
    ++respawns_;
    std::fprintf(stderr,
                 "[MATRON SAFETY NET] RESPAWN #%u (%s) from (%.2f, %.2f, %.2f) after %u s waiting: put back at (%.2f, %.2f, %.2f), "
                 "a deck below him, joined to his, out of his sight; player at (%.2f, %.2f, %.2f). This hides a bug: find why she fell.\n",
                 respawns_, respawn_why_, double(from.GetX()), double(from.GetY()), double(from.GetZ()), respawn_waits_,
                 double(to.GetX()), double(to.GetY()), double(to.GetZ()), double(p.GetX()), double(p.GetY()),
                 double(p.GetZ()));
    return true;
}

} // namespace scraperx::sim
