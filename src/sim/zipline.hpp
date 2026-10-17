#pragma once

// Ziplines (Mirror's Edge's; the owner, 2026-10-17). A taut steel cable from
// a post on one deck down across the well to a lower post on the far band of
// the deck below, or from the lowest deck down to the yard. On the cable
// rides a trolley: two sheaves in a bracket over the cable, a bar hung
// kBarDropM under it that the hands hold. Nothing of the rider's touches the
// cable: the sheaves roll on it (a hand on a long steel cable at speed burns
// off; the owner, 2026-10-17). The trolley parks on a hook under the top
// post's bracket, kParkM down the line, where a jump from the stand behind
// the post (kStandM high, up a ramp at its back) puts its bar in the hands'
// reach: the hands take the bar, the body's weight pulls the trolley off
// the hook, and it rides down under that weight. The slope drives it; the
// sheaves resist a little (kTrolleyDragN, rolling); over the line's last part
// the trolley runs into a brake block, a clamp on the cable at rest
// line.length - brake_m from the top, and pushes it down the cable against
// its clamp's drag (brake_n, sized to the line so the block takes the speed
// off at kBrakeDecelMps2, what a hanging body takes without swinging up far)
// to a sprung stop kStopMarginM short of the bottom post. JUMP OFF throws
// the body on as the hands open (the swing's launch); DROP lets go.
//
// The trolley and the block stay where the ride left them: a ridden line has
// no trolley at its top, and is not ridden again until a death or a load
// (restore()) parks the trolley and sets the block back. They are the sim's
// positions, drawn by the presentation; the body never reaches them but by
// the hands (parked over a jump's height, ridden over the head).
//
// A line is built only where it fits: a floor under each post, and the cable
// with a hanging body's room under it clear of everything already built,
// swept along its whole length. Its posts, stand and cable are one static
// kit body: solid, drawn, and kept clear of by the loose stock's spots.

#include "sim/mechanism_kit.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cstdint>
#include <vector>

namespace scraperx::sim {

class Zipline final {
public:
    // Static kit bodies, one a line, from kFirstLineEntity.
    static constexpr std::uint64_t kFirstLineEntity = 1800;
    static constexpr int kMaxLines = 24;
    static constexpr float kCableRadius = 0.015F;
    // The top anchor over its floor and the stand under it (CHOSEN: the
    // parked bar 1.1 m over a stander's hands on the stand, where a running
    // jump from it reaches, high or low; over a band's rail the hanging
    // body's feet pass clear); the bottom anchor over its floor (CHOSEN: a
    // hanging body's feet clear the band's rail on the way in, and drop a
    // metre to the floor let go of).
    static constexpr float kTopPostM = 4.8F;
    static constexpr float kStandM = 1.2F;
    static constexpr float kRampRunM = 2.4F;
    static constexpr float kBottomPostM = 3.4F;
    // The trolley: its bar under the cable (the bracket, the hanger and the
    // bar's own height), the hands' reach for the bar, and its park down the
    // line from the top anchor (on the stop there).
    static constexpr float kBarDropM = 0.28F;
    static constexpr float kCatchReachM = 0.45F;
    // The hands take the bar rising to it or coming down onto it, falling
    // no faster than this (a jump's fall from 0.6 m over it).
    static constexpr float kCatchFallMps = 3.5F;
    static constexpr float kStopMarginM = 0.8F;
    static constexpr float kParkM = kStopMarginM;
    // The sheaves' rolling drag on the cable (CHOSEN: steel on steel, a
    // rider's weight on two wheels); the brake block's deceleration of a
    // rider (CHOSEN: half a g, a 26 degree swing-up on the arms), its travel
    // and clamp drag worked out per line from its slope so the ride ends at
    // the bottom post; the rider's mass the block is sized for.
    static constexpr float kTrolleyDragN = 12.0F;
    static constexpr float kBrakeDecelMps2 = 4.9F;
    static constexpr float kRiderMassKg = 84.0F;
    // The hanging body's room under the cable, swept along the line when it
    // is built: half a metre either side, from just over the cable to
    // kRoomBelowM under it.
    static constexpr float kRoomBelowM = 2.6F;

    struct Line {
        JPH::RVec3 top;        // the top anchor
        JPH::RVec3 bottom;     // the bottom anchor
        JPH::Vec3 direction;   // unit, top to bottom
        float length = 0.0F;
        float brake_m = 0.0F;    // the brake block's travel, its rest to the stop
        float brake_n = 0.0F;    // its clamp's drag on the cable
        float trolley_m = 0.0F;  // the trolley's place down the line from the top anchor
        float block_m = 0.0F;    // the block's
        bool parked = true;      // the trolley on its hook at the top
        kit::BodyIndex body{};
    };
    struct State {
        bool riding = false;
        int line = -1;
        float along_m = 0.0F;      // the trolley's place down the line from the top anchor
        float speed_mps = 0.0F;    // down the line (+)
        bool braking = false;      // pushing the brake block
        std::uint64_t rides = 0;
    };

    Zipline(JPH::PhysicsSystem &physics, kit::Kit &kit, JPH::BodyID player);
    ~Zipline();
    Zipline(const Zipline &) = delete;
    Zipline &operator=(const Zipline &) = delete;

    // A line from a top post standing on top_floor to a bottom post on
    // bottom_floor, built only if it fits (see above): false where it does
    // not, and nothing is built.
    bool add_line(JPH::RVec3 top_floor, JPH::RVec3 bottom_floor);
    // The line whose parked trolley's bar is within `reach` of `hands` (the
    // caller holds the bar over the head: kSwingMinHandRise): its index and
    // the bar, or -1.
    [[nodiscard]] int at_hand(JPH::RVec3 hands, float reach, JPH::RVec3 &bar) const;
    // The hands, at `hands_offset` from the rider's centre, take `line`'s
    // parked trolley off its hook: the line's body, which the hands hold
    // through the trolley. The bar is at bar() from then on.
    JPH::BodyID clip(int line, JPH::Vec3 hands_offset);
    [[nodiscard]] JPH::RVec3 bar() const;
    // The hands open; the trolley stays where it is.
    void unclip();
    // After the physics step: where the trolley is and how fast; the brake.
    void post_step(float delta_seconds);
    // After a death or a load: the hands open, every trolley parked and every
    // block at its rest.
    void restore();

    [[nodiscard]] bool riding() const noexcept { return state_.riding; }
    [[nodiscard]] const State &state() const noexcept { return state_; }
    [[nodiscard]] const std::vector<Line> &lines() const noexcept { return lines_; }

private:
    [[nodiscard]] bool floor_under(JPH::RVec3 at) const;
    [[nodiscard]] bool room_along(JPH::RVec3 top, JPH::RVec3 bottom) const;

    JPH::PhysicsSystem &physics_;
    kit::Kit &kit_;
    JPH::BodyID player_;
    std::vector<Line> lines_;
    JPH::Ref<JPH::SliderConstraint> slider_;
    JPH::Vec3 hands_offset_ = JPH::Vec3::sZero();
    State state_{};
};

} // namespace scraperx::sim
