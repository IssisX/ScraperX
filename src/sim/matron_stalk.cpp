// The stalk (matron.hpp, kStalkNearM): she stalks a player who has seen her,
// and takes one who has not. Under his gaze she freezes; looked away from,
// she closes to her band and holds there, facing him. Seen up a ladder in a
// stalk, she goes round to its head by another way when hers is the shorter,
// and waits there. Everything she has of him here is what her eyes gave her
// (pre_step: sees_player_, watched_, the seen lead); her feet are walk_toward's
// (force within her budget) and her ways are her own travel over her scan.

#include "sim/matron.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace scraperx::sim {
using namespace JPH;

namespace {
constexpr float kPi = 3.14159265F;
constexpr float kTurnRate = 7.0F;   // as matron.cpp

float turn_toward(const float yaw, const float want, const float rate, const float dt) {
    const float delta = std::remainder(want - yaw, 2.0F * kPi);
    const float step = std::clamp(delta, -rate * dt, rate * dt);
    return std::remainder(yaw + step, 2.0F * kPi);
}

float distance_xz(const float ax, const float az, const float bx, const float bz) {
    return std::hypot(ax - bx, az - bz);
}
}  // namespace

bool Matron::face_turned_to_her(const RVec3 player) const {
    if (player_look_.LengthSq() < 0.25F) {
        return false;   // he faces nowhere in particular
    }
    const Eyes e = eyes();
    if (!e.open) {
        return false;
    }
    const RVec3 head = player + RVec3(0.0, kPlayerEyeAboveCentreM, 0.0);
    const Vec3 to_her(e.at - head);
    const float length = to_her.Length();
    if (length > 1.0e-3F && player_look_.Normalized().Dot(to_her / length) < kFaceTurnedCos) {
        return false;
    }
    // His face, seen: her eyes on his head, not a read of where he looks.
    return look_at(head, kEyeRangeM) == Glimpse::Sight;
}

void Matron::end_encounter() noexcept {
    noticed_ = false;
    watched_ = false;
    stalking_ = false;
    stalk_spent_ = false;
    stalk_s_ = 0.0F;
    ambush_ = Ambush{};
    seen_climb_ = -1;
    seen_climb_watch_s_ = 0.0F;
    seen_climb_rate_ = -1.0F;
    ambush_weighed_t_ = -1.0F;
}

bool Matron::stalk_step(const float dt, const float ax, const float az, const float feet_y, const float wx,
                        const float wz) {
    stalking_ = false;
    if (!stalk_encounter()) {
        return false;
    }
    // Him in sight a moment ago, on her own floor, on no climb or line.
    const bool fresh = lead_sense_ == Sense::Sight && since_heard_s_ <= kStalkSightMemoryS && guess_climb_ < 0 &&
                       guess_line_ < 0 && std::abs(lead_floor_ - feet_y) <= 1.0F;
    if (!fresh || held_ >= 0) {
        return false;
    }
    const float near_him = distance_xz(ax, az, heard_x_, heard_z_);
    // The rule's other half: come within her strike, out of patience, or
    // in the Turn (his noise), she attacks, and this encounter's stalk is spent.
    if (near_him < kStalkStrikeM || stalk_s_ >= kStalkPatienceS || turn_ != Turn::None) {
        stalk_spent_ = true;
        ++stalk_attacks_;
        return false;
    }
    stalking_ = true;
    stalk_s_ += dt;
    float speed = 0.0F;
    if (!watched_) {
        speed = near_him > kStalkFarM ? kStrideMps : near_him > kStalkNearM ? kCreepMps : 0.0F;
    }
    if (speed > 0.0F) {
        pace_ = speed == kStrideMps ? Pace::Stride : Pace::Creep;
        walk_toward(dt, wx, wz, speed);
    } else {
        // Frozen under his gaze, or holding at her band's near edge: still,
        // her face on him.
        pace_ = Pace::Creep;
        walk_toward(dt, ax, az, 0.0F);
        yaw_ = turn_toward(yaw_, std::atan2(heard_x_ - ax, heard_z_ - az), kTurnRate, dt);
    }
    return true;
}

void Matron::watch_climber(const int edge, const float player_y, const float dt) {
    if (edge != seen_climb_) {
        seen_climb_ = edge;
        seen_climb_y_ = player_y;
        seen_climb_t_ = clock_s_;
        seen_climb_watch_s_ = 0.0F;
        seen_climb_rate_ = -1.0F;
        return;
    }
    seen_climb_watch_s_ += dt;
    const float span = clock_s_ - seen_climb_t_;
    if (span >= kAmbushWatchS) {
        seen_climb_rate_ = std::max(0.0F, (player_y - seen_climb_y_) / span);
        seen_climb_y_ = player_y;
        seen_climb_t_ = clock_s_;
    }
}

