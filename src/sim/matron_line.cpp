// Her lines (matron.hpp): down a cable toward a lead below, onto it from its
// stand, the ride, and letting go at its end. A Mover (matron_mover.hpp).
#include "sim/matron.hpp"
#include "sim/matron_grip.hpp"
#include "sim/mechanism_kit.hpp"
#include "sim/zipline.hpp"

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace scraperx::sim {
using namespace JPH;

namespace {
constexpr float kPi = 3.14159265F;
constexpr float kTurnRate = 7.0F;
float turn_toward(const float yaw, const float want, const float rate, const float dt) {
    float delta = std::remainder(want - yaw, 2.0F * kPi);
    const float step = rate * dt;
    delta = std::clamp(delta, -step, step);
    return std::remainder(yaw + delta, 2.0F * kPi);
}
float distance_xz(const float ax, const float az, const float bx, const float bz) {
    return std::hypot(ax - bx, az - bz);
}
// Up a ramp (follow_line) she aims this far ahead along its middle.
constexpr float kRampAheadM = 0.6F;
// On a ramp's strip: this near its middle line (the ramp is 1 m across).
constexpr float kRampStripM = 0.6F;
} // namespace

// A line from her level down toward her lead's (to it, or to a level on the
// way): the one she saw taken, or the nearest. -1 when none does.
int Matron::line_to_take(const float ax, const float az) const {
    if (zipline_ == nullptr) {
        return -1;
    }
    const float floor_y = levels_[static_cast<std::size_t>(level_)].floor_y;
    const std::vector<Zipline::Line> &lines = zipline_->lines();
    int best = -1;
    float best_d = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const Zipline::Line &line = lines[i];
        const float bottom = float(line.bottom_floor.GetY());
        if (std::abs(float(line.top_floor.GetY()) - floor_y) > 0.6F || bottom > floor_y - 0.6F) {
            continue;
        }
        // A line down to a floor below the lead's heard height is no way
        // to a lead standing on that floor -- unless the lead was heard
        // at this line's own stand and the line has been ridden (its
        // trolley down the line): then the lead rode it down, and its
        // bottom is where they went, not an overshoot past them. This
        // is the case a way of her own leaves behind: a journey carries
        // her to the stand after the ride, its early return skipping
        // the ride-guess that would otherwise name the bottom floor,
        // so the lead stays at the stand-top height they were heard at
        // and every line down from her floor reads as below them.
        const bool heard_at_stand =
            distance_xz(heard_x_, heard_z_, float(line.stand.GetX()), float(line.stand.GetZ())) <= 1.0F;
        if (bottom < lead_floor_ - 0.6F && !(heard_at_stand && !line.parked)) {
            continue;
        }
        const float d = static_cast<int>(i) == guess_line_
                            ? -1.0F
                            : distance_xz(ax, az, float(line.stand.GetX()), float(line.stand.GetZ()));
        if (d < best_d) {
            best_d = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

// On the ramp up to the line's stand, and on the stand itself: past its
// foot, on its strip. The top of the ramp counts. Stopping 0.3 m under
// the stand handed her to a found climb, which walked her back down it.
bool Matron::on_line_ramp(const RVec3 at) const {
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const int best = line_to_take(ax, az);
    if (best < 0) {
        return false;
    }
    const Zipline::Line &line = zipline_->lines()[static_cast<std::size_t>(best)];
    const float fx = float(line.ramp_foot.GetX());
    const float fz = float(line.ramp_foot.GetZ());
    const float sx = float(line.stand.GetX());
    const float sz = float(line.stand.GetZ());
    const float rx = sx - fx;
    const float rz = sz - fz;
    const float rlen = std::hypot(rx, rz);
    if (rlen < 0.1F) {
        return false;
    }
    const float along = ((ax - fx) * rx + (az - fz) * rz) / rlen;
    const float across = std::abs((ax - fx) * rz - (az - fz) * rx) / rlen;
    const float feet = float(at.GetY()) - kStandCentre;
    const float floor_y = levels_[static_cast<std::size_t>(level_)].floor_y;
    const bool on_strip = along > 0.0F && along <= rlen + 0.6F && across < kRampStripM;
    const bool on_stand = distance_xz(ax, az, sx, sz) <= 0.85F;
    return feet >= floor_y + 0.15F && feet <= float(line.stand.GetY()) + 0.25F && (on_strip || on_stand);
}

void Matron::notice_ridden_line(const RVec3 player) {
    if (zipline_ == nullptr || !zipline_->riding() || !body_in_world_ || dead_) {
        return;
    }
    const int line = zipline_->state().line;
    if (line < 0 || static_cast<std::size_t>(line) >= zipline_->lines().size()) {
        return;
    }
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    // The same sight the hunt uses. A climb that owns her body used to
    // return before that ran, so a player who took the line in front of
    // her left no lead, and the Turn she had already started sat frozen
    // until the climb let go.
    if (Vec3(player - at).Length() > kSightM || !sees(at + RVec3(0.0, -0.5, 0.0), player - RVec3(0.0, 0.3, 0.0))) {
        return;
    }
    const RVec3 end = zipline_->lines()[static_cast<std::size_t>(line)].bottom_floor;
    heard_x_ = float(end.GetX());
    heard_z_ = float(end.GetZ());
    lead_floor_ = float(end.GetY());
    guess_line_ = line;
    guess_climb_ = -1;
    since_heard_s_ = 0.0F;
    pursuit_s_ = 0.0F;
    lead_seen_ = true;
    lead_valid_ = true;
    wary_ = false;
}

bool Matron::follow_line(const float dt, const RVec3 at, const bool wait_ok) {
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const int best = line_to_take(ax, az);
    if (best < 0) {
        return false;
    }
    const float floor_y = levels_[static_cast<std::size_t>(level_)].floor_y;
    const std::vector<Zipline::Line> &lines = zipline_->lines();
    const Zipline::Line &line = lines[static_cast<std::size_t>(best)];
    // A line she would only wait at (outside her Turn, or one nobody has
    // ridden) is not a way yet: her own climbing goes first, and she comes
    // to its stand only when she finds none.
    if (!wait_ok && (turn_ == Turn::None || line.parked)) {
        return false;
    }
    // Frail, a parked line is refused on its stand (below); refused before
    // the walk there too, or she walks to the stand, refuses it, walks off,
    // and is sent back to it the next tick: a lead a floor or more below
    // held her on the 154 stand for 41 s of 60 (sim_tests_matron_stand.cpp).
    if (line.parked && turn_ == Turn::None) {
        return false;
    }
    const float sx = float(line.stand.GetX());
    const float sz = float(line.stand.GetZ());
    const float stand_y = float(line.stand.GetY());
    const float feet = float(at.GetY()) - kStandCentre;
    // Not on her level at all: a full metre below its floor, she fell off
    // it (a level her settle has not caught up with yet, as after letting
    // go of a cable onto the floor below). Walking to this line's stand
    // from there drives her across the floor she actually landed on and
    // off its edge, chasing a stand on a floor she is no longer on. Say
    // no here; the settle and the hunt's other ways take her from where
    // she is once her level catches up.
    if (feet < floor_y - 1.0F) {
        return false;
    }
    if (feet < stand_y - 0.3F || distance_xz(ax, az, sx, sz) > 0.6F) {
        // To the ramp's foot along her lines, then up the ramp to the stand.
        float wx = sx;
        float wz = sz;
        const float fx = float(line.ramp_foot.GetX());
        const float fz = float(line.ramp_foot.GetZ());
        if (feet < floor_y + 0.15F && distance_xz(ax, az, fx, fz) > 0.5F) {
            route_toward(ax, az, fx, fz, false, wx, wz);
        } else {
            // Up the ramp she keeps to its middle: she aims a little ahead along the line from its foot to
            // the stand, not at the stand from wherever she is (a ramp is narrow, and a diagonal run at the
            // stand walks her off its side).
            const float rx = sx - fx;
            const float rz = sz - fz;
            const float rlen = std::hypot(rx, rz);
            if (rlen > 0.1F) {
                const float along = std::clamp(((ax - fx) * rx + (az - fz) * rz) / rlen, 0.0F, rlen);
                const float ahead = std::min(along + kRampAheadM, rlen);
                wx = fx + rx / rlen * ahead;
                wz = fz + rz / rlen * ahead;
            }
        }
        walk_toward(dt, wx, wz, kHuntMps, turn_ != Turn::None ? kTurnFootAccel : kFootAccel);
        return true;
    }
    // On the stand under the cable. A parked line is not a place to wait
    // while she is frail: the trolley is still at the top, and waiting there
    // held her until the chase clock ran out (a parked 374 stand, 150 s,
    // the lead a floor below) while her other ways went untried. Frail, on
    // a line that has been ridden, she stands and looks down it. She does
    // not jump. In the Turn she jumps whether or not the trolley has left,
    // and takes the bare cable. Refusing the parked line in the Turn too
    // left her standing on the stand (the touch line: Turn, on the stand,
    // never onto the cable).
    if (line.parked && turn_ == Turn::None) {
        return false;
    }
    const RVec3 cable = line.top + line.direction * Zipline::kParkM;
    const RVec3 hands = at + Vec3(0.0F, kHandsAboveCentre, 0.0F);
    const bool in_reach = Vec3(cable - hands).Length() < kCableReachM;
    if (turn_ != Turn::None && !in_reach) {
        // Standing on it: the jump for the cable, a leg stroke of her feet
        // against the stand (walk_toward drives it from her one budget).
        const Vec3 velocity = physics_.GetBodyInterface().GetLinearVelocity(body_);
        if (feet < stand_y + 0.08F && std::abs(velocity.GetY()) < 0.2F) {
            leg_jump_vy_ = kCableJumpMps;
        }
    }
    walk_toward(dt, ax, az, 0.0F);
    yaw_ = turn_toward(yaw_, std::atan2(line.direction.GetX(), line.direction.GetZ()), kTurnRate, dt);
    if (turn_ == Turn::None) {
        return true;
    }
    if (in_reach) {
        zipline_->clip_other(best, body_, Vec3(0.0F, kHandsAboveCentre, 0.0F), true);
        on_line_ = true;
        riding_ = true;
        let_go_s_ = kLetGoSeconds;
        foot_driving_ = false;
        foot_prep_ = false;
        ++cable_slides_;
        return true;
    }
    return true;
}

void Matron::ride_line() {
    if (!on_line_ || zipline_ == nullptr) {
        return;
    }
    const Zipline::State &ride = zipline_->other_state();
    if (!ride.riding) {
        on_line_ = false;   // taken off it (a restore)
        riding_ = false;
        let_go_s_ = 0.0F;
        return;
    }
    // Down at the end (at the stop, or against the trolley or its rider
    // there) and still: she lets go, swinging on over the far band as she
    // does. Never into the shaft: held still where her swing would come down
    // with any of her over it (the buffer's clamp, a trolley left short in
    // it, a rider still hanging at the stop ahead of her), she eases her
    // forearms' drag and comes on as far as the cable lets her.
    BodyInterface &bodies = physics_.GetBodyInterface();
    const Zipline::Line &line = zipline_->lines()[static_cast<std::size_t>(ride.line)];
    const RVec3 at = bodies.GetPosition(body_);
    const bool at_end = ride.along_m > line.length - Zipline::kStopMarginM - Zipline::kBufferM;
    const auto over_shaft = [](const RVec3 p) {
        return std::abs(float(p.GetX())) < kShaftHalfM + kRadius && std::abs(float(p.GetZ()) - kMidZ) < kShaftHalfM + kRadius;
    };
    // Where letting go puts her: on along the line as far as her swing
    // carries her while she drops to the far floor. The box around the
    // shaft is a little smaller than the hole, so a point just outside it
    // can still be open air. She lets go only when that swing comes down
    // on a floor.
    const Vec3 run = Vec3(line.direction.GetX(), 0.0F, line.direction.GetZ()).NormalizedOr(Vec3::sZero());
    const float drop = std::max(0.0F, float(at.GetY() - line.bottom_floor.GetY()) - kStandCentre);
    const float gravity = -physics_.GetGravity().GetY();
    const float fall_s = std::sqrt(2.0F * drop / std::max(gravity, 0.1F));
    const auto deck_under = [&](const RVec3 p) {
        RayCastResult hit;
        const RRayCast ray(p, Vec3(0.0F, -3.0F, 0.0F));
        if (!physics_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, IgnoreSingleBodyFilter(body_))) {
            return false;
        }
        const RVec3 on = ray.GetPointOnRay(hit.mFraction);
        const float top = float(on.GetY());
        if (top < float(line.bottom_floor.GetY()) - 0.4F || top > float(at.GetY()) - 0.2F) {
            return false;
        }
        const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mBodyID);
        return lock.Succeeded() &&
               lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, on).GetY() > 0.7F;
    };
    // Her swing on is her own momentum: no push when she lets go (there is
    // nothing to push on but the trolley she hangs from). Where her motion
    // carries her while she drops is where letting go puts her: she lets go
    // still, straight down, onto a floor under her, or while the trolley
    // still carries her on (no faster than kLetGoSwingMps) when that motion
    // comes down on a floor, clear of the shaft.
    const Vec3 vel = bodies.GetLinearVelocity(body_);
    const Vec3 flat(vel.GetX(), 0.0F, vel.GetZ());
    const float on_run = flat.Dot(run);
    const bool still = std::abs(ride.speed_mps) < 0.3F;
    const bool down_here = !over_shaft(at) && deck_under(at);
    const RVec3 carried = at + flat * fall_s;
    const bool carried_lands = on_run > 0.0F && on_run <= kLetGoSwingMps && !over_shaft(carried) && deck_under(carried);
    // Where a full swing on would come down: whether braking to a stop is
    // worth it here (the deck is ahead) or the drag comes off.
    const RVec3 full = at + run * (kLetGoSwingMps * fall_s);
    const bool deck = down_here || carried_lands || (!over_shaft(full) && deck_under(full));
    // Her forearms' drag: friction on her weight across the cable, as a
    // squeeze of the line's brake. At the end with the swing still over
    // the hole, the drag comes off so she can come on as far as the cable
    // lets her.
    const float across = std::sqrt(std::max(0.0F, 1.0F - line.direction.GetY() * line.direction.GetY()));
    zipline_->set_other_brake(at_end && (over_shaft(at) || !deck)
                                  ? 0.0F
                                  : kCableFriction * kMassKg * gravity * across / Zipline::kHandBrakeMaxN);
    if (at_end && still && !down_here) {
        line_stall_s_ += 1.0F / 90.0F;
    } else {
        line_stall_s_ = 0.0F;
    }
    if (at_end && ((still && (down_here || line_stall_s_ >= 2.0F)) || (!still && carried_lands))) {
        zipline_->unclip_other();
        on_line_ = false;
        line_stall_s_ = 0.0F;
        // The cable was what riding_ meant. Leaving it has to clear that
        // before the hunt runs this tick, or the cling stands her still
        // and her swing on never happens: she drops where she was, in the
        // hole. Off the cable she is falling, not standing on a floor that
        // just stopped: the riding hold (which brakes her to still for a
        // second after a moving floor stops) would also brake the motion
        // she carries out of her while she drops. Clear both here.
        riding_ = false;
        let_go_s_ = 0.0F;
    }
}

