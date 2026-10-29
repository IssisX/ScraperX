// The Matron's perception boundary (matron_perception.hpp): the one place
// that reads the player's true state, and what it may hand her.
#include "sim/matron.hpp"
#include "sim/zipline.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace scraperx::sim {
using namespace JPH;

namespace {
float xz(const float ax, const float az, const float bx, const float bz) { return std::hypot(ax - bx, az - bz); }
}  // namespace

// His face turned to her, seen: her eyes on his head, not a read of where he
// looks.
bool Perception::face_turned_to_her(const Matron &m, const RVec3 player, const Vec3 look) {
    if (look.LengthSq() < 0.25F) {
        return false;   // he faces nowhere in particular
    }
    const Matron::Eyes e = m.eyes();
    if (!e.open) {
        return false;
    }
    const RVec3 head = player + RVec3(0.0, Matron::kPlayerEyeAboveCentreM, 0.0);
    const Vec3 to_her(e.at - head);
    const float length = to_her.Length();
    if (length > 1.0e-3F && look.Normalized().Dot(to_her / length) < Matron::kFaceTurnedCos) {
        return false;
    }
    return m.look_at(head, Matron::kEyeRangeM) == Matron::Glimpse::Sight;
}

void Perception::take(Matron &m, const Stimulus &now, const float dt) {
    using State = Matron::State;
    Evidence &ev = m.ev_;
    const StimulusExtras &extras = m.extras_;
    const RVec3 player = now.at_;
    const float loudness = now.loudness_;

    // This tick's evidence starts empty; the memory ages.
    ev.saw = ev.glimpsed = ev.sees_far = ev.watched = ev.close_look = false;
    ev.seen_holding = 0;
    ev.sound = ev.breath = false;
    ev.loudness = 0.0F;
    ev.carry_m = std::numeric_limits<float>::max();
    ev.level = 0.0F;
    ev.climb = -1;
    ev.touch = false;
    ev.seen_age_s = std::min(ev.seen_age_s + dt, Evidence::kNever);
    ev.heard_age_s = std::min(ev.heard_age_s + dt, Evidence::kNever);

    const float px = float(player.GetX());
    const float pz = float(player.GetZ());
    // On her level (the height of the floor under her, within the tower's
    // footprint) the player can be seen and heard at all: her deck, the ring
    // a cage brought her to, the cage she rides with the player in it.
    const bool in_footprint = std::abs(px) < 27.0F && pz < -123.0F && pz > -177.0F;
    const bool on_deck = player.GetY() > m.feet_y_ - 1.0 && player.GetY() < m.feet_y_ + 6.0 && in_footprint;
    // Or up a ladder from her level (in its column, off its foot's floor).
    int player_climb = -1;
    const float level_floor = m.levels_[static_cast<std::size_t>(m.level_)].floor_y;
    for (int i = 0; i < m.climb_count(); ++i) {
        Matron::Climb c{};
        // Only a ladder has a middle he can be on: by a short lip's window he is standing on its top.
        if (!m.climb_of(i, c, Matron::Ways::Ladders)) {
            continue;
        }
        if (std::abs(c.foot_y - level_floor) < 0.6F && std::abs(px - c.x) < 1.0F && std::abs(pz - c.z) < 1.0F &&
            player.GetY() > c.foot_y + 1.3F && player.GetY() < c.head_y + 1.2F) {
            player_climb = i;
        }
    }
    // The world's checkpoint rule (Matron::freezes_checkpoint), not hers to
    // read: he is on her deck, up a ladder from it, or on the deck her riser
    // is taking her to, or (no longer frail) a deck a riser of hers joins to hers.
    {
        const float player_floor_y = m.floor_of(float(player.GetY()));
        const Mover *taking = m.holder();
        const bool by_her_pipe =
            m.body_in_world_ && in_footprint &&
            ((taking != nullptr && taking->view().has_goal &&
              std::abs(player_floor_y - taking->view().goal_floor_y) < 0.1F) ||
             (m.stage_ != Matron::Stage::Pest && m.riser_joins(level_floor, player_floor_y)));
        m.checkpoint_freeze_ = on_deck || player_climb >= 0 || by_her_pipe;
    }
    if (m.dead_) {
        return;
    }

    // Her hand on him: what she holds she feels, where it is.
    if (Matron::holding_state(m.state_) || m.state_ == State::Release) {
        ev.touch = true;
        ev.felt_at = player;
        return;
    }

    // Her ears: his noise, reach over the way it carried to her ear (her
    // locker while she sleeps, her body awake). Just off the corner pipe she
    // stands listening: every sound carries farther to her.
    const BodyInterface &bodies = m.physics_.GetBodyInterface();
    const RVec3 body_at = m.body_in_world_ ? bodies.GetPosition(m.body_) : RVec3::sZero();
    const float hearing = m.body_in_world_ && m.pipe_way_.status() == MoverStatus::Idle && m.pipe_listen_s_ > 0.0F
                              ? Matron::kListenReach
                              : 1.0F;
    const RVec3 locker_ear(Matron::kLockerX, m.home_y_ + Matron::kLockerHalfY, Matron::kLockerZ);
    const RVec3 ear = m.body_in_world_ ? body_at : locker_ear;
    const float reach = loudness * Matron::kFullHearingM * hearing;
    const float carry = loudness > 0.0F && Vec3(player - ear).Length() <= reach ? m.carry_m(player, ear)
                                                                                : std::numeric_limits<float>::max();
    ev.sound = reach >= carry;
    if (ev.sound) {
        ev.loudness = loudness;
        ev.carry_m = carry;
        ev.level = loudness * std::max(0.0F, 1.0F - carry / std::max(reach, 0.1F));
    }
    // Within kBreathM of her body: his breath and the steel taking his weight
    // (still and silent).
    ev.breath = m.body_in_world_ && Vec3(player - body_at).Length() <= Matron::kBreathM &&
                m.carry_m(player, body_at) <= Matron::kBreathM;
    if (ev.heard()) {
        ev.heard_at = player;
        ev.last_heard_at = player;
        ev.heard_age_s = 0.0F;
    }

    // Her eyes, awake and on her feet about the world.
    const bool looking = m.body_in_world_ && (m.state_ == State::Hunting || m.state_ == State::Searching ||
                                              m.state_ == State::Shuffling || m.state_ == State::Guarding ||
                                              m.state_ == State::Peering || m.state_ == State::Unrigging);
    if (looking) {
        // Seen on her level, or up a climb from it (the straight line to the
        // player mostly up it).
        const bool perceivable = on_deck || player_climb >= 0;
        const float flat = xz(float(body_at.GetX()), float(body_at.GetZ()), px, pz);
        const float span = on_deck ? flat : std::hypot(flat, float(player.GetY() - body_at.GetY()));
        const RVec3 his_body = player - RVec3(0.0, 0.3, 0.0);
        if (perceivable && span <= Matron::kEyeRangeM) {
            const Matron::Glimpse g = m.look_at(his_body, Matron::kEyeRangeM);
            ev.saw = g == Matron::Glimpse::Sight;
            ev.glimpsed = g == Matron::Glimpse::Peripheral;
        }
        ev.watched = ev.saw && face_turned_to_her(m, player, extras.look_);
        ev.sees_far = perceivable && span <= Matron::kCommitSightM &&
                      (ev.saw || m.look_at(his_body, Matron::kCommitSightM) == Matron::Glimpse::Sight);
        if (ev.saw) {
            ev.seen_holding = extras.holding_;
        }
    }
    // Close, wherever he is and whatever she is about: under the bay's leaf in
    // her trap, or a line he rides past her (her own climbs included).
    if (m.body_in_world_ && (m.trap_ == Matron::Trap::Holding || (m.zipline_ != nullptr && m.zipline_->riding())) &&
        Vec3(player - body_at).Length() <= Matron::kSightM) {
        ev.close_look = m.look_at(player - RVec3(0.0, 0.3, 0.0), Matron::kSightM) == Matron::Glimpse::Sight;
    }
    if (ev.seen() || ev.close_look) {
        ev.seen_at = player;
        ev.last_seen_at = player;
        ev.seen_age_s = 0.0F;
    }
    if (ev.saw || ev.glimpsed || ev.sees_far || ev.heard()) {
        ev.climb = player_climb;
    }
}

bool Matron::knows_where(const float max_age_s, RVec3 &at) const noexcept {
    if (ev_.fix(at)) {
        return true;
    }
    const bool seen = ev_.seen_age_s <= max_age_s;
    const bool heard = ev_.heard_age_s <= max_age_s;
    if (seen && (!heard || ev_.seen_age_s <= ev_.heard_age_s)) {
        at = ev_.last_seen_at;
        return true;
    }
    if (heard) {
        at = ev_.last_heard_at;
        return true;
    }
    return false;
}

}  // namespace scraperx::sim
