#pragma once

// The Matron's perception boundary (00_START_HERE.md rule 29: perception is
// evidence with an age).
//
// The world knows the player's true state; she does not. It hands that state
// over as a Stimulus, which her code can carry but cannot open: its fields are
// private and only Perception reads them. Perception, once a tick, turns it
// into Evidence: what her eyes saw this tick and where, what her ears heard
// and from where with its level at her ear, what her hand feels while she
// holds him, and the last of each with its age. Her decisions read the
// Evidence and her own memory, never the Stimulus. A decision that wanted his
// true position would have to name a Stimulus field, and that does not
// compile outside Perception.

#include <Jolt/Jolt.h>

#include <Jolt/Math/Real.h>
#include <Jolt/Math/Vec3.h>

#include <cstdint>
#include <limits>

namespace scraperx::sim {

class Matron;
class Perception;

// The player's true state this tick, as the world knows it. Write-only to
// her: built by the world (or a test), read only by Perception.
class Stimulus {
public:
    Stimulus() = default;
    // `at`: his body's centre; `loudness`: 0..1, the noise his movement makes
    // at its source (physics_world_movement.cpp matron_loudness).
    Stimulus(const JPH::RVec3 at, const float loudness) noexcept : at_(at), loudness_(loudness) {}

private:
    friend class Perception;
    JPH::RVec3 at_ = JPH::RVec3::sZero();
    float loudness_ = 0.0F;
};

// What the world tells her between ticks about him (his facing, what is in
// his hands): stored by her setters, read only by Perception.
class StimulusExtras {
public:
    void look(const JPH::Vec3 v) noexcept { look_ = v; }
    void holding(const std::uint64_t entity) noexcept { holding_ = entity; }

private:
    friend class Perception;
    JPH::Vec3 look_ = JPH::Vec3::sZero();
    std::uint64_t holding_ = 0;
};

// What she perceived. Built only by Perception; everything of hers reads it.
// This tick's fields are reset every tick; the memory fields keep the last
// of each and age.
struct Evidence {
    static constexpr float kNever = 3600.0F;

    // Her eyes, this tick (Matron::look_at: her head's height and facing,
    // her field of view, a real line through the solids).
    bool saw = false;          // in sight (kEyeRangeM, on her level or up a climb from it)
    bool glimpsed = false;     // at the edge of her eye, close: a lead, no sighting
    bool sees_far = false;     // the long look across the shaft (kCommitSightM)
    bool watched = false;      // his face turned to her, seen
    bool close_look = false;   // in sight within kSightM wherever he is (her trap, a line he rides)
    JPH::RVec3 seen_at = JPH::RVec3::sZero();   // his body's centre where she saw it (any of the above)
    std::uint64_t seen_holding = 0;              // the entity she saw in his hands (saw only)

    // Her ears, this tick.
    bool sound = false;        // his noise carried to her ear (reach over the carried path)
    bool breath = false;       // within kBreathM: his breath, the steel taking his weight
    JPH::RVec3 heard_at = JPH::RVec3::sZero();  // where it came from (sound or breath)
    float loudness = 0.0F;     // his noise as she heard it: its source level, 0 unheard
    float carry_m = std::numeric_limits<float>::max();   // the way it carried to her ear
    float level = 0.0F;        // its level at her ear, 0..1

    // Seen or heard on a ladder from her level: that climb (-1 for none, or
    // when nothing of him reached her).
    int climb = -1;

    // Her hand on him (a hold): his body where it is in her hand.
    bool touch = false;
    JPH::RVec3 felt_at = JPH::RVec3::sZero();

    // Memory: the last of each, and how old it is.
    JPH::RVec3 last_seen_at = JPH::RVec3::sZero();
    float seen_age_s = kNever;
    JPH::RVec3 last_heard_at = JPH::RVec3::sZero();
    float heard_age_s = kNever;

    [[nodiscard]] bool heard() const noexcept { return sound || breath; }
    [[nodiscard]] bool seen() const noexcept { return saw || glimpsed || sees_far; }
    // Where this tick's evidence has him (sight first, then sound, then her
    // hand); false when nothing of him reached her.
    [[nodiscard]] bool fix(JPH::RVec3 &at) const noexcept {
        if (seen()) {
            at = seen_at;
            return true;
        }
        if (heard()) {
            at = heard_at;
            return true;
        }
        if (touch) {
            at = felt_at;
            return true;
        }
        return false;
    }
};

// The boundary: the one place that reads a Stimulus. A friend of Matron, so it
// can look through her eyes and listen with her ears; it writes only her
// Evidence (and the world's checkpoint flag).
class Perception {
public:
    // Once a tick, before any decision of hers: this tick's evidence from what
    // the world handed her, in her state as it stands.
    static void take(Matron &m, const Stimulus &now, float dt);

private:
    static bool face_turned_to_her(const Matron &m, JPH::RVec3 player, JPH::Vec3 look);
};

}  // namespace scraperx::sim
