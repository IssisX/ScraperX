#pragma once
// Where the owner's vertical machines stand on the route, and how the player
// works them. Each machine runs on its own finite motor; the player's control
// is the machine's own: from its deck, Action sends the deck to the other end;
// from its lower or upper receiver, Action calls the deck to that receiver.
// Several are worked by the body instead (see each below).
#include "sim/vertical/vertical_machine.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace scraperx::sim::vertical {

class Route final {
public:
    enum class Role { None, Deck, Entry, Exit };

    struct Here {
        int machine = -1;      // index into the route, -1 when on none
        Role role = Role::None;
        float travel = 0.0F;   // the deck between its receivers: 0 entry, 1 exit
        float target = 0.0F;   // where it was last sent
        bool moving = false;   // the deck moving faster than 0.05 m/s
    };

    struct State {
        std::vector<ControlState> controls;
        std::vector<float> targets;
    };

    Route(kit::Kit &kit, PhysicsSystem &world);
    ~Route();
    Route(const Route &) = delete;
    Route &operator=(const Route &) = delete;

    // Write phase, before the one PhysicsSystem::Update. `support` is the
    // entity the player stands on (0 when none); `haul`, the player's hands
    // held on a hand line (the pitman lift's).
    void pre_step(float dt, bool action, std::uint64_t support, bool haul = false);

    // Build-order tail: bodies added after every other kit body so an older
    // save's kit lists still line up (the pitman's deck counterweight).
    void build_late();

    [[nodiscard]] Here at(std::uint64_t support) const;
    [[nodiscard]] std::size_t size() const noexcept { return placed_.size(); }
    [[nodiscard]] const Machine &machine(std::size_t i) const { return *placed_[i].machine; }
    [[nodiscard]] float travel(std::size_t i) const;
    [[nodiscard]] float deck_speed(std::size_t i) const;
    // The luffing derrick (-1 when not placed): worked by the load on its
    // cradle, not Action. Its iron pigs aboard now, and whether its motor is
    // holding the cradle down (called down, until a rider stands on it).
    [[nodiscard]] int derrick() const noexcept { return derrick_; }
    [[nodiscard]] int derrick_iron_aboard() const;
    [[nodiscard]] bool derrick_holding() const noexcept;
    // The traction tram (-1 when not placed): a shuttle on its own, not
    // Action. It waits at each end, then leaves for the other; seconds until
    // it leaves, or -1 while it runs.
    [[nodiscard]] int tram() const noexcept { return tram_; }
    [[nodiscard]] float tram_departs_s() const noexcept;
    // Its waits (90 Hz ticks, CHOSEN): long enough at the foot to walk on
    // from the gangway, at the head to step off; not so long that the
    // receiver can be dawdled on.
    static constexpr int kTramFootWaitTicks = 540;
    static constexpr int kTramHeadWaitTicks = 360;
    // The slab incline (-1 when not placed), its state, and the friction its
    // brake treadle presses on the rail now.
    enum class InclineState { Armed = 0, Released = 1, Resetting = 2 };
    [[nodiscard]] int incline() const noexcept { return incline_; }
    [[nodiscard]] InclineState incline_state() const noexcept;
    [[nodiscard]] float incline_brake_n() const noexcept { return incline_brake_n_; }
    // The brake treadle's full friction (CHOSEN; see slab_incline.cpp) and
    // the trip plate's angle past which the pin is out (rad).
    static constexpr float kInclineBrakeFullN = 24000.0F;
    static constexpr float kInclineTripAngle = .03F;
    // The fraction of the treadle's travel at which the linkage presses the
    // shoe fully: short of its stop, so a rider standing on it holds full
    // friction.
    static constexpr float kInclineBrakeTakeUp = .8F;
    // The slab's fall in its tower: the trolley's travel along the incline
    // (30 m at 60 degrees).
    static constexpr float kInclineSlabTravel = 34.641F;

