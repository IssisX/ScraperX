// Her travel across the tower (the owner, 2026-10-21: "I really want her to
// be able to climb through more decks, up and down"). Her map
// (matron_routes.hpp) chooses the ways from the deck she stands on to the one
// her evidence points at; this walks her to each way and along its steps, the
// climbs by her hands (matron.cpp climb()). Started along a way, she finishes
// it before anything else: a new sound does not let go of a rung. A way that
// fails under her is left alone a while, and she plans again from wherever
// she came down; between two decks with no way known she stands and listens.
//
// Her stage decides whether she takes a way: the frail pest stays on her deck
// unless the Turn takes her (as at the ladder's foot); dangerous or changed
// she follows by any way she knows, the corner pipe among them.

#include "sim/matron.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace scraperx::sim {
using namespace JPH;

namespace {
constexpr float kRouteArriveM = 0.3F;     // at a step's point
constexpr float kRouteWalkMps = 1.6F;     // along a way's own steel: a bridge, steps, a duct
constexpr float kRouteStepS = 15.0F;      // a walk step this long without getting nearer got her nowhere
constexpr float kRouteHeadwayM = 0.1F;    // nearer by this is headway
constexpr float kPiF = 3.14159265358979F;
constexpr float kTurnRate = 7.0F;
float distance(const float ax, const float az, const float bx, const float bz) { return std::hypot(ax - bx, az - bz); }
float turn_to(const float yaw, const float want, const float rate, const float dt) {
    float delta = std::remainder(want - yaw, 2.0F * kPiF);
    delta = std::clamp(delta, -rate * dt, rate * dt);
    return std::remainder(yaw + delta, 2.0F * kPiF);
}
} // namespace

int Matron::climb_count() const noexcept {
    return scanned_ == nullptr ? 0 : static_cast<int>(scanned_->edges().size());
}

// A way up as her own scan of the world found it: a usable rung edge of her
// scanned map, read straight from the scan (no table of hers), faced toward
// its rungs from the side she starts on.
bool Matron::climb_of(const int edge, Climb &out, const Ways ways) const noexcept {
    if (edge < 0 || edge >= climb_count()) {
        return false;
    }
    const traversal::Edge &e = scanned_->edges()[static_cast<std::size_t>(edge)];
    const bool mantle = e.kind == traversal::MoveKind::Mantle && ways == Ways::Any;
    if (!e.usable || (e.kind != traversal::MoveKind::RungUp && !mantle)) {
        return false;
    }
    const auto &regions = scanned_->regions();
    const traversal::MoveSpec &m = e.spec;
    const Vec3 face = -m.out;
    // A lip from a floor is a way up for her hands as rungs are: the scan's line is her standing spot, so the lip is
    // midway between it and the top she steps onto.
    const float line_x = mantle ? 0.5F * (m.start.GetX() + m.goal.GetX()) : m.line.GetX();
    const float line_z = mantle ? 0.5F * (m.start.GetZ() + m.goal.GetZ()) : m.line.GetZ();
    const float head = regions[static_cast<std::size_t>(e.to)].top;
    // She hangs against the rungs, not where she stood at the foot: her radius and a hand's clearance off the line.
    const float hang = kRadius + 0.05F;
    // The lip is the scan's own head patch, not the region's average: a bridge or fascia can stand above its deck.
    out = {line_x - face.GetX() * hang, line_z - face.GetZ() * hang, m.start.GetY(), head, face.GetX(), face.GetZ(),
           m.goal.GetX(), m.goal.GetZ(), true, hang, m.goal_top};
    return true;
}

Matron::Climb Matron::climb_spec(const int edge) const noexcept {
    Climb c{};
    (void)climb_of(edge, c);
    return c;
}

