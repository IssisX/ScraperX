// The Matron's journeys over the tower's steel (matron.hpp, travel_to): a
// plan over the scanned map (matron_traversal.hpp), walked by her own legs
// between moves and climbed by her own hands, each move physical. A region is
// hers only when she stands on it; a failed move is a fall, and she plans
// again from where she lands. Nothing here sets her position.
#include "sim/matron.hpp"

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace scraperx::sim {

using namespace JPH;

namespace {
constexpr int kMaxReplans = 6;
constexpr float kLandWaitS = 6.0F;
constexpr float kTwoPi = 6.28318530718F;
constexpr float kTurnRate = 7.0F;   // as matron.cpp

float turn_toward(const float yaw, const float want, const float rate, const float dt) {
    float delta = std::remainder(want - yaw, kTwoPi);
    const float step = rate * dt;
    delta = std::clamp(delta, -step, step);
    return std::remainder(yaw + delta, kTwoPi);
}

float distance_xz(const float ax, const float az, const float bx, const float bz) {
    return std::hypot(ax - bx, az - bz);
}
} // namespace

void Matron::set_routes(const traversal::AffordanceMap *routes) { scanned_ = routes; }

int Matron::region_under() const {
    if (scanned_ == nullptr || !body_in_world_) {
        return -1;
    }
    float top = 0.0F;
    if (!climber_->supported(&top)) {
        return -1;
    }
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    return scanned_->region_at(Vec3(float(at.GetX()), top, float(at.GetZ())));
}

bool Matron::travel_to(const int region) {
    if (scanned_ == nullptr || !body_in_world_ || dead_) {
        return false;
    }
    // Her hands on this body (a respawn gives her a new one).
    climber_ = std::make_unique<traversal::Climber>(physics_, body_, traversal::Ignore{body_, player_, static_cast<int>(moving_layer_)});
    const int here = region_under();
    if (here < 0) {
        return false;
    }
    travel_avoid_.clear();
    travel_edges_ = scanned_->plan(here, region, travel_avoid_);
    if (travel_edges_.empty() && here != region) {
        return false;
    }
    travel_.active = here != region;
    travel_.goal = region;
    travel_.region = here;
    travel_.step = 0;
    travel_.steps = static_cast<int>(travel_edges_.size());
    travel_walk_.clear();
    travel_walk_i_ = 0;
    travel_wait_s_ = 0.0F;
    climber_->cancel();
    drop_stock();
    return true;
}

void Matron::step_travel(const float dt) {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    speed_ = 0.0F;
    // The player on a climb she means to take (on it, at its foot, or at
    // its head): the way is theirs. Any climb, from its own line and span.
    const auto player_on = [&](const traversal::MoveSpec &spec) {
        if (player_.IsInvalid() || !bodies.IsAdded(player_)) {
            return false;
        }
        const RVec3 p = bodies.GetPosition(player_);
        const float lo = std::min(spec.bottom_y, spec.start.GetY()) - 2.5F;
        const float hi = std::max(spec.top_y, spec.goal_top) + 0.5F;
        return distance_xz(float(p.GetX()), float(p.GetZ()), spec.line.GetX(), spec.line.GetZ()) < kYieldReachM &&
               float(p.GetY()) > lo && float(p.GetY()) < hi;
    };
    // A move not yet off her floor gives way the same as one not begun.
    if (climber_->status() == traversal::Climber::Status::Running && climber_->phase() == 0 &&
        travel_.step < travel_.steps &&
        player_on(scanned_->edges()[static_cast<std::size_t>(travel_edges_[static_cast<std::size_t>(travel_.step)])].spec)) {
        climber_->cancel();
    }
    // A move under way: her hands and feet on it, nothing else.
    if (climber_->status() == traversal::Climber::Status::Running) {
        climber_->step(dt);
        travel_.move = climber_->view().kind;
        const Vec3 v = bodies.GetLinearVelocity(body_);
        if (std::abs(v.GetX()) + std::abs(v.GetZ()) > 0.2F) {
            yaw_ = turn_toward(yaw_, std::atan2(v.GetX(), v.GetZ()), kTurnRate, dt);
        }
        stride_ += v.Length() * dt;
        if (climber_->status() == traversal::Climber::Status::Done) {
            ++travel_.moves_done;
            // Hers only on its steel: the region under her feet now.
            const int under = region_under();
            const traversal::Edge &e = scanned_->edges()[static_cast<std::size_t>(travel_edges_[static_cast<std::size_t>(travel_.step)])];
            if (under == e.to) {
                travel_.region = under;
                ++travel_.step;
            } else {
                // Somewhere else (or not yet on anything): plan from where she is.
                travel_.step = travel_.steps;   // forces a replan below
            }
            climber_->cancel();
        } else if (climber_->status() == traversal::Climber::Status::Failed) {
            ++travel_.moves_failed;
            travel_.last_failure = climber_->failure();
            // Failed before she left her floor (could not reach its start, no
            // hold there): that way is closed to this journey; a physical
            // failure on the way is not (it may go next time).
            if (climber_->phase() == 0 && travel_.step < travel_.steps) {
                travel_avoid_.push_back(travel_edges_[static_cast<std::size_t>(travel_.step)]);
            }
            travel_.step = travel_.steps;
            travel_wait_s_ = 0.0F;
            climber_->cancel();
        }
        return;
    }
    // Arrived?
    const int under = region_under();
    if (under == travel_.goal) {
        ++travel_.arrivals;
        travel_.active = false;
        travel_.region = under;
        climber_->walk_between(dt, Vec3(float(at.GetX()), float(at.GetY()), float(at.GetZ())), 0.0F);
        return;
    }
    // Off the plan (a fall, a move that ended elsewhere, knocked off her
    // floor partway along a walk): when she is on her feet again, plan from
    // there.
    if (travel_.step >= travel_.steps || (under >= 0 && under != travel_.region)) {
        if (under < 0) {
            travel_wait_s_ += dt;
            if (travel_wait_s_ > kLandWaitS) {
                travel_.active = false;   // nothing under her she knows: she gives it up
            }
            return;
        }
        if (travel_.replans >= kMaxReplans) {
            travel_.active = false;
            return;
        }
        ++travel_.replans;
        travel_edges_ = scanned_->plan(under, travel_.goal, travel_avoid_);
        travel_.region = under;
        travel_.step = 0;
        travel_.steps = static_cast<int>(travel_edges_.size());
        travel_walk_.clear();
        travel_walk_i_ = 0;
        if (travel_edges_.empty()) {
            travel_.active = false;
        }
        return;
    }
    // Walk her region to the next move's start, then begin it.
    const traversal::Edge &e = scanned_->edges()[static_cast<std::size_t>(travel_edges_[static_cast<std::size_t>(travel_.step)])];
    const Vec3 feet(float(at.GetX()), float(at.GetY()) - kStandCentre, float(at.GetZ()));
    // Giving way: off the climb's foot (her body is not to stand in the
    // player's way onto it) and still until it is clear. Her yielding is
    // her own feet; the player's climbing is not touched.
    if (player_on(e.spec)) {
        ++travel_.yields;
        const Vec3 line(e.spec.line.GetX(), feet.GetY(), e.spec.line.GetZ());
        Vec3 away(feet.GetX() - line.GetX(), 0.0F, feet.GetZ() - line.GetZ());
        const float off = away.Length();
        away = off > 1.0e-3F ? away / off : e.spec.out;
        if (off < kYieldClearM) {
            climber_->walk_between(dt, line + away * (kYieldClearM + 0.2F), 1.0F);
        } else {
            climber_->walk_between(dt, feet, 0.0F);
        }
        travel_walk_.clear();
        return;
    }
    const float to_start = distance_xz(feet.GetX(), feet.GetZ(), e.spec.start.GetX(), e.spec.start.GetZ());
    // Over the scanned map's walkable tops only (no authored ways): patch to
    // patch to the move's start, handing over to the move's own feet once
    // the rest of the way is floor all along and close.
    if (travel_walk_.empty()) {
        // Her feet's own search over the floor first; the map's patch links
        // for a long way (or where the search finds nothing).
        if (to_start < 40.0F) {
            travel_walk_ = traversal::grid_walk(physics_, traversal::Ignore{body_, player_, static_cast<int>(moving_layer_)}, climber_->limits(), feet,
                                                 e.spec.start);
        }
        if (travel_walk_.empty()) {
            travel_walk_ = scanned_->walk_path(feet, e.spec.start);
        }
        travel_walk_i_ = 0;
    }
    // A waypoint is passed when she is at it, or when her feet will take her
    // no nearer (an edge her feet keep her from): on to the next.
    {
        const float to_wp = distance_xz(feet.GetX(), feet.GetZ(), travel_walk_[travel_walk_i_].GetX(),
                                        travel_walk_[travel_walk_i_].GetZ());
        if (to_wp < travel_wp_best_ - 0.05F) {
            travel_wp_best_ = to_wp;
            travel_wp_stall_s_ = 0.0F;
        } else {
            travel_wp_stall_s_ += dt;
        }
        if (travel_wp_stall_s_ > 1.5F && to_wp > 1.0F && !travel_rerouted_) {
            // Something on the floor in her way: her feet's search round it.
            std::vector<Vec3> round = traversal::grid_walk(physics_, traversal::Ignore{body_, player_, static_cast<int>(moving_layer_)},
                                                            climber_->limits(), feet, travel_walk_[travel_walk_i_], 3.0F);
            travel_rerouted_ = true;
            if (!round.empty()) {
                travel_walk_.erase(travel_walk_.begin(), travel_walk_.begin() + static_cast<long>(travel_walk_i_));
                travel_walk_.insert(travel_walk_.begin(), round.begin(), round.end() - 1);
                travel_walk_i_ = 0;
                travel_wp_best_ = 1.0e9F;
                travel_wp_stall_s_ = 0.0F;
            }
        }
        if (travel_walk_i_ + 1 < travel_walk_.size() && (to_wp < 0.35F || (to_wp < 1.5F && travel_wp_stall_s_ > 0.8F))) {
            travel_rerouted_ = false;
            ++travel_walk_i_;
            travel_wp_best_ = 1.0e9F;
            travel_wp_stall_s_ = 0.0F;
        }
    }
    const bool last_leg = travel_walk_i_ + 1 >= travel_walk_.size();
    if (!(last_leg && to_start < 1.6F)) {
        const Vec3 w = travel_walk_[travel_walk_i_];
        const float off = distance_xz(feet.GetX(), feet.GetZ(), w.GetX(), w.GetZ());
        // Her own capped feet (the one budget), keeping to floor.
        climber_->walk_between(dt, w, std::min(2.4F, 0.6F + 1.5F * off));
        if (off > 0.15F) {
            yaw_ = turn_toward(yaw_, std::atan2(w.GetX() - feet.GetX(), w.GetZ() - feet.GetZ()), kTurnRate, dt);
        }
        return;
    }
    travel_walk_.clear();
    travel_.move = e.kind;
    climber_->begin(e.spec);
}

} // namespace scraperx::sim