    // The pitman lift (-1 when not placed): worked by the rider's own hands.
    // A counterweight on a rope over its head balances the deck, the rod's
    // deck half and half a rider, so a rider hauling the hand line from its
    // deck lifts only the other half of their own weight: the line is a gun
    // tackle (one end on the deck), so each newton at the hands lifts two.
    // Let go, the brake on the deck's guide holds it where it is. From the
    // lower receiver the same hands haul it down (1:1, the call line); from
    // the upper receiver they free the brake and the empty deck, lighter than
    // its counterweight, rises governed.
    [[nodiscard]] int pitman() const noexcept { return pitman_; }
    [[nodiscard]] bool pitman_hauling() const noexcept { return pitman_haul_n_ > 0.0F; }
    // The stone wheel's stone: in its hopper, and in the skip over it (kg).
    // The stone wheel's reload skip, gate and handle: built after every other
    // body (Simulation calls it last), so saves written before it line up.
    void build_wheel_reload();
    [[nodiscard]] float wheel_hopper_kg() const noexcept;
    [[nodiscard]] float wheel_skip_kg() const noexcept;
    [[nodiscard]] float pitman_haul_n() const noexcept { return pitman_haul_n_; }
    // The hands: at most kHaulMaxN, falling to nothing at kHaulLineMps of
    // line through them (a human pull's force-speed limit, CHOSEN: a strong
    // two-handed haul, about 2.2 m/s of line hand over hand at its fastest).
    static constexpr float kHaulMaxN = 700.0F;
    static constexpr float kHaulLineMps = 2.2F;
    // The counterweight: deck 1,200 + rod's half 425 + half a rider 42.5 kg.
    static constexpr float kPitmanCounterweightKg = 1667.5F;
    static constexpr std::uint64_t kPitmanCounterweightEntity = 2975;
    // The stone wheel's reload (built late): the skip over the hopper (fixed),
    // its gate, and the trip line's handle at the upper receiver.
    static constexpr std::uint64_t kWheelSkipEntity = 2976;
    static constexpr std::uint64_t kWheelSkipGateEntity = 2978;
    static constexpr std::uint64_t kWheelReloadHandleEntity = 2979;
    static constexpr float kWheelSkipKg = 8000.0F;
    // The brake on the deck's guide, and the governed speed the empty deck
    // rises at when freed from the upper receiver.
    static constexpr float kPitmanBrakeN = 30000.0F;
    static constexpr float kPitmanGovernedMps = .6F;
    static constexpr float kPitmanGuideFrictionN = 150.0F;

    // Restore the Kit first, then this.
    [[nodiscard]] State state() const;
    void restore(const State &state);