// Her nearest scanned way off this floor: up from its foot, or down from its
// top. -1 when her scan found none.
int Matron::nearest_climb(const float x, const float z, const float floor_y, const bool down) const noexcept {
    int best = -1;
    float best_d = std::numeric_limits<float>::max();
    for (int i = 0; i < climb_count(); ++i) {
        Climb c{};
        if (!climb_of(i, c)) {
            continue;
        }
        // A climb's top floor can be its deck or the bridge over it.
        const float gap = down ? std::min(std::abs(c.head_y - floor_y), std::abs(c.lip_y - floor_y))
                               : std::abs(c.foot_y - floor_y);
        if (gap > 0.6F) {
            continue;
        }
        const float d = down ? std::hypot(x - c.top_x, z - c.top_z) : std::hypot(x - c.x, z - c.z);
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

// Out of her own trap: the scanned way up nearest her, off the floor she
// stands on.
int Matron::bay_way_out() const noexcept {
    if (!body_in_world_) {
        return -1;
    }
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    return nearest_climb(float(at.GetX()), float(at.GetZ()), float(at.GetY()) - kStandCentre, false);
}

routes::Connector Matron::connector_of(const int) const noexcept { return routes::Connector::Count; }

std::size_t Matron::pipe_way(const float from_floor, const float to_floor) const noexcept {
    return routes_.find(routes_.region_at(kPipeDeckX, from_floor, kPipeDeckZ),
                        routes_.region_at(kPipeDeckX, to_floor, kPipeDeckZ),
                        to_floor < from_floor ? routes::Kind::PipeDown : routes::Kind::PipeUp);
}

routes::Region Matron::lead_region() const noexcept {
    // The deck under her lead: where she placed it, on the floor she placed it.
    return routes_.region_at(heard_x_, lead_floor_, heard_z_);
}

bool Matron::travels() const noexcept { return turn_ != Turn::None || stage_ != Stage::Pest; }

void Matron::route_clear() noexcept {
    route_ = {};
    route_at_ = 0;
    route_step_ = 0;
    route_step_s_ = 0.0F;
    route_step_best_ = std::numeric_limits<float>::max();
    route_on_edge_ = false;
    route_goal_ = routes::Region::Count;
    climb_end_ = ClimbEnd::None;
}

void Matron::route_failed(const std::size_t edge, const routes::Reason reason) {
    routes_.fail(edge, reason, clock_s_);
    ++route_failures_;
    route_clear();
}

bool Matron::follow_route(const float dt, const RVec3 at) {
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const float accel = turn_ != Turn::None ? kTurnFootAccel : kFootAccel;
    if (!route_on_edge_) {
        const routes::Region here = routes_.region_at(ax, feet_y_, az);
        const routes::Region goal = lead_region();
        if (here == routes::Region::Count || goal == routes::Region::Count || here == goal || !travels()) {
            route_clear();
            return false;
        }
        if (route_goal_ != goal || route_at_ >= route_.count ||
            routes_.edge(route_.edges[route_at_]).from != here) {
            if (!routes_.plan(here, ax, az, goal, clock_s_, kHuntMps, route_)) {
                route_clear();
                return false;
            }
            route_goal_ = goal;
            route_at_ = 0;
        }
        // To the way along her lines; there, she starts along it.
        const routes::Edge &e = routes_.edge(route_.edges[route_at_]);
        const float off = distance(ax, az, e.entry_x, e.entry_z);
        if (off > kRouteArriveM) {
            float wx = e.entry_x;
            float wz = e.entry_z;
            route_toward(ax, az, e.entry_x, e.entry_z, false, wx, wz);
            walk_toward(dt, wx, wz, std::min(kHuntMps, 1.0F + 3.0F * off), accel);
            return true;
        }
        route_on_edge_ = true;
        route_step_ = 0;
        route_step_s_ = 0.0F;
        route_step_best_ = std::numeric_limits<float>::max();
        climb_end_ = ClimbEnd::None;
    }
    const std::size_t index = route_.edges[route_at_];
    const routes::Edge &e = routes_.edge(index);
    if (route_step_ >= e.step_count) {
        // Along it and off it, on the deck it leads to.
        routes_.succeed(index);
        ++route_edges_done_;
        route_on_edge_ = false;
        ++route_at_;
        walk_toward(dt, ax, az, 0.0F, accel);
        return true;
    }
    const routes::Step &step = e.steps[route_step_];
    route_step_s_ += dt;
    if (step.what == routes::Step::Do::Walk) {
        const float off = distance(ax, az, step.x, step.z);
        if (off <= kRouteArriveM) {
            ++route_step_;
            route_step_s_ = 0.0F;
            route_step_best_ = std::numeric_limits<float>::max();
            walk_toward(dt, ax, az, 0.0F, accel);
            return true;
        }
        if (off < route_step_best_ - kRouteHeadwayM) {
            route_step_best_ = off;
            route_step_s_ = 0.0F;
        }
        if (route_step_s_ > kRouteStepS) {
            route_failed(index, routes::Reason::Failed);
            return false;
        }
        walk_toward(dt, step.x, step.z, std::min(kRouteWalkMps, 0.5F + 1.5F * off), accel);
        return true;
    }
    // A climb: its end (climb() lets go with her feet on its far support, or
    // fails), or its start.
    if (climb_end_ == ClimbEnd::Done) {
        climb_end_ = ClimbEnd::None;
        ++route_step_;
        route_step_s_ = 0.0F;
        route_step_best_ = std::numeric_limits<float>::max();
        return true;
    }
    if (climb_end_ == ClimbEnd::Failed) {
        route_failed(index, routes::Reason::Failed);
        return false;
    }
    if (step.climb == routes::Connector::CornerPipe) {
        // At the north rail, facing it; her hands on it, and over (matron_pipe.cpp).
        walk_toward(dt, ax, az, 0.0F, accel);
        yaw_ = turn_to(yaw_, kPiF, kTurnRate, dt);
        if (!travels() || climb_retry_s_ > 0.0F) {
            climb_retry_s_ = std::max(0.0F, climb_retry_s_ - dt);
            return true;
        }
        climb_end_ = ClimbEnd::None;
        const float floor_y = levels_[static_cast<std::size_t>(level_)].floor_y;
        const float to = floor_y + (step.what == routes::Step::Do::ClimbDown ? -kStackPitch : kStackPitch);
        if (!pipe_reaches(floor_y) || !pipe_reaches(to) || !get_on_pipe(floor_y, to)) {
            route_failed(index, routes::Reason::Failed);
            return false;
        }
        return true;
    }
    // The climb at her feet: the step before stood her at its foot (or, going down, at its top).
    const int climb = nearest_climb(ax, az, float(at.GetY()) - kStandCentre, step.what == routes::Step::Do::ClimbDown);
    if (climb < 0) {
        route_failed(index, routes::Reason::Untested);
        return false;
    }
    const Climb c = climb_spec(climb);
    walk_toward(dt, ax, az, 0.0F, accel);
    yaw_ = turn_to(yaw_, std::atan2(c.out_x, c.out_z), kTurnRate, dt);
    if (!travels() || climb_retry_s_ > 0.0F) {
        climb_retry_s_ = std::max(0.0F, climb_retry_s_ - dt);
        return true;   // at its foot (or its top), looking at it
    }
    climb_end_ = ClimbEnd::None;
    if (column_blocked_near(c.x, c.z) || !begin_climb(climb, step.what == routes::Step::Do::ClimbDown)) {
        route_failed(index, routes::Reason::Failed);
        return false;
    }
    return true;
}

} // namespace scraperx::sim
