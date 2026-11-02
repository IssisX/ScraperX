// The tower's machines she hears (gap 10, 03_EXECUTION/GAP_LIST.md): every
// moving part of each stage is a tracked body in the ImpactLedger, so a hard
// hit it takes is heard from its measured energy; each stage's rider part
// (cage, cab, platform, traveler) is also its running source, heard while it
// moves from its measured kinetic energy. A machine at rest draws nothing.
#include "sim/impacts.hpp"
#include "sim/matron.hpp"
#include "sim_tests.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

using scraperx::sim::ImpactLedger;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;

void run_machine_hearing() {
    // The running law: creep is silent; a run is quieter than the same energy
    // in one strike, and never louder than the yard crane's unloaded hoist.
    require(ImpactLedger::run_loudness(2000.0F, 0.05F) == 0.0F, "machine hearing: a creeping machine is silent");
    require(ImpactLedger::run_loudness(2000.0F, 1.0F) > 0.0F &&
                ImpactLedger::run_loudness(2000.0F, 1.0F) < ImpactLedger::loudness(1000.0F),
            "machine hearing: a run is quieter than the same energy struck");
    require(ImpactLedger::run_loudness(1.0e5F, 10.0F) <= ImpactLedger::kRunMaxLoudness,
            "machine hearing: a run is held to the hoist's loudness");

    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(4.0);
    const auto parts = s.machine_parts();
    std::cout << "  machine hearing: " << parts.size() << " machine parts tracked\n";
    require(parts.size() >= 25, "machine hearing: the tower's machine parts are tracked");
    // At rest the tower is quiet.
    require(s.machine_noises().empty(), "machine hearing: machines at rest draw nothing");

    int runners = 0;
    for (const auto &part : parts) {
        const std::uint32_t body = s.kit_body_index(part.entity);
        require(body != Simulation::kKitNone && s.kit_body_dynamic(body),
                "machine hearing: each tracked part is a dynamic kit body");
        const scraperx::sim::Vector3 at = s.kit_body_position(body);
        const std::uint64_t seen = s.last_impact_sequence();
        // Moved by hand at 1.5 m/s up: the run is measured from its motion.
        s.debug_launch_kit_body(body, at, {0.0, 1.5, 0.0});
        (void)s.advance_frame(1.0 / 60.0);
        const scraperx::sim::Vector3 v = s.kit_body_velocity(body);
        const double speed = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        double run = 0.0;
        for (const auto &n : s.machine_noises()) {
            if (n.entity == part.entity) {
                run = n.loudness;
            }
        }
        // Let go, it comes back down onto its guide, stop or deck.
        double hit_j = 0.0;
        for (int i = 0; i < 240 && hit_j == 0.0; ++i) {
            (void)s.advance_frame(1.0 / 60.0);
            for (const auto &hit : s.impacts()) {
                if (hit.sequence > seen && hit.entity == part.entity) {
                    hit_j = std::max(hit_j, hit.energy_j);
                }
            }
        }
        const double hit_loud = ImpactLedger::loudness(static_cast<float>(hit_j));
        std::cout << "  machine hearing: entity " << part.entity << (part.runs ? " (runs)" : "")
                  << ": moving " << speed << " m/s, run " << run << " (carries " << run * Matron::kFullHearingM << " m), hit "
                  << hit_j << " J (" << hit_loud << ", carries " << hit_loud * Matron::kFullHearingM << " m)\n";
        if (part.runs) {
            // Heard exactly while it measurably moves: one held by its latch
            // or stop is not running, and is heard by its hit instead.
            const bool moving = speed >= ImpactLedger::kRunMinMps;
            runners += moving ? 1 : 0;
            require(moving == (run > 0.0), "machine hearing: a rider part is heard running exactly while it moves");
            require(moving || hit_j > 0.0, "machine hearing: a held rider part is heard by its hit");
        } else {
            require(run == 0.0, "machine hearing: a counterweight, boom or leaf is no second running lead");
        }
        (void)s.advance_frame(3.0);
    }
    std::cout << "  machine hearing: " << runners << " running sources heard moving\n";
    // Her ear, end to end (no hearing injected): for every rider part that
    // runs, she is put awake 3 m from it and past the loudest run's reach,
    // the part is set running by hand, and Matron::hear's own pick decides.
    // Past its reach no source is heard. Within it, a source on a level she
    // keeps (her decks up to the 198 ring) is heard; above them she is on her
    // way back to her own levels (a journey hears nothing), so there the near
    // case is printed, not gated.
    {
        int heard_near = 0;
        int checked = 0;
        for (const auto &part : parts) {
            if (!part.runs) {
                continue;
            }
            const std::uint32_t body = s.kit_body_index(part.entity);
            const scraperx::sim::Vector3 at = s.kit_body_position(body);
            const auto listen = [&](const double off) {
                s.debug_place_matron(scraperx::sim::Vector3{at.x + off, at.y, at.z});
                (void)s.advance_frame(1.0 / 60.0);
                double heard = 0.0;
                for (int i = 0; i < 6; ++i) {
                    s.debug_launch_kit_body(body, s.kit_body_position(body), {0.0, 1.5, 0.0});
                    (void)s.advance_frame(1.0 / 60.0);
                    heard = std::max(heard, s.matron_state().noise_heard);
                }
                (void)s.advance_frame(3.0);
                return heard;
            };
            const double reach = ImpactLedger::kRunMaxLoudness * Matron::kFullHearingM;
            const double near = listen(3.0);
            const double far = listen(reach + 20.0);
            std::cout << "  machine hearing: entity " << part.entity << " heard " << near << " at 3 m, " << far << " at "
                      << reach + 20.0 << " m\n";
            ++checked;
            heard_near += near > 0.0 ? 1 : 0;
            require(far == 0.0, "machine hearing: past its reach a running machine is not heard");
            if (at.y < 200.0) {
                require(near > 0.0, "machine hearing: 3 m from a running machine on her levels she hears it");
            }
        }
        std::cout << "  machine hearing: her ear: " << heard_near << " of " << checked << " heard near\n";
        require(heard_near >= 3, "machine hearing: she hears the running machines on her levels");
    }

    require(runners >= 8, "machine hearing: the stages' running sources are heard");
    (void)s.advance_frame(10.0);
    std::cout << "  machine hearing: " << s.machine_noises().size() << " running after settling\n";
}
