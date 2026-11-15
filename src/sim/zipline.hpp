#pragma once

// Ziplines (Mirror's Edge's; the owner, 2026-10-17). A taut steel cable from
// a post on one deck down across the well to a lower post on the far band of
// the deck below, or from the lowest deck down to the yard. On the cable
// rides a trolley: two sheaves in a bracket over the cable, a bar hung
// kBarDropM under it that the hands hold. Nothing of the rider's touches the
// cable: the sheaves roll on it (a hand on a long steel cable at speed burns
// off; the owner, 2026-10-17). The trolley parks on a hook kParkM down the
// line from the top post's bracket, over a stand (kStandM high, up a ramp
// along the band at its side): a standing jump from the stand puts its bar
// in the hands' reach, the hands take the bar, the body's weight pulls the
// trolley off the hook, and it rides down under that weight. The slope drives it; the
// sheaves resist a little (kTrolleyDragN, rolling). The stick held back
// squeezes the trolley's hand brake on the cable (up to kHandBrakeMaxN): the
// rider sets the speed. Over the line's last kBufferM the trolley runs into
// a buffer block, a clamp on the cable, and pushes it down the cable against
// its clamp's drag (brake_n: kBufferDecelMps2 on a rider, over the slope's
// drive) to a sprung stop kStopMarginM short of the bottom post. Arrive
// braked (kSafeArrivalMps or under) and the buffer stops the trolley short
// of the stop; arrive faster and the stop's jolt is more than the hands can
// hold (the caller's grip, as on a ledge): they are torn off the bar and the
// body goes on at what speed it had left, into the bottom post. JUMP OFF throws the body on
// as the hands open (the swing's launch); DROP lets go.
//
// The trolley and the block stay where the ride left them: a ridden line has
// no trolley at its top, and is not ridden again until a death or a load
// (restore()) parks the trolley and sets the block back.
//
// A second rider, not the player, rides the same way (clip_other): the
// Matron, her hands on a parked trolley's bar, squeezing its brake herself.
// One rider a trolley: whoever takes it first takes it down. Or with no
// trolley (on_cable): her forearms hooked over the bare cable from the park
// down, the trolley left where it is, her own drag on the cable her brake. They are the sim's
// positions, drawn by the presentation; the body never reaches them but by
// the hands (parked over a jump's height, ridden over the head).
//
// A line is built only where it fits: a floor under each post, and the cable
// with a hanging body's room under it clear of everything already built,
// swept along its whole length. Its posts, stand and cable are one static
// kit body: solid, drawn, and kept clear of by the loose stock's spots.

