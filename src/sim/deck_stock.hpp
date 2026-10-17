#pragma once

// Loose stock on the 154 m deck, the Matron's deck (the owner, 2026-10-16:
// boxes, beams and containers to move about and use against her). Nothing
// here has a rule of its own: each piece is a kit body the player shoves by
// walking into it, and what it does comes from that.
//  - three timber crates by the south band's rail, two side by side and one on
//    top like a step: climb onto the low one and shove the top one off, and
//    it crashes down; she hears a crash as she hears the player (so does a
//    crate rammed hard into a rail);
//  - two loose crates near the catwalk's east end, to push into her way;
//  - a steel box of ingots on a rolling dolly: pushed out onto the rotten
//    catwalk it brings a plate down under itself (it is not her, so it does
//    not kill her; it can break the way across);
//  - a timber beam on the south band.
// Each is saved and restored with the machines, like every kit body.

#include "sim/mechanism_kit.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/PhysicsSystem.h>

#include <array>
#include <cstdint>

namespace scraperx::sim {

class DeckStock final {
public:
    static constexpr std::uint64_t kFirstEntity = 2982;
    static constexpr int kPieces = 7;
    static constexpr float kDeckTop = 154.0F;
    // A change of velocity this large in one tick is a blow, heard as loud
    // as kNoisePerMps per m/s above kQuietMps: a crate toppled off the
    // stack's top (about 2.4 m/s in a tick) is heard some 30 m off, a crate
    // shoved slowly into a rail not at all.
    static constexpr float kImpactMps = 2.0F;
    static constexpr float kQuietMps = 0.5F;
    static constexpr float kNoisePerMps = 0.3F;
    static constexpr float kCrateMassKg = 160.0F;
    static constexpr float kIngotBoxMassKg = 420.0F;

    struct Noise {
        bool heard = false;
        float x = 0.0F, y = 0.0F, z = 0.0F;
        float loudness = 0.0F;
    };

    DeckStock(JPH::PhysicsSystem &physics, kit::Kit &kit);

    // After the physics step: the loudest blow among the pieces this tick.
    void post_step();
    // The loudest blow since it was last taken (taken once).
    [[nodiscard]] Noise take_noise() noexcept {
        const Noise noise = noise_;
        noise_ = {};
        return noise;
    }
    // After a restore puts the pieces back: their velocities are taken as
    // they now are, so the jump is not heard as a blow.
    void resync();
    [[nodiscard]] kit::BodyIndex piece(int index) const noexcept {
        return pieces_[static_cast<std::size_t>(index)];
    }

private:
    JPH::PhysicsSystem &physics_;
    kit::Kit &kit_;
    std::array<kit::BodyIndex, kPieces> pieces_{};
    std::array<JPH::Vec3, kPieces> last_velocity_{};
    Noise noise_{};
};

} // namespace scraperx::sim
