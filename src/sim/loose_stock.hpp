#pragma once

// Loose stock: the things lying about the tower that are not holding it up
// (the owner, 2026-10-17: "if SOME objects are moveable, MOST objects should
// be moveable"). Crates, drums, pallets, timber, steel beams, plate, pipe and
// shipping containers: each a real body with no rule of its own. The player
// carries the light ones (the carry's grip holds up to about 70 kg), shoves
// the middling ones by walking into them and stacks them to reach a ledge;
// the heaviest wait for a machine big enough to move them. What they do
// comes from that. The tower's frame, decks, stairs and machines stay put.
//
// They sleep once at rest, so a yard full of them costs nothing while it is
// left alone; anything that touches one wakes it. They are saved and
// restored with the machines like every kit body.
//
// A sudden blow to a piece (a change of velocity of kImpactMps or more in one
// tick) is a crash, reported with its loudness for whoever listens (the
// Matron hears crashes on her deck).

#include "sim/mechanism_kit.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/PhysicsSystem.h>

#include <cstdint>
#include <vector>

namespace scraperx::sim {

class LooseStock final {
public:
    // Kit entities: piece i of the table is kFirstEntity + i.
    static constexpr std::uint64_t kFirstEntity = kit::kStockEntityBase;

    enum class Kind : std::uint8_t {
        Crate = 0,          // 0.96 x 0.88 x 0.96 m timber crate, 160 kg: shoved
        IngotDolly = 1,     // a box of ingots on a rolling dolly, 420 kg
        TimberBeam = 2,     // 4 m baulk of timber, 70 kg
        Drum = 3,           // an empty oil drum on its end, 55 kg: carried
        Pallet = 4,         // 1.2 x 1.0 m, 22 kg: carried
        Container = 5,      // a 40 ft shipping container, empty, 3,750 kg
        SteelBeam = 6,      // a 10 m steel beam, 600 kg
        Sleeper = 7,        // a 3 m hardwood sleeper, 90 kg
        PipeBearer = 8,     // a 2.4 m steel cradle with a post at each end, 250 kg
        PlateStack = 9,     // a 3 x 1.5 m stack of steel plate, 8 t
        PlateSlab = 10,     // a 2.4 x 1.2 m slab of plate, 2 t
        Pipe = 11,          // an 8 m steel pipe, 0.6 m across, 700 kg
    };
    static constexpr int kKinds = 12;

    struct Placement {
        Kind kind;
        std::uint8_t variant;  // its paint, for the presentation (no physics)
        float x, y, z;         // where its underside sits, its centre over (x, z)
        float yaw;             // about +y
    };

    // A sudden blow of this much change of velocity in one tick is a crash,
    // heard as loud as kNoisePerMps per m/s above kQuietMps.
    static constexpr float kImpactMps = 2.0F;
    static constexpr float kQuietMps = 0.5F;
    static constexpr float kNoisePerMps = 0.3F;

    struct Crash {
        bool heard = false;
        float x = 0.0F, y = 0.0F, z = 0.0F;
        float loudness = 0.0F;
    };

    LooseStock(JPH::PhysicsSystem &physics, kit::Kit &kit);

    // After the physics step: the loudest blow among the pieces this tick.
    void post_step();
    // The loudest blow since it was last taken (taken once).
    [[nodiscard]] Crash take_crash() noexcept {
        const Crash crash = crash_;
        crash_ = {};
        return crash;
    }
    // After a restore puts the pieces back: their velocities are taken as they
    // now are, so the jump is not heard as a blow.
    void resync();

    [[nodiscard]] int count() const noexcept { return static_cast<int>(pieces_.size()); }
    [[nodiscard]] kit::BodyIndex body(int index) const noexcept {
        return pieces_[static_cast<std::size_t>(index)];
    }

    // The pieces as placed, in order (also the order of their entities).
    [[nodiscard]] static const std::vector<Placement> &placements();
    // The Matron's deck's own seven, first in the table.
    static constexpr int kDeckStackLow = 0;      // two crates side by side and one on top
    static constexpr int kDeckCrateNorth = 3;    // a loose crate north of the catwalk's east end
    static constexpr int kDeckCrateSouth = 4;
    static constexpr int kDeckIngotDolly = 5;
    static constexpr int kDeckBeam = 6;

private:
    JPH::PhysicsSystem &physics_;
    kit::Kit &kit_;
    std::vector<kit::BodyIndex> pieces_;
    std::vector<JPH::Vec3> last_velocity_;
    Crash crash_{};
};

} // namespace scraperx::sim
