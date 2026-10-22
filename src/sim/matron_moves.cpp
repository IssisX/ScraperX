// The Matron's pace, her getting unstuck, the player's trail she walks, and
// her silence for theirs (matron.hpp, "Her paces", "Getting unstuck", "The
// player's way"). Members of Matron; walk_toward and pre_step call these.
// Every choice here is simulation state and a seeded roll: deterministic.

#include "sim/matron.hpp"

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>

#include <algorithm>
#include <cmath>

namespace scraperx::sim {
namespace {
using namespace JPH;
float xz(const float ax, const float az, const float bx, const float bz) { return std::hypot(ax - bx, az - bz); }
} // namespace

// xorshift32: the same rolls every run from the same start (the seed is
// put back at every restore).
float Matron::roll() noexcept {
    rng_ ^= rng_ << 13U;
    rng_ ^= rng_ >> 17U;
    rng_ ^= rng_ << 5U;
    return static_cast<float>(rng_ >> 8U) / static_cast<float>(1U << 24U);
}

Matron::Pace Matron::choose_pace(const float dt, const float distance, const bool saw, const bool heard,
                                 const float loudness) {
    hunt_s_ += dt;
    if (fixed_pace_ >= 0) {
        pace_ = static_cast<Pace>(std::clamp(fixed_pace_, 0, 3));
        return pace_;
    }
    pace_hold_s_ -= dt;
    const bool close_sight = saw && distance <= kPaceCloseM;
    // Sight won or lost is a reason to think again now.
    if (saw != pace_saw_) {
        pace_hold_s_ = std::min(pace_hold_s_, 0.25F);
        pace_saw_ = saw;
    }
    // A loud noise is a reason too (a run, a landing reels her in).
    if (heard && loudness >= 0.35F && pace_ != Pace::Hunt) {
        pace_hold_s_ = std::min(pace_hold_s_, 0.1F);
    }
    if (pace_hold_s_ > 0.0F) {
        // Never slower than a stride with the player in plain sight close by.
        if (close_sight && pace_ < Pace::Stride) {
            pace_ = Pace::Stride;
            ++pace_changes_;
        }
        return pace_;
    }
    int tier = 1;
    if (saw && distance <= kSightM) {
        tier = 3;
    } else if (saw) {
        tier = 2;
    } else if (heard && loudness >= 0.35F) {
        tier = 3;   // a run, a landing: the noise reels her in at her fastest
    } else if (heard && loudness >= 0.15F) {
        tier = 2;
    } else if (heard) {
        tier = 1;
    } else {
        // Nothing of the player this tick: after the lead, faster the farther
        // it is (she does not let a lead go cold at a creep).
        tier = distance > 15.0F ? 2 : 1;
    }
    if (stage_ == Stage::Changed) {
        ++tier;
    }
    if (hunt_s_ < kPaceWarmS && !close_sight) {
        --tier;   // slow first
    }
    const float r = roll();
    if (r < kPaceSnapUp) {
        tier = 3;   // snapping up, out of nowhere
    } else if (r < kPaceSnapUp + kPaceDropOff && !close_sight) {
        tier = 0;   // dropping back to a creep, for no reason the player can see
    } else if (r < kPaceSnapUp + kPaceDropOff + kPaceWander) {
        tier += roll() < 0.5F ? -1 : 1;
    }
    tier = std::clamp(tier, 0, 3);
    if (close_sight) {
        tier = std::max(tier, 2);
    }
    const Pace next = static_cast<Pace>(tier);
    if (next != pace_) {
        ++pace_changes_;
    }
    pace_ = next;
    pace_hold_s_ = kPaceHoldMinS + kPaceHoldSpanS * roll();
    return pace_;
}

void Matron::drop_crumb(const float px, const float pz, const float floor_y) {
    if (crumb_count_ > 0) {
        const Crumb &last = crumbs_[static_cast<std::size_t>((crumb_head_ + kCrumbs - 1) % kCrumbs)];
        if (std::abs(last.floor_y - floor_y) < 1.0F && xz(last.x, last.z, px, pz) < kCrumbEveryM) {
            return;
        }
    }
    crumbs_[static_cast<std::size_t>(crumb_head_)] = {px, pz, floor_y};
    crumb_head_ = (crumb_head_ + 1) % kCrumbs;
    crumb_count_ = std::min(crumb_count_ + 1, kCrumbs);
    if (crumb_on_ >= 0) {
        ++crumb_on_;   // everything she is walking to is one older now
        if (crumb_on_ >= crumb_count_) {
            crumb_on_ = -1;
        }
    }
}

// The trail: the crumb (on her level) nearest her within kCrumbMeetM, then
// on, newer and newer, as she reaches each. False with no trail to walk.
bool Matron::crumb_ahead(const float ax, const float az, float &wx, float &wz) {
    const float floor_y = levels_[static_cast<std::size_t>(level_)].floor_y;
    const auto crumb = [&](const int age) -> const Crumb & {
        return crumbs_[static_cast<std::size_t>((crumb_head_ + kCrumbs - 1 - age) % kCrumbs)];
    };
    if (crumb_on_ < 0) {
        float best = kCrumbMeetM;
        for (int age = 0; age < crumb_count_; ++age) {
            const Crumb &c = crumb(age);
            const float d = xz(ax, az, c.x, c.z);
            if (std::abs(c.floor_y - floor_y) < 1.0F && d < best) {
                best = d;
                crumb_on_ = age;
            }
        }
    }
    while (crumb_on_ >= 0) {
        const Crumb &c = crumb(crumb_on_);
        if (std::abs(c.floor_y - floor_y) >= 1.0F) {
            crumb_on_ = -1;
            return false;
        }
        if (xz(ax, az, c.x, c.z) > 1.0F) {
            wx = c.x;
            wz = c.z;
            return true;
        }
        ++crumbs_walked_;
        --crumb_on_;   // on to the next newer one
    }
    return false;
}

bool Matron::node_blocked(const float x, const float z) const noexcept {
    for (const Blocked &b : blocked_) {
        if (xz(b.x, b.z, x, z) < 0.4F) {
            return true;
        }
    }
    return false;
}

// The stall watchdog, each tick her feet are on a floor: `wants` is a way
// still ahead of her. Each kStallWindowS: progress under kStallMoveM is a
// stall, and the rungs go up; progress is a reset.
void Matron::watch_stall(const float dt, const RVec3 at, const float heading_x, const float heading_z,
                         const bool wants, const float to_x, const float to_z) {
    for (Blocked &b : blocked_) {
        b.s -= dt;
    }
    blocked_.erase(std::remove_if(blocked_.begin(), blocked_.end(), [](const Blocked &b) { return b.s <= 0.0F; }),
                   blocked_.end());
    if (sidestep_s_ > 0.0F) {
        sidestep_s_ -= dt;
    }
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    if (!wants) {
        stall_window_s_ = 0.0F;
        stall_x_ = ax;
        stall_z_ = az;
        stall_s_ = 0.0F;
        stall_rung_ = 0;
        return;
    }
    stall_window_s_ += dt;
    if (stall_window_s_ < kStallWindowS) {
        return;
    }
    const float moved = xz(ax, az, stall_x_, stall_z_);
    stall_window_s_ = 0.0F;
    stall_x_ = ax;
    stall_z_ = az;
    if (moved >= kStallMoveM) {
        stall_rung_ = 0;
        stall_s_ = 0.0F;
        return;
    }
    stall_s_ += kStallWindowS;
    longest_stall_s_ = std::max(longest_stall_s_, stall_s_);
    ++stalls_;
    stall_rung_ = stall_rung_ % 3 + 1;
    if (stall_rung_ == 1) {
        clamber_now_ = true;   // walk_toward: a clamber or a vault, whatever is in her way
        return;
    }
    if (stall_rung_ == 2) {
        // Her steps round what is in her way to where she was making for, over
        // the real floor with her body free on it (traversal grid_walk): the
        // first of them a stride or more off is where she steps.
        {
            const traversal::Ignore ignore{body_, player_, static_cast<int>(moving_layer_)};
            const std::vector<Vec3> steps = traversal::grid_walk(physics_, ignore, traversal::Limits{},
                                                                 Vec3(ax, feet_y_, az), Vec3(to_x, feet_y_, to_z), 4.0F);
            for (const Vec3 &step : steps) {
                if (xz(step.GetX(), step.GetZ(), ax, az) >= 0.35F) {
                    sidestep_x_ = step.GetX();
                    sidestep_z_ = step.GetZ();
                    sidestep_s_ = kSidestepS;
                    ++sidesteps_;
                    return;
                }
            }
        }
        // None: sideways along the face in her way: its normal from a ray at her
        // middle, the tangent toward the side with floor (no edge), turn about.
        const Vec3 heading(heading_x, 0.0F, heading_z);
        Vec3 normal = -heading;
        RayCastResult hit;
        const IgnoreSingleBodyFilter self(body_);
        if (physics_.GetNarrowPhaseQuery().CastRay(RRayCast(at, heading * 1.4F), hit, {}, {}, self)) {
            BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mBodyID);
            if (lock.Succeeded()) {
                const RVec3 point = at + heading * (1.4F * hit.mFraction);
                normal = lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point);
                normal.SetY(0.0F);
                normal = normal.IsNearZero() ? -heading : normal.Normalized();
            }
        }
        Vec3 tangent(-normal.GetZ(), 0.0F, normal.GetX());
        if ((stalls_ & 1U) != 0U) {
            tangent = -tangent;
        }
        // Floor that way? (Her feet's edge guard would stop her anyway; this
        // just picks the side that goes somewhere.)
        const auto floor_that_way = [&](const Vec3 &dir) {
            RayCastResult down;
            const RVec3 from = at + dir * kSidestepM + Vec3(0.0F, 0.3F, 0.0F);
            return physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from, Vec3(0.0F, -(kStandCentre + 1.0F), 0.0F)),
                                                          down, {}, {}, self);
        };
        if (!floor_that_way(tangent) && floor_that_way(-tangent)) {
            tangent = -tangent;
        }
        sidestep_x_ = ax + tangent.GetX() * kSidestepM + normal.GetX() * 0.3F;
        sidestep_z_ = az + tangent.GetZ() * kSidestepM + normal.GetZ() * 0.3F;
        sidestep_s_ = kSidestepS;
        ++sidesteps_;
        return;
    }
    // The third: the node she was making for is no way; round by another.
    if (last_way_node_) {
        blocked_.push_back({last_way_x_, last_way_z_, kRerouteBlockS});
    } else {
        blocked_.push_back({ax + heading_x * 1.2F, az + heading_z * 1.2F, kRerouteBlockS});
    }
    ++reroutes_;
}

