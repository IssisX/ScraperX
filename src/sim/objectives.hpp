#pragma once

// J-129 (#455): the objective core — B5 item 1 of
// 03_EXECUTION/PLANNING/FINISH_LINE_AND_REASONS_TO_PLAY.md. A table of
// mission objectives. Each objective is a check that only reads the world —
// a body in a zone, a machine state, her state — with a done flag. A done
// flag never clears once set. The core writes nothing to the world: no
// forces, no body writes, no velocity writes.
//
// Call update() once per fixed step, with the world after the step; a rise
// it never sees cannot be judged ridden. The tick hook and the save fields
// land after F8 (J-116 #438): one save version bump, not two, so the hook
// and the Saved wiring are not in this change.

#include <cstddef>
#include <cstdint>

namespace scraperx::sim {

class Simulation;

class Objectives final {
public:
    // The missions, in B3-table order. Bit N of the save word is objective N.
    enum class Id : std::uint8_t {
        Mission1ClockIn = 0,       // S1 run and ridden to 22 m
        Mission7BringPlantUp = 1,  // float ram, chiller, accumulator, in order
        Mission8TopOut = 2,        // summit ending Home and the searchlight lit
    };
    static constexpr std::size_t kObjectiveCount = 3;

    Objectives() = default;

    // One tick of world reading. Reads only: stack_state, wet_state, snapshot.
    void update(const Simulation &simulation) noexcept;

    [[nodiscard]] bool done(Id id) const noexcept { return done_[static_cast<std::size_t>(id)]; }
    [[nodiscard]] std::uint32_t done_count() const noexcept;

    // The save seam, for F8's bump: one done bit per objective, bit N is
    // objective N. restore_bits only ever sets flags, never clears them, so
    // a load cannot unfinish a mission; an old save restores zero bits and
    // loads with every flag false.
    [[nodiscard]] std::uint32_t save_bits() const noexcept;
    void restore_bits(std::uint32_t bits) noexcept;

private:
    // Mission 1, "Clock in": the cage's rise with the rider aboard.
    bool s1_rising_ = false;  // the cage left grade and has not come back
    bool s1_rode_ = false;    // aboard for the whole rise so far
    // Mission 7, "Bring the plant up": the ordered stages.
    std::uint8_t plant_stage_ = 0;  // 0 ram, 1 chiller, 2 accumulator, 3 done
    std::uint32_t e_still_ticks_ = 0;  // E's cab still at the top, in ticks

    bool done_[kObjectiveCount] = {};
};

}  // namespace scraperx::sim
