// J-094 (#412): her weight is the only key -- the probe. A 25 kg plate
// drops 0.35 m onto a pan over a brake stop (50 mm, 10 kN, 500 J), measured
// at 90 Hz for four loads: empty, him standing and dropped, a 55 kg drum,
// her. Hold for empty / him / drum, shear for her. F_stop comes from the
// stack's vertical momentum; brake work is its integral over the pan's
// descent. The plate-pan impact dissipates most of the drop, so legs is the
// impact dissipation for rigid loads, and for her that plus her legs.
// Hold/shear reads the DYNAMIC stop point (the governor creeps at 0.08 m/s
// after the arrest). Drum case is plate + drum (him + drum is 649 J).
#include "sim/loose_stock.hpp"
#include "sim/matron.hpp"
#include "sim/trapeze.hpp"  // the tuning header names Trapeze::kArmSpringHz
#include "sim/physics_world_tuning.hpp"
#include "sim_tests.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
using scraperx::sim::InitialSpawn;
using scraperx::sim::LooseStock;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;
using scraperx::sim::detail::kPlayerHalfHeight;
using scraperx::sim::detail::kPlayerMassKg;
namespace {
constexpr double kDt = Simulation::kFixedStepSeconds;  // 1/90 s, the probe rate
constexpr double kStopSpeed = 0.1;                     // the stack has stopped
constexpr double kBottomMm = 48.0;  // within 2 mm of the 50 mm stroke: bottomed
struct Sample {  // one 90 Hz sample of the falling stack
    double plate_vy = 0.0;
    double pan_y = 0.0;
    double pan_vy = 0.0;
    double rider_vy = 0.0;  // 0 when there is no rider
};
struct ProbeResult {
    double ke_contact_j = 0.0;
    double stop_energy_j = 0.0;
    double peak_stop_n = 0.0;
    double dynamic_travel_mm = 0.0;  // pan travel at the dynamic stop point
    bool bottomed_out = false;       // dynamic travel reached the end of travel
    double delivered_j = 0.0;
    double legs_j = 0.0;  // delivered - absorbed - remaining KE
    double static_n = 0.0;  // mean stop force over the last 0.5 s (static case)
};
enum class Rider { kEmpty, kPlayerStatic, kPlayerDropped, kDrum, kMatron };
constexpr const char *kRiderNames[] = {"empty", "him-static", "him-dropped", "drum", "her"};
double rider_mass(const Rider rider) {
    if (rider == Rider::kEmpty) return 0.0;
    if (rider == Rider::kDrum) return 55.0;  // LooseStock Drum: 49 + 3 + 3 kg
    if (rider == Rider::kMatron) return static_cast<double>(Matron::kMassKg);
    return static_cast<double>(kPlayerMassKg);  // him, static and dropped
}
std::uint32_t find_drum(Simulation &simulation) {  // a Drum's kit body index
    for (std::uint32_t i = 0; i < simulation.loose_stock_count(); ++i) {
        if (simulation.loose_stock_piece(i).kind ==
            static_cast<std::uint8_t>(LooseStock::Kind::Drum)) {
            return simulation.kit_body_index(Simulation::kLooseStockFirstEntity + i);
        }
    }
    return Simulation::kKitNone;
}
ProbeResult run_case(const Rider rider, const double drop_m) {  // drop 0 = static
    Simulation simulation(InitialSpawn::StaticDeck);
    const std::uint32_t plate = simulation.kit_body_index(Simulation::kProbePlateEntityId);
    const std::uint32_t pan = simulation.kit_body_index(Simulation::kProbePanEntityId);
    require(plate != Simulation::kKitNone, "probe: plate rig not built (proving spawn only)");
    require(pan != Simulation::kKitNone, "probe: pan rig not built (proving spawn only)");
    require(std::abs(simulation.kit_body_mass(plate) - Simulation::kProbePlateMassKg) < 0.01,
            "probe: plate mass is the rig contract");
    require(std::abs(simulation.kit_body_mass(pan) - Simulation::kProbePanMassKg) < 0.01,
            "probe: pan mass is the rig contract");
    for (int i = 0; i < 45; ++i) {
        require(simulation.advance_frame(kDt).accepted, "probe: settle must accept");
    }
    const double pan_top_rest =
        simulation.kit_body_position(pan).y + Simulation::kProbePanHalfHeightM;
    require(std::abs(pan_top_rest - Simulation::kProbePanTopYM) < 0.005,
            "probe: pan holds at the top of travel at rest");
    const double plate_top = pan_top_rest + drop_m + 2.0 * Simulation::kProbePlateHalfHeightM;
    const double plate_y = plate_top - Simulation::kProbePlateHalfHeightM;
    simulation.debug_launch_kit_body(
        plate, Vector3{Simulation::kProbeRigX, plate_y, Simulation::kProbeRigZ},
        Vector3{0.0, 0.0, 0.0});
    std::uint32_t drum = Simulation::kKitNone;
    double matron_y_prev = 0.0;
    switch (rider) {
        case Rider::kPlayerStatic:
        case Rider::kPlayerDropped:
            simulation.debug_launch_player(
                Vector3{Simulation::kProbeRigX,
                        plate_top + static_cast<double>(kPlayerHalfHeight),
                        Simulation::kProbeRigZ},
                Vector3{0.0, 0.0, 0.0});
            break;
        case Rider::kDrum:
            drum = find_drum(simulation);
            require(drum != Simulation::kKitNone, "probe: no Drum loose stock to drop");
            simulation.debug_launch_kit_body(
                drum, Vector3{Simulation::kProbeRigX, plate_top + 0.44, Simulation::kProbeRigZ},
                Vector3{0.0, 0.0, 0.0});
            break;
        case Rider::kMatron:
            simulation.debug_fix_matron_pace(0);  // creep: she stays on the plate
            simulation.debug_hold_matron_hands(10.0);
            simulation.debug_place_matron(
                Vector3{Simulation::kProbeRigX, plate_top, Simulation::kProbeRigZ});
            matron_y_prev = simulation.matron_state().position.y;
            break;
        case Rider::kEmpty: break;
    }
    std::vector<Sample> samples;  // 2 s at 90 Hz: the stroke is ~0.1 s
    for (int i = 0; i < 180; ++i) {
        require(simulation.advance_frame(kDt).accepted, "probe: advance must accept");
        Sample sample;
        sample.plate_vy = simulation.kit_body_velocity(plate).y;
        sample.pan_y = simulation.kit_body_position(pan).y;
        sample.pan_vy = simulation.kit_body_velocity(pan).y;
        switch (rider) {
            case Rider::kPlayerStatic:
            case Rider::kPlayerDropped:
                sample.rider_vy = simulation.snapshot().player_linear_velocity.y;
                break;
            case Rider::kDrum: sample.rider_vy = simulation.kit_body_velocity(drum).y; break;
            case Rider::kMatron: {
                const double y = simulation.matron_state().position.y;
                sample.rider_vy = (y - matron_y_prev) / kDt;
                matron_y_prev = y;
                break;
            }
            case Rider::kEmpty: break;
        }
        samples.push_back(sample);
    }
    const double plate_mass = Simulation::kProbePlateMassKg;
    const double pan_mass = Simulation::kProbePanMassKg;
    const double mass_rider = rider_mass(rider);
    const double mass_total = plate_mass + pan_mass + mass_rider;
    auto pan_top = [&](const Sample &s) { return s.pan_y + Simulation::kProbePanHalfHeightM; };
    auto momentum = [&](const Sample &s) {
        return plate_mass * s.plate_vy + pan_mass * s.pan_vy + mass_rider * s.rider_vy;
    };
    std::size_t contact = 0;
    if (drop_m > 0.0) {
        for (std::size_t i = 5; i < samples.size(); ++i) {  // skip placement shock
            if (samples[i].plate_vy > samples[i - 1].plate_vy ||
                samples[i].pan_vy < -kStopSpeed) {
                contact = i;
                break;
            }
        }
        require(contact > 0, "probe: the plate never reached the pan");
    }
    ProbeResult result;
    if (drop_m > 0.0) {
        const Sample &before = samples[contact - 1];
        result.ke_contact_j = 0.5 * plate_mass * before.plate_vy * before.plate_vy +
                              0.5 * mass_rider * before.rider_vy * before.rider_vy;
    }
    std::size_t stopped = samples.size() - 1;
    for (std::size_t i = contact + 1; i < samples.size(); ++i) {
        if (std::abs(samples[i].plate_vy) < kStopSpeed) {
            stopped = i;
            break;
        }
    }
    double stop_energy = 0.0;
    double peak = 0.0;
    for (std::size_t i = contact + 1; i <= stopped; ++i) {
        const double force =
            (momentum(samples[i]) - momentum(samples[i - 1])) / kDt + mass_total * kGravity;
        peak = std::max(peak, force);
        if (samples[i].pan_vy < 0.0) stop_energy += force * (-samples[i].pan_vy) * kDt;
    }
    result.stop_energy_j = stop_energy;
    result.peak_stop_n = peak;
    result.dynamic_travel_mm = (pan_top_rest - pan_top(samples[stopped])) * 1000.0;
    result.bottomed_out = result.dynamic_travel_mm >= kBottomMm;
    result.delivered_j =
        result.ke_contact_j + mass_total * kGravity * result.dynamic_travel_mm / 1000.0;
    const Sample &last = samples[stopped];
    const double ke_end = 0.5 * plate_mass * last.plate_vy * last.plate_vy +
                          0.5 * pan_mass * last.pan_vy * last.pan_vy +
                          0.5 * mass_rider * last.rider_vy * last.rider_vy;
    result.legs_j = result.delivered_j - result.stop_energy_j - ke_end;
    if (rider == Rider::kPlayerStatic) {
        double sum = 0.0;
        for (std::size_t i = samples.size() - 45; i < samples.size(); ++i) {
            sum += (momentum(samples[i]) - momentum(samples[i - 1])) / kDt +
                   mass_total * kGravity;
        }
        result.static_n = sum / 45.0;
    }
    std::cout << "PROBE " << kRiderNames[static_cast<int>(rider)]
              << ": ke_contact=" << result.ke_contact_j << " J stop=" << result.stop_energy_j
              << " J peak=" << result.peak_stop_n << " N dynamic_travel="
              << result.dynamic_travel_mm << " mm bottomed="
              << (result.bottomed_out ? "yes" : "no")
              << " delivered=" << result.delivered_j << " J legs=" << result.legs_j << " J";
    if (rider == Rider::kPlayerStatic) {
        std::cout << " static=" << result.static_n << " N (weight " << mass_total * kGravity
                  << " N)";
    }
    std::cout << "\n";
    return result;
}
void check_hold(const ProbeResult &result, const char *what) {
    require(!result.bottomed_out, what);
    require(result.delivered_j < Simulation::kProbeStopCapacityJ - 30.0, what);
    require(result.peak_stop_n < Simulation::kProbeStopCapN + 1000.0, what);
}
}  // namespace
void run_probe_plate() {
    const ProbeResult empty = run_case(Rider::kEmpty, Simulation::kProbeDropM);
    const double empty_free_share = Simulation::kProbePanMassKg /
                                    (Simulation::kProbePlateMassKg + Simulation::kProbePanMassKg);
    require(empty.legs_j > empty_free_share * empty.ke_contact_j &&
                empty.legs_j < 1.15 * empty.ke_contact_j,
            "probe: empty leg term is the impact dissipation");
    check_hold(empty, "probe: empty plate must hold");
    const ProbeResult him_static = run_case(Rider::kPlayerStatic, 0.0);
    const double weight = (Simulation::kProbePlateMassKg + Simulation::kProbePanMassKg +
                           static_cast<double>(kPlayerMassKg)) *
                          kGravity;
    require(std::abs(him_static.static_n - weight) / weight < 0.10,
            "probe: static stop force is the weight");
    const ProbeResult him = run_case(Rider::kPlayerDropped, Simulation::kProbeDropM);
    check_hold(him, "probe: him dropped must hold");
    const ProbeResult drum = run_case(Rider::kDrum, Simulation::kProbeDropM);
    const double drum_free_share = Simulation::kProbePanMassKg /
                                   (Simulation::kProbePlateMassKg + 55.0 +
                                    Simulation::kProbePanMassKg);
    require(drum.legs_j > drum_free_share * drum.ke_contact_j &&
                drum.legs_j < 1.15 * drum.ke_contact_j,
            "probe: drum leg term is the impact dissipation");
    check_hold(drum, "probe: drum must hold");
    const ProbeResult her = run_case(Rider::kMatron, Simulation::kProbeDropM);
    require(her.bottomed_out, "probe: her drop must shear the stop");
    require(her.delivered_j > Simulation::kProbeStopCapacityJ + 50.0,
            "probe: her drop clears the stop capacity");
    std::cout << "PASS probe-plate all four loads\n";
}
SCRAPERX_SLICE("probe-plate", run_probe_plate);