// Silence for silence (matron.hpp kHushAfterS): true while she stands
// listening. Spent once per quiet spell; any sound ends it and re-arms it.
bool Matron::hush_step(const float dt, const bool saw, const float distance, const float loudness) {
    if (fixed_pace_ >= 0) {
        return false;
    }
    if (loudness > kQuietLoudness) {
        quiet_s_ = 0.0F;
        hush_spent_ = false;
        if (hush_s_ > 0.0F) {
            hush_s_ = 0.0F;
            ++creaks_;   // she moves again, at the sound
        }
        return false;
    }
    quiet_s_ += dt;
    if (saw || distance < 4.0F) {
        if (hush_s_ > 0.0F) {
            hush_s_ = 0.0F;
            ++creaks_;
        }
        return false;
    }
    if (hush_s_ > 0.0F) {
        hush_s_ -= dt;
        // She listens; what she last heard grows no younger for it (its age
        // is the sound's own, matron.cpp pre_step).
        if (hush_s_ <= 0.0F) {
            ++creaks_;   // a creak: she is moving again
            pace_ = Pace::Creep;
            pace_hold_s_ = kPaceHoldMinS + kPaceHoldSpanS * roll();
            return false;
        }
        return true;
    }
    if (!hush_spent_ && quiet_s_ >= kHushAfterS) {
        hush_spent_ = true;
        hush_s_ = kHushMinS + kHushSpanS * roll();
        return true;
    }
    return false;
}

void Matron::patrol_for_test() {
    if (dead_) {
        return;
    }
    if (!body_in_world_) {
        spawn_body();
    }
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    patrol_only_ = true;
    state_ = State::Shuffling;
    pick_patrol(float(at.GetX()), float(at.GetZ()));
}

} // namespace scraperx::sim
