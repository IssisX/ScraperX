// The climbing lab's exam on structure nobody has seen (issue #3): a seeded
// generator builds one climbable structure in an empty world, and a judge
// watches a body each tick against the lab's pass rule. Test code only:
// runtime code never reads a seed, a parameter or this header.

#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Geometry/AABox.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cstdint>
#include <string>
#include <vector>

namespace scraperx::sim::exam {

enum class Kind : std::uint8_t { Ladder, Pole, Panel, LedgeStack };
[[nodiscard]] const char *kind_name(Kind kind) noexcept;

// What the generator chose for one seed, kept for the record and the bounds.
struct Params {
    Kind kind = Kind::Ladder;
    float height_m = 0.0F;       // the goal's top over the floor's
    float pitch_m = 0.0F;        // rung or bar spacing; a ledge stack's rise
    float first_hold_m = 0.0F;   // the lowest hold over the floor's top
    float section_m = 0.0F;      // a rung's, bar's or pole's section
    float standoff_m = 0.0F;     // the hold line off the goal's face
    float opening_m = 0.0F;      // the gap in the goal's rail over the holds
    int missing = -1;            // the rung or bar left out, or -1
    bool wall = false;           // a wall on the floor across the straight way
};

// The bounds every seed keeps, from her body (a 0.30 m radius, 2.1 m tall
// capsule of 220 kg): with them, every structure is climbable by the moves
// the lab names (rungs, bars, a pole, a mantle).
inline constexpr float kMinPitchM = 0.22F;
inline constexpr float kMaxPitchM = 0.42F;
inline constexpr float kMaxHoldGapM = 0.90F;       // between two holds, a missing one included
inline constexpr float kMinFirstHoldM = 0.20F;
inline constexpr float kMaxFirstHoldM = 2.40F;     // a panel's first bar: within a stretch, or a jump
inline constexpr float kMinOpeningM = 0.90F;       // the rail's gap: her 0.60 m and room
inline constexpr float kMinRiseM = 0.80F;          // a ledge stack's step
inline constexpr float kMaxRiseM = 1.60F;

struct Structure {
    std::uint64_t seed = 0;
    Params params;
    JPH::BodyID floor;                  // the start's support
    JPH::BodyID goal;                   // the goal's support
    JPH::BodyID holds;                  // the structure (a static compound), or invalid
    JPH::RVec3 start_at;                // her feet at the start, on the floor
    JPH::AABox start_feet;              // where her feet must stand after the way down
    JPH::AABox goal_feet;               // where her feet must stand at the top
    std::vector<JPH::BodyID> bodies;    // everything built, for the caller to remove
    std::uint64_t fingerprint = 0;      // of the built world: the same seed, the same number
};

// Static bodies only, in `static_layer`: a floor (top at y 0), the structure
// on it, and the goal (a platform with its rail).
[[nodiscard]] Structure build(JPH::PhysicsSystem &physics, std::uint64_t seed, JPH::ObjectLayer static_layer);

struct Verdict {
    bool arrived = false;           // stood on the support a second, holding nothing
    bool alive = true;
    float worst_step_m = 0.0F;      // the largest move in one tick
    float peak_commanded_n = 0.0F;  // the largest commanded force reported
    float worst_landing_mps = 0.0F;
    float supported_s = 0.0F;       // the current run of supported standing
    std::string reason;             // the first broken rule, or "arrived", or "not yet"
};

// Fed once a tick by the harness, never by the executor under test. Any
// broken rule ends the exam as failed, whatever comes after.
class Judge {
public:
    static constexpr float kMaxStepM = 0.20F;
    static constexpr float kSupportedSeconds = 1.0F;
    static constexpr float kStillMps = 0.30F;
    static constexpr float kLethalLandingMps = 15.34F;   // a 12 m fall

    Judge(JPH::BodyID support, const JPH::AABox &feet, float force_cap_n, float stand_centre_m);
    void tick(const JPH::PhysicsSystem &physics, JPH::BodyID body, float commanded_force_n, bool gripping, float dt);
    [[nodiscard]] const Verdict &verdict() const noexcept { return verdict_; }
    [[nodiscard]] bool done() const noexcept { return verdict_.arrived || failed_; }

private:
    void fail(const char *why);

    JPH::BodyID support_;
    JPH::AABox feet_;
    float cap_n_;
    float stand_centre_m_;
    bool started_ = false;
    bool failed_ = false;
    JPH::RVec3 last_ = JPH::RVec3::sZero();
    float last_vy_ = 0.0F;
    Verdict verdict_;
};

} // namespace scraperx::sim::exam
