// The tower's machines she hears (gap 10): every moving part is tracked, so a hard hit is heard; each rider part is also its running source while it moves.
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
    require(s.machine_noises().empty(), "machine hearing: machines at rest draw nothing");

    int runners = 0;
    for (const auto &part : parts) {
        const std::uint32_t body = s.kit_body_index(part.entity);
        require(body != Simulation::kKitNone && s.kit_body_dynamic(body),
                "machine hearing: each tracked part is a dynamic kit body");
        const scraperx::sim::Vector3 at = s.kit_body_position(body);
        const std::uint64_t seen = s.last_impact_sequence();
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
            // Heard exactly while it measurably moves: one held by its latch or stop is heard by its hit instead.
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
    // Her ear, end to end: a running rider part within reach on her decks is
    // heard; past its reach it is not, above her decks it is printed, not gated.
    {
        int heard_near = 0;
        int checked = 0;
        for (const auto &part : parts) {
            if (!part.runs) {
                continue;
            }
            const std::uint32_t body = s.kit_body_index(part.entity);
            const scraperx::sim::Vector3 at = s.kit_body_position(body);
            // run_only: the far listen isolates the run; the near listen hears
            // the part (a held part is heard by its hit, not by a run it has
            // none of). The far check is about the run's reach, and the hit is
            // a different sound with its own reach — at the far distance the
            // ear heard the launch's hit (0.768808, carrying 42.28 m), not the
            // run, so a far listen that read right after each launch failed on
            // the hit. One launch only, then read in the gap: the launch sets
            // the part moving, so its run sounds while it flies; the launch's
            // own hits (a jolt as it takes up, the landing ~0.3 s later) are
            // one-tick sounds, heard for a frame each. Three frames clear the
            // launch; the reads that follow hear the run alone, before the
            // landing.
            const auto listen = [&](const double off, const bool run_only) {
                s.debug_place_matron(scraperx::sim::Vector3{at.x + off, at.y, at.z});
                (void)s.advance_frame(1.0 / 60.0);
                double heard = 0.0;
                if (run_only) {
                    // Measured: the launch's arrest hit is heard at frame +3
                    // (entity 2100: 0.723004), the run is present at +4..+9
                    // (speed 0.28 down to 0.14 m/s), and the landing is past
                    // +9 — so three frames clear the launch and the reads
                    // that follow hear the run alone, before the landing.
                    s.debug_launch_kit_body(body, s.kit_body_position(body), {0.0, 1.5, 0.0});
                    (void)s.advance_frame(1.0 / 60.0);
                    (void)s.advance_frame(1.0 / 60.0);
                    (void)s.advance_frame(1.0 / 60.0);
                    for (int i = 0; i < 6; ++i) {
                        (void)s.advance_frame(1.0 / 60.0);
                        heard = std::max(heard, s.matron_state().noise_heard);
                    }
                } else {
                    for (int i = 0; i < 6; ++i) {
                        s.debug_launch_kit_body(body, s.kit_body_position(body), {0.0, 1.5, 0.0});
                        (void)s.advance_frame(1.0 / 60.0);
                        heard = std::max(heard, s.matron_state().noise_heard);
                    }
                }
                // She is alive at each listen: a dead Matron hears nothing,
                // so a far listen that passed because she died on the way
                // down would be vacuous.
                require(s.matron_state().alive, "machine hearing: she is alive at each listen");
                (void)s.advance_frame(3.0);
                return heard;
            };
            const double reach = ImpactLedger::kRunMaxLoudness * Matron::kFullHearingM;
            const double near = listen(3.0, false);
            const double far = listen(reach + 20.0, true);
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
SCRAPERX_GROUP("machine_hearing", 1070, run_machine_hearing);
SCRAPERX_SLICE("machine-hearing", run_machine_hearing);
