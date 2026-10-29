// The Matron on a hoist (matron.hpp set_hoists): a body on a vertical guide
// that a catch holds, read from the world's kit as the player meets it. When
// no way of her map joins her floor to where she is going, a journey goes by
// a hoist that does: to the stop it waits at, onto its floor, her hands on
// what holds it (a pin shoved clear along its own guide; a trip line's
// hanging handle weighed down), a ride standing on its floor, and off at the
// far stop onto the landing there. Her hands push from her one budget (8 kN,
// 22 kW) and push her back as hard; nothing here sets her position or speed.

#include "sim/matron.hpp"

#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/TransformedShape.h>

#include <algorithm>

#include <cmath>
#include <limits>

namespace scraperx::sim {
using namespace JPH;

namespace {
constexpr float kStopTolM = 0.25F;       // its floor this near a stop's height: at that stop
constexpr float kLandingTolM = 0.35F;    // a landing's top this near the stop's floor
constexpr float kLandingGapM = 1.6F;     // and this near the car's floor, across
constexpr float kBoardMps = 1.0F;
constexpr float kHandPushN = 1500.0F;    // a firm shove: what a pin's shoes hold, and more
constexpr float kPushReachM = 0.35F;     // she stands this short of what she pushes
constexpr float kWorkGiveUpS = 25.0F;
constexpr float kRideStillS = 1.0F;
constexpr float kExitGiveUpS = 20.0F;

float distance_xz(const float ax, const float az, const float bx, const float bz) {
    return std::hypot(ax - bx, az - bz);
}

float xz_gap(const AABox &box, const float x, const float z) {
    const float dx = std::max({box.mMin.GetX() - x, 0.0F, x - box.mMax.GetX()});
    const float dz = std::max({box.mMin.GetZ() - z, 0.0F, z - box.mMax.GetZ()});
    return std::hypot(dx, dz);
}
} // namespace

bool Matron::hoist_footprint(const HoistFeed &h, AABox &box, float &floor_top) const {
    const BodyInterface &bodies = physics_.GetBodyInterface();
    if (h.car.IsInvalid() || !bodies.IsAdded(h.car)) {
        return false;
    }
    const TransformedShape shape = bodies.GetTransformedShape(h.car);
    box = shape.GetWorldSpaceBounds();
    const float cx = 0.5F * (box.mMin.GetX() + box.mMax.GetX());
    const float cz = 0.5F * (box.mMin.GetZ() + box.mMax.GetZ());
    // Its floor: the lowest of it, read from below, then its top from just over it.
    RayCastResult up;
    const float below = box.mMin.GetY() - 0.2F;
    if (!shape.CastRay(RRayCast(RVec3(cx, below, cz), Vec3(0.0F, 1.0F, 0.0F)), up)) {
        return false;
    }
    const float bottom = below + up.mFraction;
    RayCastResult down;
    if (!shape.CastRay(RRayCast(RVec3(cx, bottom + 0.5F, cz), Vec3(0.0F, -0.6F, 0.0F)), down)) {
        return false;
    }
    floor_top = bottom + 0.5F - 0.6F * down.mFraction;
    return true;
}

// The landing at a stop: the walkable top of her map at the car floor's
// height nearest its footprint, within a step across; its region, and the
// point of it nearest the car's middle.
int Matron::hoist_landing(const AABox &car, const float floor_top, Vec3 &point) const {
    int best = -1;
    float best_gap = kLandingGapM;
    const float cx = 0.5F * (car.mMin.GetX() + car.mMax.GetX());
    const float cz = 0.5F * (car.mMin.GetZ() + car.mMax.GetZ());
    for (const traversal::Patch &p : scanned_->patches()) {
        if (p.region < 0 || std::abs(p.top - floor_top) > kLandingTolM) {
            continue;
        }
        const float dx = std::max({p.min_x - car.mMax.GetX(), 0.0F, car.mMin.GetX() - p.max_x});
        const float dz = std::max({p.min_z - car.mMax.GetZ(), 0.0F, car.mMin.GetZ() - p.max_z});
        const float gap = std::hypot(dx, dz);
        if (gap < best_gap) {
            best_gap = gap;
            best = p.region;
            point = Vec3(std::clamp(cx, p.min_x, p.max_x), p.top, std::clamp(cz, p.min_z, p.max_z));
        }
    }
    return best;
}

// A hoist that joins `from` to `goal`: waiting at a stop whose landing she can
// walk or climb to, its far stop's landing one she can go on from to the goal.
bool Matron::plan_hoist(const int from, const int goal) {
    if (scanned_ == nullptr || from < 0) {
        return false;
    }
    for (std::size_t i = 0; i < hoists_.size(); ++i) {
        const HoistFeed &h = hoists_[i];
        AABox box;
        float floor_now = 0.0F;
        if (!hoist_footprint(h, box, floor_now) || h.axis_y == 0.0F) {
            continue;
        }
        // Where it waits, and where it goes: from the end it stands at to the other.
        const float span = h.max_travel - h.min_travel;
        float other = 0.0F;
        if (std::abs(h.travel - h.min_travel) * std::abs(h.axis_y) < kStopTolM) {
            other = h.max_travel;
        } else if (std::abs(h.travel - h.max_travel) * std::abs(h.axis_y) < kStopTolM) {
            other = h.min_travel;
        } else {
            continue;   // between its stops: it is moving, or stuck
        }
        if (span * std::abs(h.axis_y) < 2.0F) {
            continue;
        }
        const float far_top = floor_now + (other - h.travel) * h.axis_y;
        const AABox far_box(box.mMin + Vec3(0.0F, far_top - floor_now, 0.0F), box.mMax + Vec3(0.0F, far_top - floor_now, 0.0F));
        Vec3 board_at, exit_at;
        const int board = hoist_landing(box, floor_now, board_at);
        const int exit = hoist_landing(far_box, far_top, exit_at);
        if (board < 0 || exit < 0 || board == exit) {
            continue;
        }
        const bool to_board = board == from || !scanned_->plan(from, board).empty();
        const bool to_goal = exit == goal || !scanned_->plan(exit, goal).empty();
        if (!to_board || !to_goal) {
            continue;
        }
        hoist_ = {};
        hoist_path_.clear();
        hoist_.phase = 1;
        hoist_.hoist = static_cast<int>(i);
        hoist_.board = board;
        hoist_.exit = exit;
        hoist_.goal = goal;
        hoist_.board_at = board_at;
        hoist_.exit_at = exit_at;
        hoist_.from_travel = h.travel;
        hoist_.to_travel = other;
        return true;
    }
    return false;
}

void Matron::step_hoist(const float dt) {
    hoist_hand_n_ = 0.0F;
    BodyInterface &bodies = physics_.GetBodyInterface();
    if (hoist_.hoist < 0 || hoist_.hoist >= static_cast<int>(hoists_.size())) {
        hoist_.phase = 0;
        travel_.active = false;
        return;
    }
    const HoistFeed &h = hoists_[static_cast<std::size_t>(hoist_.hoist)];
    AABox box;
    float floor_top = 0.0F;
    if (!hoist_footprint(h, box, floor_top)) {
        hoist_.phase = 0;
        travel_.active = false;
        return;
    }
    hoist_.t += dt;
    const RVec3 at = bodies.GetPosition(body_);
    const Vec3 feet(float(at.GetX()), float(at.GetY()) - kStandCentre, float(at.GetZ()));
    const float cx = 0.5F * (box.mMin.GetX() + box.mMax.GetX());
    const float cz = 0.5F * (box.mMin.GetZ() + box.mMax.GetZ());
    BodyID under;
    float top = 0.0F;
    const bool on_car = climber_->supported(&top, &under) && under == h.car;
    const float car_vy = bodies.GetLinearVelocity(h.car).GetY();
    const auto give_up = [&]() {
        hoist_.phase = 0;
        travel_.active = false;
        climber_->cancel();
    };
    switch (hoist_.phase) {
    case 2: {   // over her map's tops to the landing's edge, then onto its floor, to its middle
        const bool at_edge = distance_xz(feet.GetX(), feet.GetZ(), hoist_.board_at.GetX(), hoist_.board_at.GetZ()) < 0.4F;
        hoist_.still_s = at_edge || on_car ? 1.0F : hoist_.still_s;
        if (hoist_.still_s > 0.0F) {
            (void)climber_->walk_between(dt, Vec3(cx, floor_top, cz), kBoardMps, 0.3F);
        } else {
            if (hoist_path_.empty()) {
                hoist_path_ = traversal::grid_walk(physics_, traversal::Ignore{body_, player_, static_cast<int>(moving_layer_)},
                                                   climber_->limits(), feet, hoist_.board_at, 6.0F, 0.1F, 150000);
                if (hoist_path_.empty()) hoist_path_ = scanned_->walk_path(feet, hoist_.board_at);
                if (hoist_path_.empty()) hoist_path_ = {hoist_.board_at};
                hoist_path_i_ = 0;
            }
            while (hoist_path_i_ + 1 < hoist_path_.size() &&
                   distance_xz(feet.GetX(), feet.GetZ(), hoist_path_[hoist_path_i_].GetX(), hoist_path_[hoist_path_i_].GetZ()) < 0.35F) {
                ++hoist_path_i_;
            }
            (void)climber_->walk_between(dt, hoist_path_[hoist_path_i_], kBoardMps, 0.3F);
        }
        if (on_car && distance_xz(feet.GetX(), feet.GetZ(), cx, cz) < 0.5F) {
            hoist_.phase = 3;
            hoist_.t = 0.0F;
        } else if (hoist_.t > kWorkGiveUpS) {
            give_up();
        }
        return;
    }
    case 3: {   // what holds it, freed by her hands
        if (!on_car) {
            give_up();
            return;
        }
        if (!h.held || std::abs(car_vy) > 0.15F) {
            hoist_.phase = 4;
            hoist_.t = 0.0F;
            hoist_.still_s = 0.0F;
            return;
        }
        // A pin not yet clear: she stands behind it, on its far side from
        // where it goes, and shoves it along its guide.
        for (const HoistFeed::Pin &pin : h.pins) {
            if (pin.travel >= pin.clear + 0.03F || pin.body.IsInvalid()) {
                continue;
            }
            const AABox pb = bodies.GetTransformedShape(pin.body).GetWorldSpaceBounds();
            Vec3 along(pin.axis.GetX(), 0.0F, pin.axis.GetZ());
            if (along.LengthSq() < 0.25F) {
                continue;
            }
            along = along.Normalized();
            const Vec3 mid = pb.GetCenter();
            const float half = std::abs(pb.GetExtent().GetX() * along.GetX()) + std::abs(pb.GetExtent().GetZ() * along.GetZ());
            const Vec3 stand = mid - along * (half + kRadius + kPushReachM);
            const Vec3 goal(std::clamp(stand.GetX(), box.mMin.GetX() + kRadius, box.mMax.GetX() - kRadius), floor_top,
                            std::clamp(stand.GetZ(), box.mMin.GetZ() + kRadius, box.mMax.GetZ() - kRadius));
            const float used = climber_->walk_between(dt, goal, kBoardMps, 0.0F);
            // Her hands reach it from where she stands (her one arm reach,
            // Matron::kArmReachM), as near as the car lets her get.
            if (xz_gap(pb, feet.GetX(), feet.GetZ()) < kRadius + kArmReachM) {
                const traversal::Limits budget;
                const float v = std::max(0.3F, bodies.GetLinearVelocity(pin.body).Dot(along));
                const float force = std::min({kHandPushN, budget.total_force_n - used, budget.power_w / v});
                if (force > 0.0F) {
                    const Vec3 push = along * force;
                    bodies.AddForce(pin.body, push, EActivation::Activate);
                    bodies.AddForce(body_, -push, EActivation::Activate);
                    hoist_hand_n_ = force;
                }
            }
            if (hoist_.t > kWorkGiveUpS) {
                give_up();
            }
            return;
        }
        // A trip line's handle: she stands by it and bears down on it with
        // her hands, no harder than her own weight leaves her standing.
        for (const BodyID handle : h.weights) {
            if (handle.IsInvalid() || !bodies.IsAdded(handle)) {
                continue;
            }
            const AABox hb = bodies.GetTransformedShape(handle).GetWorldSpaceBounds();
            const Vec3 mid = hb.GetCenter();
            Vec3 in(cx - mid.GetX(), 0.0F, cz - mid.GetZ());
            if (in.LengthSq() < 1.0e-4F) {
                in = Vec3(0.0F, 0.0F, 1.0F);
            }
            in = in.Normalized();
            const float reach = kRadius + kPushReachM + 0.5F * std::max(hb.GetSize().GetX(), hb.GetSize().GetZ());
            const Vec3 goal(std::clamp(mid.GetX() + in.GetX() * reach, box.mMin.GetX() + kRadius, box.mMax.GetX() - kRadius),
                            floor_top,
                            std::clamp(mid.GetZ() + in.GetZ() * reach, box.mMin.GetZ() + kRadius, box.mMax.GetZ() - kRadius));
            const float used = climber_->walk_between(dt, goal, kBoardMps, 0.0F);
            if (distance_xz(feet.GetX(), feet.GetZ(), goal.GetX(), goal.GetZ()) < 0.35F &&
                hb.mMax.GetY() < feet.GetY() + 1.6F) {
                const traversal::Limits budget;
                const float v = std::max(0.3F, -bodies.GetLinearVelocity(handle).GetY());
                const float force = std::min({0.5F * kMassKg * 9.81F, budget.total_force_n - used, budget.power_w / v});
                if (force > 0.0F) {
                    bodies.AddForce(handle, Vec3(0.0F, -force, 0.0F), EActivation::Activate);
                    bodies.AddForce(body_, Vec3(0.0F, force, 0.0F), EActivation::Activate);
                    hoist_hand_n_ = force;
                }
            }
            if (hoist_.t > kWorkGiveUpS) {
                give_up();
            }
            return;
        }
        // Held, and nothing of it hers to free.
        if (hoist_.t > 2.0F) {
            give_up();
        }
        (void)climber_->stand(dt);
        return;
    }
    case 4: {   // the ride: standing on its floor, in its middle
        (void)climber_->walk_between(dt, Vec3(cx, floor_top, cz), 0.3F, 0.0F);
        const bool arrived = std::abs(h.travel - hoist_.to_travel) * std::abs(h.axis_y) < 0.6F;
        hoist_.still_s = std::abs(car_vy) < 0.05F ? hoist_.still_s + dt : 0.0F;
        if (arrived && hoist_.still_s >= kRideStillS) {
            hoist_.phase = 5;
            hoist_.t = 0.0F;
        } else if (!on_car && hoist_.t > 1.0F) {
            give_up();
        } else if (hoist_.still_s > 10.0F) {
            give_up();   // stopped short, and not going on
        }
        return;
    }
    case 5: {   // off onto the landing, a step past its edge
        Vec3 out(hoist_.exit_at.GetX() - cx, 0.0F, hoist_.exit_at.GetZ() - cz);
        out = out.LengthSq() > 1.0e-4F ? out.Normalized() : Vec3(0.0F, 0.0F, 1.0F);
        (void)climber_->walk_between(dt, hoist_.exit_at + out * 0.8F, kBoardMps, 0.2F);
        if (region_under() == hoist_.exit) {
            const int goal = hoist_.goal;
            hoist_.phase = 0;
            ++hoist_rides_;
            hoist_inner_ = true;
            const bool going = goal == hoist_.exit ? false : travel_to(goal);
            hoist_inner_ = false;
            if (!going) {
                travel_.region = region_under();
                travel_.goal = goal;
                travel_.step = travel_.steps = 0;
                travel_wait_s_ = 0.0F;
                travel_.active = goal == region_under();
                if (!travel_.active) {
                    climber_->cancel();
                }
            }
        } else if (hoist_.t > kExitGiveUpS) {
            give_up();
        }
        return;
    }
    default:
        return;
    }
}

} // namespace scraperx::sim
