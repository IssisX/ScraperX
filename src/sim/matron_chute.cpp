// Her parachute (Cory, 2026-10-27): a long fall is not the end of her. Falling
// past kChuteDeployMps with more than kChuteDropM of air under her, she opens
// a canopy like the player's (kParachuteDragCoefficient), steers it to a deck
// her own scan joins to the player's, lands and searches again. Its drag and
// pull are on her one account (8,000 N / 22 kW; Cory 16:01 CT): holding her
// weight takes m*g*v, so it opens early (~7 m/s) and flies at ~9 m/s.
//
// The safety net (last resort, logged loudly): off the world, or a landing that
// would kill her when the fall was not the catwalk's designed kill, she is put
// back about one deck below the player, out of his sight, searching.

#include "sim/matron.hpp"
#include "sim/trapeze.hpp"
#include "sim/physics_world_tuning.hpp"

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>

namespace scraperx::sim {
using namespace JPH;

namespace {
constexpr float kGlideRatio = 0.12F;    // horizontal metres per metre of drop she makes good (measured: the
                                        // 22 kW account leaves ~1 m/s^2 of pull at ~9 m/s)
constexpr float kChuteFlyMps = 4.0F;    // horizontal speed she steers toward
constexpr float kLostSeconds = 20.0F;   // open this long without landing: the net
constexpr float kRespawnBelowMinM = 3.0F, kRespawnBelowMaxM = 16.0F;
constexpr float kRespawnNearM = 8.0F, kRespawnFarM = 40.0F;

int chute_region_under(const traversal::AffordanceMap &map, const RVec3 body_centre) {
    for (const float down : {0.9F, 1.0F, 1.2F, 0.6F, 1.5F}) {
        const int r = map.region_at(Vec3(float(body_centre.GetX()), float(body_centre.GetY()) - down,
                                         float(body_centre.GetZ())));
        if (r >= 0) return r;
    }
    return -1;
}
} // namespace

void Matron::step_chute(const float dt) {
    if (!body_in_world_ || dead_) {
        chute_open_ = false;
        return;
    }
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    Vec3 v = bodies.GetLinearVelocity(body_);
    // What is under her feet now (and how far the floor is).
    // Her centre and four points on her rim: a ring's edge under one side of
    // her catches her as surely as a deck under her middle.
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
    if (any && drop < 0.3F && std::abs(fall_vy_) < 2.0F && std::abs(v.GetY()) < 2.0F) {   // standing, not landing
        // The catwalk (its rotten plates, and over the shaft beside it) is the
        // game's one designed kill: a fall from it is not flown and not netted.
        bool plank = false;
        for (const Plank &p : planks_) plank = plank || p.body == hit.mBodyID;
        const float feet = float(at.GetY()) - kStandCentre;
        last_on_plank_ = plank || (std::abs(feet - kDeckTop) < 1.0F &&
                                   on_catwalk_line(float(at.GetX()), float(at.GetZ()), 1.0F));
    }
    const Mover *way = holder();
    if (riding_ || holding_state(state_) || (way != nullptr && (way->view().held || way->view().lands))) {
        chute_open_ = false;
        return;
    }
    if (!chute_open_) {
        if (-v.GetY() < kChuteDeployMps || drop <= kChuteDropM || last_on_plank_) return;
        chute_open_ = true;
        chute_s_ = 0.0F;
        ++chutes_;
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
    const float speed = v.Length();
    const traversal::Limits law{};   // her one account (the owner's law), not a number of its own
    const float cap = std::min(law.total_force_n, law.power_w / std::max(speed, 0.1F));
    Vec3 force = Vec3::sZero();
    if (speed > 1.0e-4F) {
        const float drag = std::min(kMassKg * detail::kParachuteDragCoefficient * speed * speed, cap);
        force = -(v / speed) * drag;
    }
    if (chute_has_goal_) {
        Vec3 to(chute_x_ - float(at.GetX()), 0.0F, chute_z_ - float(at.GetZ()));
        const float dist = to.Length();
        const Vec3 want = dist > 0.05F ? to / dist * std::min(kChuteFlyMps, dist) : Vec3::sZero();
        Vec3 dv = want - Vec3(v.GetX(), 0.0F, v.GetZ());
        Vec3 steer = dv * (kMassKg / 0.5F);
        const float most = std::min(kMassKg * kChuteSteerAccel, std::max(0.0F, cap - force.Length()));
        if (steer.Length() > most) steer = steer.Normalized() * most;
        force += steer;
    }
    if (force.Length() > cap) force = force.Normalized() * cap;   // one account: force and power
    v += force / kMassKg * dt;
    bodies.SetLinearVelocity(body_, v);
    if (any && drop < 0.2F && chute_s_ > 0.2F) {
        chute_open_ = false;
        if (!chute_has_goal_) {
            chute_respawn("landed where no deck joins the player's");
            return;
        }
        feet_y_ = float(at.GetY()) - kStandCentre;
        // Her own mind takes it from here (a journey that failed replans from
        // where she landed); the player's place is a fresh lead.
        if (!out(state_) || state_ == State::Shuffling) state_ = State::Searching;
        const RVec3 p = bodies.GetPosition(player_);
        take_lead(float(p.GetX()), float(p.GetZ()), floor_of(float(p.GetY())), 1.0F, false);
        std::fprintf(stderr, "[matron-chute] LANDED at (%.2f, %.2f, %.2f) after %.1f s; searching\n",
                     double(at.GetX()), double(feet_y_), double(at.GetZ()), double(chute_s_));
        return;
    }
    if (chute_s_ > kLostSeconds) {
        chute_open_ = false;
        chute_respawn("chute open too long without a landing");
    }
}

bool Matron::chute_pick(const RVec3 at) {
    if (scanned_ == nullptr) return false;
    const traversal::AffordanceMap &map = *scanned_;
    const RVec3 p = physics_.GetBodyInterface().GetPosition(player_);
    const int player_region = chute_region_under(map, p);
    std::map<int, bool> joined;
    const auto joins = [&](const int region) {
        if (region < 0 || player_region < 0) return false;
        if (region == player_region) return true;
        const auto it = joined.find(region);
        if (it != joined.end()) return it->second;
        const bool ok = !map.plan(region, player_region).empty();
        joined[region] = ok;
        return ok;
    };
    float best = std::numeric_limits<float>::infinity();
    for (const traversal::Patch &patch : map.patches()) {
        if (patch.narrow || patch.top > float(at.GetY()) - kStandCentre - 1.0F) continue;
        if ((patch.max_x - patch.min_x) < 1.0F || (patch.max_z - patch.min_z) < 1.0F) continue;
        const float x = std::clamp(float(at.GetX()), patch.min_x + 0.5F, patch.max_x - 0.5F);
        const float z = std::clamp(float(at.GetZ()), patch.min_z + 0.5F, patch.max_z - 0.5F);
        const float fall = float(at.GetY()) - kStandCentre - patch.top;
        const float reach = std::hypot(x - float(at.GetX()), z - float(at.GetZ()));
        if (reach > kGlideRatio * fall + 1.0F) continue;
        if (!joins(patch.region)) continue;   // never an island: only decks that join the player's
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

void Matron::chute_respawn(const char *why) {
    const RVec3 p = physics_.GetBodyInterface().GetPosition(player_);
    const RVec3 from = body_in_world_ ? physics_.GetBodyInterface().GetPosition(body_) : RVec3::sZero();
    RVec3 to = RVec3(kLockerX, kDeckTop + kStandCentre + 0.02F, kSpawnZ);
    bool found = false;
    if (scanned_ != nullptr) {
        const traversal::AffordanceMap &map = *scanned_;
        const int player_region = chute_region_under(map, p);
        float best = std::numeric_limits<float>::infinity();
        for (int pass = 0; pass < 2 && !found; ++pass) {
            for (const traversal::Patch &patch : map.patches()) {
                if (patch.narrow || (patch.max_x - patch.min_x) < 1.2F || (patch.max_z - patch.min_z) < 1.2F) continue;
                const float below = float(p.GetY()) - patch.top;
                if (below < kRespawnBelowMinM || below > kRespawnBelowMaxM) continue;
                const float x = 0.5F * (patch.min_x + patch.max_x), z = 0.5F * (patch.min_z + patch.max_z);
                const float d = std::hypot(x - float(p.GetX()), z - float(p.GetZ()));
                if (d < kRespawnNearM || d > kRespawnFarM) continue;
                if (player_region >= 0 && patch.region != player_region &&
                    map.plan(patch.region, player_region).empty()) continue;
                const RVec3 head(x, patch.top + 1.6F, z);
                const RVec3 eye = p + Vec3(0.0F, 0.6F, 0.0F);
                RayCastResult block;
                const RRayCast sight(eye, Vec3(head - eye));
                const bool hidden = physics_.GetNarrowPhaseQuery().CastRay(sight, block, {}, {},
                                                                          IgnoreSingleBodyFilter(player_));
                if (pass == 0 && !hidden) continue;   // out of his sight first; in it only if nowhere else
                const float score = std::abs(below - 11.0F) + 0.1F * d;
                if (score < best) {
                    best = score;
                    to = RVec3(x, patch.top + kStandCentre + 0.02F, z);
                    found = true;
                }
            }
        }
    }
    remove_body();
    spawn_body();
    BodyInterface &bodies = physics_.GetBodyInterface();
    bodies.SetPosition(body_, to, EActivation::Activate);
    bodies.SetLinearAndAngularVelocity(body_, Vec3::sZero(), Vec3::sZero());
    dead_ = false;
    chute_open_ = false;
    lost_s_ = 0.0F;
    fall_vy_ = 0.0F;
    feet_y_ = float(to.GetY()) - kStandCentre - 0.02F;
    state_ = State::Searching;
    take_lead(float(p.GetX()), float(p.GetZ()), floor_of(float(p.GetY())), 1.0F, false);
    ++respawns_;
    std::fprintf(stderr,
                 "[MATRON SAFETY NET] RESPAWN #%u (%s) from (%.2f, %.2f, %.2f): put back at (%.2f, %.2f, %.2f) %s, player at (%.2f, %.2f, %.2f). "
                 "This hides a bug: find why she fell.\n",
                 respawns_, why, double(from.GetX()), double(from.GetY()), double(from.GetZ()), double(to.GetX()), double(to.GetY()), double(to.GetZ()),
                 found ? "a deck below him, joined to his" : "at her locker (no deck found)", double(p.GetX()),
                 double(p.GetY()), double(p.GetZ()));
}

} // namespace scraperx::sim
