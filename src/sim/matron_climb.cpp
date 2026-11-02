// Her rung way in the hunt (matron.hpp): up or down a climb her scan found,
// by her Climber's own moves (matron_traversal.hpp), the moves every other
// climb of hers is made of: a ladder's rungs hand over hand and over its
// head, a lip's mantle, and down either again. A Mover (matron_mover.hpp).
#include "sim/matron.hpp"
#include "sim/matron_traversal.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

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

// The way down a climb her scan read going up, as her scan pairs the two
// (AffordanceMap::build_edges): the same steel, from the floor at its head
// she would have topped out onto to the floor at its foot she would have
// set out from. Down a ladder's rungs; off a lip, hanging from it and then
// her feet down onto the floor under it (as low as that floor lets her hang).
traversal::MoveSpec down_of(const traversal::MoveSpec &up) {
    traversal::MoveSpec down = up;
    down.kind = up.kind == traversal::MoveKind::Mantle || up.kind == traversal::MoveKind::LedgeJump
                    ? traversal::MoveKind::HangDrop
                    : traversal::MoveKind::RungDown;
    down.start = up.goal;
    down.goal = up.start;
    down.goal_top = up.start.GetY();
    return down;
}
} // namespace

bool Matron::follow_climb(const float dt, const RVec3 at) {
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const float level_floor = levels_[static_cast<std::size_t>(level_)].floor_y;
    const float feet = float(at.GetY()) - kStandCentre;
    // A climb from her level up toward her lead's (to it, or to a level on
    // the way): the one she saw taken, or the nearest. Its foot must be
    // under her own feet, not only under her level: her level can name a
    // floor she left (a scan-only floor has no level), and a climb from
    // that floor is far below her with its foot's x/z near hers — walking
    // to it strands her on the edge above it, claimed every tick.
    int best = -1;
    float best_d = std::numeric_limits<float>::max();
    for (int i = 0; i < climb_count(); ++i) {
        Climb c{};
        // Her hand climb in the hunt is for ladders; a lip she takes by her own plan.
        if (!climb_of(i, c, Ways::Ladders)) {
            continue;
        }
        if (!c.hers || std::abs(c.foot_y - level_floor) > 0.6F || std::abs(c.foot_y - feet) > 0.6F ||
            c.head_y > lead_floor_ + 0.6F || c.head_y < level_floor + 0.6F) {
            continue;
        }
        const float d = i == guess_climb_ ? -1.0F : distance_xz(ax, az, c.x, c.z);
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    if (best < 0) {
        return false;
    }
    const Climb c = climb_spec(best);
    const float off = distance_xz(ax, az, c.x, c.z);
    const float accel = turn_ != Turn::None ? kTurnFootAccel : kFootAccel;
    if (off > 0.2F) {
        // To its foot along her lines, slowing into it.
        float wx = c.x;
        float wz = c.z;
        route_toward(ax, az, c.x, c.z, false, wx, wz);
        walk_toward(dt, wx, wz, std::min(kHuntMps, 1.0F + 3.0F * off), accel);
        return true;
    }
    // At its foot, looking up it. Frail, she cannot take hold of it; in the
    // Turn she does, and her hands have her weight. The bay's rungs she
    // always does: they are her way out of her own trap.
    walk_toward(dt, c.x, c.z, 0.0F, accel);
    yaw_ = turn_toward(yaw_, std::atan2(c.out_x, c.out_z), kTurnRate, dt);
    if (turn_ == Turn::None && best != bay_climb_) {
        return true;
    }
    take_climb(best);
    return true;
}

// Onto a climb of her scan's, up it from its foot or down it from its head,
// as her Climber's move: from her next tick she steps in under its first
// hold and takes it, and every hold after it, on the steel itself (a real
// rung or lip, still there and in her reach), out of her one budget, the
// reaction onto the steel she holds. Not while her hands are on another move.
bool Matron::take_climb(const int index, const bool down) {
    Climb c{};
    if (climb_retry_s_ > 0.0F || !body_in_world_ || !climb_of(index, c)) {
        return false;
    }
    if (climber_ == nullptr) {
        climber_ = std::make_unique<traversal::Climber>(physics_, body_,
                                                        traversal::Ignore{body_, player_, static_cast<int>(moving_layer_)});
    } else if (climber_->status() == traversal::Climber::Status::Running || climber_->view().holding) {
        return false;
    }
    const traversal::MoveSpec &up = scanned_->edges()[static_cast<std::size_t>(index)].spec;
    climber_->begin(down ? down_of(up) : up);
    climb_ = index;
    climb_hand_known_ = false;
    climb_force_n_ = 0.0F;
    // Her hand's reach for it until it holds: its steel's line before her,
    // at her reach overhead, no higher than its lip and no lower than its foot.
    const float reach_y = float(physics_.GetBodyInterface().GetPosition(body_).GetY()) + climber_->limits().hand_above_center;
    climb_hand_ = RVec3(c.x + c.out_x * c.standoff, std::clamp(reach_y, c.foot_y, c.lip_y), c.z + c.out_z * c.standoff);
    chase_way_held_ = true;  // a climb taken holds the chase window past its own ticks
    return true;
}

void Matron::release_climb(const bool failed) {
    if (climb_ >= 0 && climber_ != nullptr) {
        climber_->cancel();   // her hands off whatever they held: a fall if on steel
    }
    climb_ = -1;
    climb_force_n_ = 0.0F;
    if (failed) {
        ++climb_releases_;
        climb_retry_s_ = 2.0F;
        // The column it dropped her from, her found ways leave alone as long
        // (none when no hold of it was ever found).
        if (climb_hand_known_) {
            block_climb_at(float(climb_hand_.GetX()), float(climb_hand_.GetZ()));
        }
    }
    // No velocity write: a missed hand or broken hold leaves her momentum
    // and gravity to the same collision solver as every other falling body.
}

// This tick of her climb: her Climber's move, facing its steel. Done (or up
// over its head), she stands where it put her; failed (no hold, a hold gone,
// no rung in reach, a stall, no room over its head), her hands open and she
// falls.
void Matron::step_climb(const float dt) {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const Climb c = climb_spec(climb_);
    const std::uint32_t grips = climber_->view().grips;
    const traversal::Climber::Status status = climber_->step(dt);
    const traversal::ClimbView &move = climber_->view();
    climb_grips_ += move.grips - grips;
    climb_force_n_ = move.force_n;
    if (climber_->on_steel()) {
        if (!climb_hand_known_) {
            ++climbs_done_;   // her first hold on it: the climb is taken
        }
        climb_hand_ = move.hand;
        climb_hand_known_ = true;
    }
    yaw_ = turn_toward(yaw_, std::atan2(c.out_x, c.out_z), kTurnRate, dt);
    speed_ = 0.0F;
    stride_ += bodies.GetLinearVelocity(body_).Length() * dt;
    if (status == traversal::Climber::Status::Done) {
        release_climb(false);
    } else if (status != traversal::Climber::Status::Running) {
        release_climb(true);
    } else if (move.topped) {
        // Up over its head, her body over its floor: her rung way is done and
        // her feet have her, as her hunt's climbs always handed her over there
        // (her journeys wait for the move's own Done, her weight on her feet,
        // before their next move).
        release_climb(false);
    }
}

// Her rung way (matron_mover.hpp): to a ladder's foot and onto it
// (follow_climb), then up or down it (step_climb) inside her hunt.
MoverStatus Matron::ClimbWay::step(const float dt, traversal::Budget & /*budget*/) {
    if (m_.climb_ >= 0) {
        m_.step_climb(dt);
        return MoverStatus::Running;
    }
    if (!m_.follow_climb(dt, goal_.at)) {
        return MoverStatus::Idle;
    }
    return status() == MoverStatus::Running ? MoverStatus::Running : MoverStatus::Walking;
}

void Matron::ClimbWay::cancel() {
    if (m_.climb_ >= 0) {
        m_.release_climb(false);
    }
}

MoverStatus Matron::ClimbWay::status() const {
    return m_.climb_ >= 0 ? MoverStatus::Running : MoverStatus::Idle;
}

MoverView Matron::ClimbWay::view() const {
    MoverView v;
    v.tick = MoverTick::Hunt;
    v.turns = true;
    v.hand = m_.climb_hand_;   // her report's hand while no way shows one: the last she held or reached for
    if (m_.climb_ < 0 || m_.climber_ == nullptr) {
        return v;
    }
    const traversal::Climber &climber = *m_.climber_;
    v.drives = true;
    v.held = true;
    v.lands = true;
    v.holds_chase = true;
    v.blow_proof = true;
    v.move_active = true;
    v.move = climber.view().kind;
    v.phase = climber.phase();
    if (v.phase > 0) {
        // Past its start (walking to it, her feet are on the floor).
        v.airborne = climber.view().airborne;
        v.sliding = climber.view().sliding;
        v.leg_driving = climber.view().leg_driving;
    }
    v.force_shown = true;
    v.force_n = m_.climb_force_n_;
    // Climbing, from her step in under its first hold to her feet on the
    // floor at its other end: her hand on its steel, reaching for it, or just
    // off it (climb_hand_).
    v.hands = true;
    v.hand_shown = true;
    if (climber.on_steel()) {
        v.hand_held = true;
        // Its rungs (or its lip) run across her facing (out, toward its
        // steel): rung_left runs along them as this way has always reported
        // it; her feet land on its top.
        const Climb spec = m_.climb_spec(m_.climb_);
        v.rungs_across = true;
        v.rung_left = Vec3(spec.out_z, 0.0F, -spec.out_x);
        v.hold_body = climber.hold().body;
        v.hold_member = climber.hold().sub;
        v.top = RVec3(spec.top_x, spec.lip_y, spec.top_z);
    }
    return v;
}

bool Matron::ClimbWay::let_go() {
    if (m_.climb_ < 0) {
        return false;
    }
    m_.release_climb(true);
    return true;
}

} // namespace scraperx::sim