#include "sim/mechanism_kit.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Constraints/PointConstraint.h>
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
    // rider's weight on two wheels); the hand brake's most (CHOSEN: about
    // 0.7 g net on the steepest line, and a rider held still on any); the
    // buffer's travel and its deceleration of a rider (CHOSEN: half a g, a 26
    // degree swing-up on the arms), its clamp drag worked out per line from
    // its slope; the fastest arrival the buffer stops short of the stop
    // (DERIVED: sqrt(2 * 4.9 * 3); what is left of a faster one meets the
    // sprung stop, whose damping jolt runs about 4 kN a metre a second, and a
    // few metres a second over tears the hands off: MEASURED, torn from 6.5
    // to 9 m/s in at the buffer on the yard line); the rider's mass the
    // buffer is sized for.
    static constexpr float kTrolleyDragN = 12.0F;
    static constexpr float kHandBrakeMaxN = 900.0F;
    static constexpr float kBufferM = 3.0F;
    static constexpr float kBufferDecelMps2 = 4.9F;
    static constexpr float kSafeArrivalMps = 5.4F;
    static constexpr float kRiderMassKg = 84.0F;
    // The hanging body's room under the cable, swept along the line when it
    // is built: half a metre either side, from just over the cable to
    // kRoomBelowM under it.
    static constexpr float kRoomBelowM = 2.6F;
    // The room around a machine, or anything the hands take hold of, that a
    // line's footings leave to the one working it.
    static constexpr float kWorkingRoomM = 2.0F;

    // The keeper pin (J-095 #413): one pin holds a line's cable. The pin is
    // a small steel body in the top bracket, carried as a handle; pulling it
    // out of its hole releases the cable's near end, and the cable becomes a
    // rope of segments (kCableMassPerM), hinged at its far end, that goes
    // slack and drapes. The free end falls; it does not rise.
    static constexpr std::uint64_t kPinEntityId = 2400;
    static constexpr std::uint64_t kFirstCableEntity = 2401;
    static constexpr int kMaxCableSegments = 30;
    // 30 mm steel wire rope, about 3.5 kg a metre (solid steel at that
    // section is 5.5 kg/m; a wire rope's fill is about 0.63 of solid).
    static constexpr float kCableMassPerM = 3.5F;
    static constexpr float kCableSegmentM = 2.0F;  // a segment's length
    static constexpr float kPinToleranceM = 0.15F; // the pull that clears the hole
    // The pin sits in a slot on the top post, low enough that a hand on the
    // deck or the stand can pull it: the pick-up's reach is 1.2 m from the
    // body's centre, and it is refused while hanging, so a pin at the
    // anchor (4.8 m up) could never be pulled by hand. The post's saddle
    // bracket is the anchor; the pin in the post below it is the keeper.
    static constexpr float kPinSeatHeightM = 2.5F; // the pin over the top floor
    // The pin's centre along the line's run from the top post: far enough
    // south that the pin (0.5 m long along the run) clears the post's
    // south face, or the solver ejects it out of the penetration at
    // build. (First seated at 0.1 m: the pin passed through the post and
    // fell to the floor below before the player reached the stand.)
    static constexpr float kPinRunM = 0.45F;
    // A transfer rope's most: her forearms over the cable, his hands on the
    // bar 0.28 m under it.
    static constexpr float kTransferRopeHerM = 0.15F;
    static constexpr float kTransferRopeHisM = 0.35F;

    struct Line {
        JPH::RVec3 top;        // the top anchor
        JPH::RVec3 bottom;     // the bottom anchor
        JPH::RVec3 top_floor;  // the floor under the top anchor
        JPH::RVec3 bottom_floor;
        JPH::RVec3 stand;      // the stand's top, under the parked bar
        JPH::RVec3 ramp_foot;  // the foot of the ramp up to it
        JPH::Vec3 direction;   // unit, top to bottom
        float length = 0.0F;
        float brake_m = 0.0F;    // the buffer's travel, its rest to the stop (kStopMarginM short of the end)
        float brake_n = 0.0F;    // its clamp's drag on the cable
        float trolley_m = 0.0F;  // the trolley's place down the line from the top anchor
        float block_m = 0.0F;    // the block's
        bool parked = true;      // the trolley on its hook at the top
        bool keeper_pin = false; // the cable is a chain held by the keeper pin (J-095)
        kit::BodyIndex body{};
        // The chain (a keeper-pin line only): its segments, the ropes
        // joining them end to end, the pin in its slot on the top post,
        // the catch holding the top segment while the pin is seated, and
        // the hinge at the bottom anchor the chain swings about. The
        // welds hold each segment to the static body while pinned (below).
        std::vector<kit::BodyIndex> cable_segments;
        std::vector<kit::RopeIndex> cable_ropes;
        std::vector<JPH::Ref<JPH::FixedConstraint>> cable_welds;
        kit::BodyIndex pin_body{};
        kit::CatchIndex pin_catch{};
        JPH::Ref<JPH::PointConstraint> bottom_hinge;
        int cable_count = 0;
        float cable_pitch = 0.0F;
        bool pin_pulled = false;       // the pin is out of its hole
        bool cable_transferred = false; // the riders moved onto the falling chain
    };
    struct State {
        bool riding = false;
        int line = -1;
        float along_m = 0.0F;      // the trolley's place down the line from the top anchor
        float speed_mps = 0.0F;    // down the line (+)
        bool braking = false;      // the hand brake on, or pushing the buffer
        float hand_brake = 0.0F;   // the squeeze, 0..1
        float hands_load_n = 0.0F; // what the bar puts on the hands, this step
        std::uint64_t rides = 0;
        bool rope_riding = false;  // on the falling chain, by a transfer rope (J-095)
        int rope_segment = -1;     // the chain segment the rope holds
    };

    // `reserved`: ground no line's footings may take (the places the player
    // is put down, the crew's lanes, a practice ground).
    Zipline(JPH::PhysicsSystem &physics, kit::Kit &kit, JPH::BodyID player, std::vector<JPH::AABox> reserved);
    ~Zipline();
    Zipline(const Zipline &) = delete;
    Zipline &operator=(const Zipline &) = delete;

    // A line from a top post standing on top_floor to a bottom post on
    // bottom_floor, built only if it fits (see above): false where it does
    // not, and nothing is built. `keeper_pin`: the cable is a chain held by
    // the keeper pin (J-095) instead of a member of the static body.
    bool add_line(JPH::RVec3 top_floor, JPH::RVec3 bottom_floor, bool keeper_pin = false);
    // Pull the keeper pin out of its hole (the player's pull, or a test's):
    // the cable's near end is released.
    void pull_pin(int line);
    // Whether the pin on `line` has been pulled.
    [[nodiscard]] bool pin_pulled(int line) const noexcept;
    // The cable's near end: the top anchor, or the top segment's centre
    // on a keeper-pin line (pinned or released).
    [[nodiscard]] JPH::RVec3 cable_top(int line) const;
    // The nearest point on the cable to `at`: the top anchor while pinned.
    [[nodiscard]] JPH::RVec3 cable_nearest(int line, JPH::RVec3 at) const;
    // Where the keeper pin sits (its seat while pinned).
    [[nodiscard]] JPH::RVec3 pin_position(int line) const;
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
    // The hand brake squeezed 0..1 (the stick held back).
    void set_hand_brake(float squeeze) noexcept;
    // The other rider: `body`'s hands, at `hands_offset` from its centre,
    // take `line`'s parked trolley (or, on_cable, the cable itself at the
    // park); let go; the brake it squeezes.
    void clip_other(int line, JPH::BodyID body, JPH::Vec3 hands_offset, bool on_cable);
    void unclip_other();
    void set_other_brake(float squeeze) noexcept;
    [[nodiscard]] const State &other_state() const noexcept { return other_state_; }
    // Where a line's parked bar hangs.
    [[nodiscard]] JPH::RVec3 parked_bar(int line) const;
    // After the physics step: where the trolley is and how fast, the brakes,
    // and the load on the hands.
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
    // Out of every machine's way: the cable with the rider's room under it
    // clear of where the kit's guides and levers carry their bodies, and the
    // footings (the posts, the stand and its ramp, each a box from the
    // floor) kWorkingRoomM clear of that and of everything the hands take
    // hold of, so a fixture never stands where a machine is worked, and off
    // the reserved ground.
    [[nodiscard]] bool clear_of_machines(JPH::RVec3 top, JPH::RVec3 bottom,
                                         const std::vector<JPH::AABox> &footings) const;

    JPH::PhysicsSystem &physics_;
    kit::Kit &kit_;
    JPH::BodyID player_;
    // The kit's travel envelopes, taken when the lines go up (the zipline is
    // built after every machine): a cage parked at its bottom deck still
    // rises through the shaft above it.
    std::vector<JPH::AABox> machine_travel_;
    std::vector<JPH::AABox> worked_;
    std::vector<JPH::AABox> reserved_;
    std::vector<Line> lines_;
    // A rider's hold on a line (see clip), its following of the trolley and
    // the brakes on it (see post_step), and its letting go.
    void attach(int index, JPH::BodyID body, JPH::Vec3 hands_offset, bool on_cable,
                JPH::Ref<JPH::SliderConstraint> &slider, State &state);
    void detach(JPH::Ref<JPH::SliderConstraint> &slider, State &state);
    void track(const JPH::Ref<JPH::SliderConstraint> &slider, State &state, JPH::BodyID body, bool on_cable,
               float delta_seconds, const JPH::Ref<kit::RopeConstraint> &rope);
    // The chain for a keeper-pin line (J-095): its segments end to end on
    // the line, the pin in its slot, the catch, and the bottom hinge.
    void build_chain(Line &line, int index);
    // The pin welds: each segment welded to the static body at its seat
    // while pinned, off on the release.
    void weld_chain(Line &line);
    void unweld_chain(Line &line);
    // The pin out: the riders on `line` move from the static slider onto
    // the falling chain, each by a transfer rope to its nearest segment.
    void transfer_riders(Line &line, int index);
    void transfer(int index, JPH::BodyID body, JPH::Vec3 hands_offset, float max_length,
                  JPH::Ref<JPH::SliderConstraint> &slider, State &state,
                  JPH::Ref<kit::RopeConstraint> &rope);
    void remove_rope(JPH::Ref<kit::RopeConstraint> &rope);
    // Where a chain segment sits on the line, and the nearest point on a
    // segment to `at`.
    [[nodiscard]] JPH::RVec3 segment_centre(const Line &line, int segment) const;
    [[nodiscard]] JPH::RVec3 segment_point(const Line &line, int segment, JPH::RVec3 at) const;
    JPH::Ref<JPH::SliderConstraint> slider_;
    JPH::Vec3 hands_offset_ = JPH::Vec3::sZero();
    State state_{};
    JPH::BodyID other_{};
    JPH::Ref<JPH::SliderConstraint> other_slider_;
    State other_state_{};
    bool other_on_cable_ = false;
    JPH::Vec3 other_hands_offset_ = JPH::Vec3::sZero();
    // The transfer ropes (J-095): the riders' holds on the falling chain.
    JPH::Ref<kit::RopeConstraint> rope_;
    JPH::Ref<kit::RopeConstraint> other_rope_;
};

} // namespace scraperx::sim