bool Matron::start_ambush(const int edge, const RVec3 at, const float player_y) {
    // Only once she has watched him on it long enough to know his pace.
    if (scanned_ == nullptr || edge != seen_climb_ || seen_climb_rate_ < 0.0F) {
        return false;
    }
    Climb c{};
    if (!climb_of(edge, c, Ways::Ladders)) {
        return false;
    }
    const std::vector<traversal::Edge> &edges = scanned_->edges();
    const traversal::Edge &taken = edges[static_cast<std::size_t>(edge)];
    const float feet = float(at.GetY()) - kStandCentre;
    int from = scanned_->region_at(Vec3(float(at.GetX()), feet, float(at.GetZ())));
    if (from < 0) {
        from = region_under();
    }
    if (from < 0 || taken.to < 0 || from == taken.to) {
        return false;
    }
    // Not by his ladder: every move of her scan's at its column, either way.
    std::vector<int> avoid;
    for (std::size_t i = 0; i < edges.size(); ++i) {
        const traversal::Edge &other = edges[i];
        if (other.kind != traversal::MoveKind::Walk &&
            distance_xz(other.spec.line.GetX(), other.spec.line.GetZ(), taken.spec.line.GetX(),
                        taken.spec.line.GetZ()) < kAmbushColumnM) {
            avoid.push_back(static_cast<int>(i));
        }
    }
    const std::vector<int> plan = scanned_->plan(from, taken.to, avoid);
    if (plan.empty()) {
        return false;
    }
    // Her way: its moves' costs (seconds), and her walk to the first.
    float hers = 0.0F;
    for (const int k : plan) {
        hers += edges[static_cast<std::size_t>(k)].cost;
    }
    const Vec3 first = edges[static_cast<std::size_t>(plan.front())].spec.start;
    hers += distance_xz(float(at.GetX()), float(at.GetZ()), first.GetX(), first.GetZ()) / kStrideMps;
    // His: what is left of the ladder at the pace she saw him climb it.
    const float left = std::max(0.0F, c.head_y - (player_y - 0.9F));
    const float his = seen_climb_rate_ > 0.05F ? left / seen_climb_rate_ : std::numeric_limits<float>::max();
    if (hers + kAmbushMarginS >= his) {
        return false;
    }
    if (!travel_to(taken.to, avoid)) {
        return false;
    }
    ambush_ = Ambush{true, false, edge, c.top_x, c.top_z, c.head_y, c.x, c.z, 0.0F};
    ++ambushes_;
    return true;
}

bool Matron::ambush_step(const float dt, const float ax, const float az, const float feet_y, const bool saw) {
    // He is in sight off any climb (topped out, or come round): the ambush is
    // over, and the stalk's rule has her.
    if (saw && guess_climb_ < 0) {
        ambush_ = Ambush{};
        return false;
    }
    // Her travel ended off the head's floor (it failed under her): the hunt
    // has her again.
    if (std::abs(feet_y - ambush_.head_y) > 0.5F) {
        ambush_ = Ambush{};
        return false;
    }
    const float to_head = distance_xz(ax, az, ambush_.head_x, ambush_.head_z);
    if (to_head > 0.5F) {
        // Over the head's floor to it, by her scan's patches.
        float wx = ambush_.head_x;
        float wz = ambush_.head_z;
        if (scanned_ != nullptr) {
            const std::vector<Vec3> path =
                scanned_->walk_path(Vec3(ax, feet_y, az), Vec3(ambush_.head_x, ambush_.head_y, ambush_.head_z));
            for (const Vec3 &p : path) {
                if (distance_xz(ax, az, p.GetX(), p.GetZ()) > 1.2F) {
                    wx = p.GetX();
                    wz = p.GetZ();
                    break;
                }
            }
        }
        ambush_.waiting = false;
        pace_ = Pace::Stalk;
        walk_toward(dt, wx, wz, kStalkMps);
        return true;
    }
    // At its head: still, facing down it, as long as she will.
    ambush_.waiting = true;
    ambush_.wait_s += dt;
    pace_ = Pace::Creep;
    walk_toward(dt, ax, az, 0.0F);
    yaw_ = turn_toward(yaw_, std::atan2(ambush_.line_x - ax, ambush_.line_z - az), kTurnRate, dt);
    if (ambush_.wait_s >= kAmbushWaitS) {
        ambush_ = Ambush{};
    }
    return true;
}

}  // namespace scraperx::sim
