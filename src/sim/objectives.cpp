// J-129 (#455): the objective core. See objectives.hpp.

#include "sim/objectives.hpp"

#include <cmath>

#include "sim/simulation.hpp"
#include "sim/summit.hpp"

namespace scraperx::sim {
namespace {

// The bars are the ones the stage tests already use, not new numbers:
//   S1 top: the cage's travel is 21.80 m (band_stack.cpp, kCageTravel) and
//     the stack test calls the top s1_cage_travel >= kS1Travel - 0.01.
//   D done: d_platform_travel >= 35.95 (sim_tests_wet.cpp), of 36.0 m
//     (band_wet_isolation.cpp, kDTravel).
//   E done: the cab past 40.0 m and still (|speed| < 0.05 m/s) for 2 s
//     (sim_tests_wet.cpp, wait_wet_e_top), of 46.5 m (kETravel).
//   F done: f_platform_travel >= 41.95 (sim_tests_wet.cpp), of 42.0 m
//     (band_wet_isolation.cpp, kFTravel).
constexpr double kS1TopTravel = 21.79;
constexpr double kDPlatformTop = 35.95;
constexpr double kECabTop = 40.0;
constexpr double kECabStillSpeed = 0.05;
constexpr std::uint32_t kEStillTicks = 2 * Simulation::kTickRateHz;
constexpr double kFPlatformTop = 41.95;
constexpr double kCageAtGrade = 0.05;

// On the cage's floor or on its scale plate, standing, not hanging or falling.
bool aboard_s1_cage(const Simulation &simulation) noexcept {
    const auto snapshot = simulation.snapshot();
    return snapshot.player_grounded &&
           (snapshot.support_entity_id == Simulation::kStackS1CageEntityId ||
            snapshot.support_entity_id == Simulation::kStackS1PlateEntityId);
}

}  // namespace

void Objectives::update(const Simulation &simulation) noexcept {
    if (!done_[static_cast<std::size_t>(Id::Mission1ClockIn)]) {
        // Mission 1, "Clock in": the cage's rise with the rider aboard.
        // The ride is armed when the cage first leaves grade and stays
        // aboard only while the rider stands on the cage or its plate on
        // every tick of the rise, so an empty cage sent up, or a rider who
        // steps off halfway, never counts; walking onto the cage parked at
        // the top never counts either.
        const double travel = simulation.stack_state().s1_cage_travel;
        if (travel <= kCageAtGrade) {
            s1_rising_ = false;
            s1_rode_ = false;
        } else if (!s1_rising_) {
            s1_rising_ = true;
            s1_rode_ = aboard_s1_cage(simulation);
        } else {
            s1_rode_ = s1_rode_ && aboard_s1_cage(simulation);
        }
        if (s1_rising_ && s1_rode_ && travel >= kS1TopTravel) {
            done_[static_cast<std::size_t>(Id::Mission1ClockIn)] = true;
        }
    }

    if (!done_[static_cast<std::size_t>(Id::Mission7BringPlantUp)]) {
        // Mission 7, "Bring the plant up": the float ram, the chiller, the
        // accumulator, in order. A stage already done when its turn comes
        // advances at once, so a save and reload mid-plant recovers the
        // sequence from the machines' own states instead of restarting it.
        const auto wet = simulation.wet_state();
        switch (plant_stage_) {
            case 0:
                if (wet.d_platform_travel >= kDPlatformTop) {
                    plant_stage_ = 1;
                }
                break;
            case 1:
                if (wet.e_cab_travel > kECabTop && std::abs(wet.e_cab_speed) < kECabStillSpeed) {
                    if (++e_still_ticks_ >= kEStillTicks) {
                        plant_stage_ = 2;
                        e_still_ticks_ = 0;
                    }
                } else {
                    e_still_ticks_ = 0;
                }
                break;
            case 2:
                if (wet.f_platform_travel >= kFPlatformTop) {
                    plant_stage_ = 3;
                    done_[static_cast<std::size_t>(Id::Mission7BringPlantUp)] = true;
                }
                break;
            default:
                break;
        }
    }

    if (!done_[static_cast<std::size_t>(Id::Mission8TopOut)]) {
        // Mission 8, "Top out": the summit ending reached Home and the
        // searchlight is lit. Both facts latch in the world — the ending
        // only advances, the lamp never unlights — so the flag fires on the
        // first tick that sees them together and never clears.
        const auto summit = simulation.summit_state();
        if (summit.ending == static_cast<std::uint8_t>(Summit::Ending::Home) && summit.lit) {
            done_[static_cast<std::size_t>(Id::Mission8TopOut)] = true;
        }
    }
}

std::uint32_t Objectives::done_count() const noexcept {
    std::uint32_t count = 0;
    for (const bool d : done_) {
        if (d) {
            ++count;
        }
    }
    return count;
}

std::uint32_t Objectives::save_bits() const noexcept {
    std::uint32_t bits = 0;
    for (std::size_t i = 0; i < kObjectiveCount; ++i) {
        if (done_[i]) {
            bits |= 1u << static_cast<std::uint32_t>(i);
        }
    }
    return bits;
}

void Objectives::restore_bits(std::uint32_t bits) noexcept {
    for (std::size_t i = 0; i < kObjectiveCount; ++i) {
        if ((bits & (1u << static_cast<std::uint32_t>(i))) != 0) {
            done_[i] = true;
        }
    }
}

}  // namespace scraperx::sim
