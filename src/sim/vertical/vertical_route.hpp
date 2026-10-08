#pragma once
// Where the owner's vertical machines stand on the route, and how the player
// works them. Each machine runs on its own finite motor; the player's control
// is the machine's own: from its deck, Action sends the deck to the other end;
// from its lower or upper receiver, Action calls the deck to that receiver.
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
    // entity the player stands on (0 when none).
    void pre_step(float dt, bool action, std::uint64_t support);

    [[nodiscard]] Here at(std::uint64_t support) const;
    [[nodiscard]] std::size_t size() const noexcept { return placed_.size(); }
    [[nodiscard]] const Machine &machine(std::size_t i) const { return *placed_[i].machine; }
    [[nodiscard]] float travel(std::size_t i) const;
    [[nodiscard]] float deck_speed(std::size_t i) const;

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

private:
    struct Placed {
        std::unique_ptr<Machine> machine;
        BodyIndex deck, entry, exit;
        float entry_y = 0.0F, exit_y = 0.0F;
    };

    void place(const char *id, Placement placement, EntityRange statics, EntityRange dynamics);

    kit::Kit &kit_;
    PhysicsSystem &world_;
    std::vector<Placed> placed_;
    std::vector<float> targets_;
};

} // namespace scraperx::sim::vertical
