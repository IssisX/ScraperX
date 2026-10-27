#pragma once

// One contract for every way of hers between floors (issue #3, Board v5,
// step 1). Each executor she has (the Climber's journey over her scan, the
// routecraft's bars with the column it found, a travel, the corner pipe, the
// old rung climber, the authored map's way, a cable) is a Mover. pre_step
// holds at most one that has her body (Matron::holder), steps it at the
// place in the tick its kind has always been stepped, and asks one planner
// call (Matron::take_way) for the next. No mover reads another's state: what
// the rest of her needs of a mover it reads from its view().
//
// This first contract changes no executor: each still makes its own checks
// in its first step, and each still applies its own caps, every one within
// her law. step's budget is her tick's account; no way draws from it yet
// (one budget for every way is the plan's step 6, HOW_SHE_MOVES.md).

#include "sim/matron_traversal.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>

#include <cstdint>

namespace scraperx::sim {

// Where she is going: her lead, the place she heard or saw the player and the
// floor it is on, from where she stands this tick.
struct MoverGoal {
    JPH::RVec3 at = JPH::RVec3::sZero();   // her body this tick
    float x = 0.0F;
    float z = 0.0F;
    float floor_y = 0.0F;
    // A way she would only wait at (a cable's stand before anyone rode it,
    // outside her Turn) counts.
    bool wait_ok = true;
};

enum class MoverStatus : std::uint8_t {
    Idle,      // not hers: no way for this goal from here, or none under way
    Walking,   // taking her to its start; her feet are still the hunt's
    Running,   // it has her body
};

// Where in her tick a mover that has her body is stepped. These are the
// places each kind was stepped before the contract; keeping them keeps what
// she does tick by tick the same.
enum class MoverTick : std::uint8_t {
    Owns,     // first in her tick, and nothing of the hunt runs (a journey, routecraft)
    Travel,   // after her clocks and her floor, nothing of the hunt runs (travel_to)
    Hunt,     // inside the hunt, after she hears and sees (the corner pipe, the old climber, her map's way)
    Rides,    // on its cable at the top of her tick; the hunt holds her still meanwhile
};

// What the rest of her reads of a mover. Idle, a view says nothing of her
// body; hand stays the last hold it had.
struct MoverView {
    MoverTick tick = MoverTick::Hunt;
    bool turns = false;            // her Turn's clock runs after its step (step_turn)
    // What its having her body means to the rest of her:
    bool drives = false;           // it moves her body: her stride's drive is off, and a travel waits off a floor
    bool held = false;             // it holds her on its steel or cable: she catches nobody and is not re-landed
    bool lands = false;            // a hard stop while it has her is its own, not a fall's landing
    bool holds_chase = false;      // her chase window holds while it has her
    bool blow_proof = false;       // a blow is not hers to take while it has her
    bool floor_in_hands = false;   // her floor is the steel in her hands, not what her feet find
    JPH::Vec3 drive_dv = JPH::Vec3::sZero();   // what its drive changes her velocity by this tick, where it keeps that count
    // What her report shows of it:
    bool hands = false;            // a hand of hers on its hold: she is climbing
    bool move_active = false;      // a move of it under way (walking to its start excluded)
    bool bar_traverse = false;     // hand over hand along bars
    bool airborne = false;
    bool sliding = false;
    bool leg_driving = false;
    traversal::MoveKind move = traversal::MoveKind::Walk;
    int phase = 0;
    bool hand_shown = false;       // hand is the hand her report shows
    JPH::RVec3 hand = JPH::RVec3::sZero();
    bool force_shown = false;      // force_n is the force her report shows this tick
    float force_n = 0.0F;
    // Her first hand on a hold whose support is known (her second hand is
    // posed beside it); on rungs across her facing, the rungs' run to her
    // left, the body and member she holds, and the floor over its head.
    bool hand_held = false;
    bool rungs_across = false;
    JPH::Vec3 rung_left = JPH::Vec3::sZero();
    JPH::BodyID hold_body;
    JPH::SubShapeID hold_member;
    JPH::RVec3 top = JPH::RVec3::sZero();
    // The floor it takes her to, when it knows one, and the floor it sets out
    // from, when that counts (her chase window counts a way toward her lead).
    bool has_goal = false;
    float goal_floor_y = 0.0F;
    bool has_from = false;
    float from_floor_y = 0.0F;
};

class Mover {
public:
    virtual ~Mover() = default;
    // This goal is one it may take from where she is (false: the planner asks
    // the next way).
    virtual bool begin(const MoverGoal &goal) = 0;
    // This tick's part of it. Idle: it declined after all, nothing done, and
    // the planner asks the next way this same tick.
    virtual MoverStatus step(float dt, traversal::Budget &budget) = 0;
    // Off it now, nothing held against the way: her hands let go, her feet
    // are the hunt's again.
    virtual void cancel() = 0;
    [[nodiscard]] virtual MoverStatus status() const = 0;
    [[nodiscard]] virtual MoverView view() const = 0;
};

} // namespace scraperx::sim
