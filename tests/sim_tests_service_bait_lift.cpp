// J-108 (#427), the bait lift off the 684 deck's east edge: two platforms on
// guides, one rope over the head. Nothing aboard, or him alone on A, it
// stays; her weight on A, with him on B, sinks A to the 662 deck and lifts B
// to C5's plant floor, where B settles on its dogs. Gravity on throughout;
// nothing here sets a speed: her weight and his are the only drive.

#include "sim_tests.hpp"

#include "sim/matron.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

constexpr double kDeck684 = 684.1;
constexpr double kPlantFloor = 706.1;
constexpr double kStroke = 21.9;              // A's underside onto the 662 deck
constexpr double kAX = -1.5, kBX = 0.35, kZ = -139.95;
constexpr double kPlayerAboveFeet = 0.9;      // his body's centre over his soles

void settle(Simulation &s, const double seconds) {
    for (double t = 0.0; t < seconds; t += Simulation::kFixedStepSeconds) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
}

void bait_stays() {
    // Nobody aboard: B, the heavier, sits on its stop.
    Simulation s(InitialSpawn::Deck684);
    settle(s, 10.0);
    const auto empty = s.service_state();
    std::cout << "  bait lift, nobody aboard: a=" << empty.bait_a_travel << " b=" << empty.bait_b_travel
              << " chain_kg=" << empty.bait_chain_kg << " rope_n=" << empty.bait_rope_tension << '\n';
    require(std::abs(empty.bait_a_travel) < 0.01 && std::abs(empty.bait_b_travel) < 0.01,
            "bait lift: with nobody aboard it stays");
    // Him alone on A: 30 kg over B, held by the guides' friction.
    s.debug_launch_player(Vector3{kAX, kDeck684 + kPlayerAboveFeet, kZ}, Vector3{0.0, 0.0, 0.0});
    // His landing on it jolts it a few centimetres; then the friction holds.
    settle(s, 5.0);
    const double jolt = s.service_state().bait_a_travel;
    settle(s, 10.0);
    const auto him = s.service_state();
    const auto at = s.snapshot().player_position;
    std::cout << "  bait lift, him alone on A: jolt=" << jolt << " a=" << him.bait_a_travel << " b=" << him.bait_b_travel << " him="
              << at.x << "," << at.y << "," << at.z << '\n';
    require(std::abs(at.x - kAX) < 0.6 && std::abs(at.y - kDeck684 - kPlayerAboveFeet) < 0.3,
            "bait lift: he stands on A");
    require(him.bait_a_travel < 0.1 && std::abs(him.bait_a_travel - jolt) < 0.002,
            "bait lift: with him alone on A it stays (a jolt as he lands, then held)");
    std::cout << "PASS scraperx_sim bait lift stays: nobody, and him alone on A\n";
}

void bait_ride() {
    // Him on B, waiting; her put down on A. Her hands are held off him for
    // the ride, so this is the ride alone (her stepping on is her own test).
    Simulation s(InitialSpawn::Deck684);
    settle(s, 1.0);
    s.debug_launch_player(Vector3{kBX + 0.6, kDeck684 + kPlayerAboveFeet, kZ}, Vector3{0.0, 0.0, 0.0});
    settle(s, 1.0);
    require(std::abs(s.service_state().bait_b_travel) < 0.01, "bait lift ride: him on B, it waits");
    s.debug_hold_matron_hands(120.0);
    s.debug_place_matron(Vector3{kAX, kDeck684, kZ});
    const double deaths = double(s.snapshot().death_count);
    double t = 0.0, peak_a = 0.0, peak_b = 0.0, arrive_a = -1.0, top_b = 0.0, t_arrive = -1.0;
    double prev_a = 0.0;
    for (; t < 90.0; t += Simulation::kFixedStepSeconds) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto st = s.service_state();
        const double va = (st.bait_a_travel - prev_a) / Simulation::kFixedStepSeconds;
        prev_a = st.bait_a_travel;
        peak_a = std::max(peak_a, va);
        top_b = std::max(top_b, st.bait_b_travel);
        if (arrive_a < 0.0 && st.bait_a_travel > kStroke - 0.05) {
            arrive_a = peak_a > 0.0 ? va : 0.0;
            t_arrive = t;
        }
        peak_b = std::max(peak_b, st.bait_b_peak_speed);
        if (t_arrive > 0.0 && t > t_arrive + 10.0) {
            break;
        }
    }
    const auto st = s.service_state();
    const auto him = s.snapshot();
    const auto her = s.matron_state();
    const double her_feet = her.position.y - double(scraperx::sim::Matron::kStandCentre);
    const double his_feet = him.player_position.y - kPlayerAboveFeet;
    std::cout << "  bait lift ride: arrive_t=" << t_arrive << " a=" << st.bait_a_travel << " b=" << st.bait_b_travel
              << " b_top=" << top_b << " a_arrive_mps=" << arrive_a << " a_peak_mps=" << peak_a
              << " b_peak_mps=" << peak_b << " chain_kg=" << st.bait_chain_kg << " his_feet=" << his_feet
              << " her_feet=" << her_feet << " her_alive=" << her.alive << " her_damage_ns=" << her.damage_ns
              << " her_knockdowns=" << her.knockdowns << " deaths=" << (double(him.death_count) - deaths)
              << " his_ankle_breaks=" << him.ankle_break_count << '\n';
    require(t_arrive > 0.0, "bait lift ride: her weight sinks A to the 662 deck");
    require(arrive_a <= 2.5, "bait lift ride: A meets the 662 deck no faster than the evaluator's band (2.2 m/s)");
    require(st.bait_b_travel >= kStroke - 0.05 && st.bait_b_travel <= kStroke + 0.5,
            "bait lift ride: B settles on its dogs at C5's plant floor");
    require(std::abs(his_feet - kPlantFloor) < 0.35, "bait lift ride: he is carried up to C5's plant floor");
    require(double(him.death_count) == deaths && him.ankle_break_count == 0, "bait lift ride: he rides it unhurt");
    require(her.alive && her.knockdowns == 0, "bait lift ride: she rides it down on her feet");
    std::cout << "PASS scraperx_sim bait lift ride: seconds=" << t_arrive << " a_arrive_mps=" << arrive_a
              << " b_floor=" << (kDeck684 + st.bait_b_travel) << '\n';
}
} // namespace

void run_service_bait_lift() {
    bait_stays();
    bait_ride();
}
SCRAPERX_GROUP("service_bait_lift", 335, run_service_bait_lift);
SCRAPERX_SLICE("service-bait-lift", run_service_bait_lift);