void Matron::report_line() {
    view_.on_line = on_line_;
    view_.cable_slides = cable_slides_;
}

void Matron::restore_line() noexcept {
    if (zipline_ != nullptr && on_line_) {
        zipline_->unclip_other();
    }
    on_line_ = false;
    line_stall_s_ = 0.0F;
    cable_slides_ = 0;
}

// A line as a way of hers (matron_mover.hpp): to its stand and onto its cable
// (follow_line), then the ride (ride_line) at the top of her tick.
MoverStatus Matron::LineWay::step(const float dt, traversal::Budget & /*budget*/) {
    if (m_.on_line_) {
        m_.ride_line();
        return MoverStatus::Running;
    }
    if (!m_.follow_line(dt, goal_.at, goal_.wait_ok)) {
        return MoverStatus::Idle;
    }
    return status() == MoverStatus::Running ? MoverStatus::Running : MoverStatus::Walking;
}

void Matron::LineWay::cancel() {
    if (!m_.on_line_) {
        return;
    }
    if (m_.zipline_ != nullptr) {
        m_.zipline_->unclip_other();
    }
    m_.on_line_ = false;
    m_.line_stall_s_ = 0.0F;
    m_.riding_ = false;
    m_.let_go_s_ = 0.0F;
}

MoverStatus Matron::LineWay::status() const {
    return m_.on_line_ ? MoverStatus::Running : MoverStatus::Idle;
}

MoverView Matron::LineWay::view() const {
    MoverView v;
    v.tick = MoverTick::Rides;
    if (!m_.on_line_) {
        return v;
    }
    v.drives = true;
    v.held = true;
    v.lands = true;
    v.blow_proof = true;
    return v;
}

} // namespace scraperx::sim