    // The cascade mast in the well's west half, beside the 264 ring's west
    // band: its lower receiver and deck level with the band (264.25 m).
    static constexpr std::uint64_t kCascadeStaticFirst = 1962;   // frame, entry, exit
    static constexpr std::uint64_t kCascadeDynamicFirst = 2920;  // two stages, deck
    // The pitman lift outside the west face, its crank in a plane 0.9 m clear
    // of the 308 band's outer edge: lower receiver at 291.25 m, joined to the
    // mast's upper receiver by a walkway across the face; upper at 314.89 m.
    static constexpr std::uint64_t kPitmanStaticFirst = 1965;   // frame, entry, exit
    static constexpr std::uint64_t kPitmanDynamicFirst = 2923;  // crank, deck, rod
    static constexpr std::uint64_t kMastPitmanWalkway = 1968;
    // The barrel helix west of the pitman's upper receiver: its lower receiver
    // abuts that one at 314.89 m; its upper, 24 m higher at 338.89 m, opens on
    // a walkway east across the west face to TP-340's edge, 1.36 m below the
    // plate's top: a mantle onto it.
    static constexpr std::uint64_t kHelixStaticFirst = 1969;   // frame, entry, exit
    static constexpr std::uint64_t kHelixDynamicFirst = 2926;  // barrel, deck, roller
    static constexpr std::uint64_t kHelixPlateWalkway = 1972;
    // From the 750 deck: the crown gondola east of it, its lower receiver
    // against the deck's east parapet at 750.1 m (a vault over the parapet),
    // its cabin carried through a half turn of a 28 m crown to 778.09 m.
    static constexpr std::uint64_t kCrownStaticFirst = 1973;   // frame, entry, exit
    static constexpr std::uint64_t kCrownDynamicFirst = 2929;  // crown, cabin
    // Then the luffing derrick, its mast 41 m north of the crown, its boom
    // pointing south: lower receiver against the crown's upper one at
    // 778.09 m, its cradle luffed up and in to 802.64 m.
    static constexpr std::uint64_t kDerrickStaticFirst = 1976;   // frame, entry, exit
    static constexpr std::uint64_t kDerrickDynamicFirst = 2956;  // boom, cradle, six iron pigs, ballast
    // The owner's slab incline on the south face west of S1, from the yard
    // to deck 3: a ramp up to its boarding platform (3.4 m), the trolley 30 m
    // up the 60-degree incline, and a plate from its head onto deck 3.
    static constexpr std::uint64_t kInclineStaticFirst = 1979;   // frame, entry, exit
    static constexpr std::uint64_t kInclineDynamicFirst = 2952;  // trolley, trip plate, brake treadle, slab
    static constexpr std::uint64_t kInclineRamp = 1982;
    static constexpr std::uint64_t kInclineHeadPlate = 1983;
    // The owner's stone wheel in the yard south of S1, deck 2 to deck 4: a
    // ramp down from deck 2's edge to its lower receiver (20.63 m); walk out
    // on the feed plank to fill the top bucket and hold the wheel, walk back
    // and board the bucket at the receiver; the stone carries it over the top
    // to 46.37 m, and a ramp runs down onto deck 4 (44 m).
    static constexpr std::uint64_t kWheelStaticFirst = 1984;    // frame, entry, exit, hopper
    static constexpr std::uint64_t kWheelDynamicFirst = 2936;   // wheel, striker, gate, 9 buckets
    static constexpr std::uint64_t kWheelLowRamp = 1988;
    static constexpr std::uint64_t kWheelHighRamp = 1989;
    // The traction tram beside the east bridge (64.7 m), its track rising
    // west: off the bridge's open east end onto a landing, down a gangway to
    // its lower receiver (64.0 m); its carrier driven 26 m up and 44 m west to
    // its upper receiver (90.0 m), and a plate on west to deck 8's east edge,
    // 2 m over the deck.
    static constexpr std::uint64_t kTramStaticFirst = 1990;    // track, entry, exit, two stops
    static constexpr std::uint64_t kTramDynamicFirst = 2948;   // carrier, two wheels
    static constexpr std::uint64_t kTramGangway = 1995;
    static constexpr std::uint64_t kTramLanding = 1996;
    static constexpr std::uint64_t kTramTopPlate = 1997;

private:
    struct Placed {
        std::unique_ptr<Machine> machine;
        BodyIndex deck, entry, exit;
        // Bodies on the deck that count as standing on it (plates).
        std::vector<BodyIndex> deck_parts;
        float entry_y = 0.0F, exit_y = 0.0F;
    };

    void place(const char *id, Placement placement, EntityRange statics, EntityRange dynamics);
    void pitman_step(bool haul, std::uint64_t support);

    kit::Kit &kit_;
    PhysicsSystem &world_;
    std::vector<Placed> placed_;
    std::vector<float> targets_;
    int wheel_ = -1;   // the stone wheel: worked by its feed plank, not Action
    // The slab incline: worked by its plates, not Action from its deck. Its
    // state lives in targets_ (checkpointed): see InclineState.
    int incline_ = -1;
    // The tram: its state lives in targets_ as end + 2 * ticks waited there
    // (checkpointed): the end it was last sent to and how long it has stood.
    int tram_ = -1;
    // The derrick: 0 free (its load decides), 1 held down by its motor;
    // in targets_ (checkpointed).
    int derrick_ = -1;
    float incline_brake_n_ = 0.0F;
    int pitman_ = -1;
    BodyIndex pitman_counterweight_;
    float pitman_haul_n_ = 0.0F;
    kit::BinIndex wheel_skip_bin_{};
};

} // namespace scraperx::sim::vertical
