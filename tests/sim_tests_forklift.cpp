// The yard's forklift (src/sim/forklift.hpp): parked it stays put; at its
// wheel the player takes the controls, drives and steers it, slides its forks
// under a pallet of drums, lifts it, carries it and sets it down elsewhere;
// the same inputs give the same end.
#include "sim/forklift.hpp"
#include "sim/loose_stock.hpp"
#include "sim_tests.hpp"

#include <array>
#include <cmath>
#include <iostream>

using scraperx::sim::Forklift;
using scraperx::sim::LooseStock;
using scraperx::sim::Simulation;

namespace {

scraperx::sim::Vector3 body_at(const Simulation &s, const std::uint64_t entity) {
    return s.kit_body_position(s.kit_body_index(entity));
}

double truck_yaw(const Simulation &s) {
    const auto q = s.kit_body_rotation(s.kit_body_index(Forklift::kChassisEntity));
    return 2.0 * std::atan2(q.y, q.w);
}

void hold(Simulation &s, const double x, const double z, const double seconds) {
    const auto ticks = static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (int i = 0; i < ticks; ++i) {
        (void)s.set_move_input(x, z);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
}

// From the game's start: round the beam stack's west end, onto the truck's
// floor from its west side, to the wheel, facing its forward (+z).
bool board(Simulation &s) {
    if (!(walk_to(s, 30.0, 10.0, 30.0) && walk_to(s, 36.4, 16.35, 20.0) &&
          walk_to(s, Forklift::kParkX, 16.35, 8.0, 0.1))) {
        return false;
    }
    (void)s.set_facing(0.0, 1.0);
    (void)s.advance_frame(0.5);
    return s.forklift_state().at_controls;
}

// The pallet of drums lifted and carried back 5 m, from a fresh start;
// returns the truck's, the pallet's and the first drum's end positions.
std::array<double, 6> fork_the_pallet(Simulation &s, const int pallet) {
    require(board(s) && s.request_forklift_action(), "forklift: aboard, at the wheel, the controls taken");
    (void)s.advance_frame(0.2);
    // Creep in until the forks are under the pallet's deck, the carriage
    // short of it.
    const double stop_z = body_at(s, LooseStock::kFirstEntity + static_cast<std::uint64_t>(pallet)).z - 1.75;
    int ticks = 0;
    while (body_at(s, Forklift::kChassisEntity).z < stop_z && ticks++ < 900) {
        (void)s.set_move_input(0.0, 0.25);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.advance_frame(1.5);
    const auto hits_before = s.impacts();
    const std::uint64_t seen = hits_before.empty() ? 0 : hits_before.back().sequence;
    // The lever, worked by hand: let go of the controls, take it, pull it to
    // its travel limit and hold it there.
    require(take_lever(s, true), "forklift: the lever pulled to its travel limit");
    for (int tick = 0; tick < static_cast<int>(Simulation::kTickRateHz); ++tick) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        if (tick % 15 == 0) {
            const auto snap = s.snapshot();
            const auto rot = s.kit_body_rotation(s.kit_body_index(Forklift::kChassisEntity));
            std::cout << "    lever hold t=" << tick << " lever_rad=" << s.forklift_state().lever_rad
                      << " lift_m=" << s.forklift_state().lift_m << " lift_mps=" << s.forklift_state().lift_mps
                      << " carrying=" << snap.carrying_entity_id << " p=(" << snap.player_position.x << ","
                      << snap.player_position.z << ") chassis_q=(" << rot.x << "," << rot.y << "," << rot.z << ","
                      << rot.w << ")\n";
        }
    }
    const double raising = s.forklift_state().lift_mps;
    const double held_at = s.forklift_state().lever_rad;
    // The valve opens over a fifth of a second (kLiftAccelMps2): the forks and
    // the pallet they pick up are never jerked hard enough to be heard as a hit.
    for (const auto &hit : s.impacts()) {
        require(hit.sequence <= seen ||
                    (hit.entity != Forklift::kCarriageEntity &&
                     hit.entity != LooseStock::kFirstEntity + static_cast<std::uint64_t>(pallet)),
                "forklift: raising the forks under the pallet jerks neither the forks nor the pallet");
    }
    require(release_lever(s), "forklift: let go, the lever springs back");
    (void)s.advance_frame(0.5);
    std::cout << "  forklift: held at " << held_at << " rad, the forks at " << raising << " m/s, "
              << s.forklift_state().lift_m << " m up\n";
    require(std::abs(raising - Forklift::kLiftSpeedMps) < 0.05,
            "forklift: raised with the pallet on them, the forks run up the mast at the lift's speed");
    const auto lifted = body_at(s, LooseStock::kFirstEntity + static_cast<std::uint64_t>(pallet));
    std::cout << "  forklift: the pallet lifted to y " << lifted.y << ", the forks " << s.forklift_state().lift_m
              << " m up\n";
    require(lifted.y > 0.3, "forklift: the forks under the pallet lift it, its drums on it");
    require(s.request_forklift_action(), "forklift: the controls taken again to back out");
    (void)s.advance_frame(0.2);
    hold(s, 0.0, -0.4, 2.5);
    require(s.forklift_state().gear == -1 && s.forklift_state().speed_mps < -0.1,
            "forklift: backing out with the pallet, in reverse");
    (void)s.advance_frame(1.5);
    require(take_lever(s, false), "forklift: the lever pushed to its travel limit");
    (void)s.advance_frame(1.5);
    require(release_lever(s), "forklift: let go, the lever springs back");
    require(s.request_forklift_action(), "forklift: the controls taken again to back out");
    (void)s.advance_frame(0.2);
    hold(s, 0.0, -0.4, 1.5);
    (void)s.advance_frame(1.5);
    const auto truck = body_at(s, Forklift::kChassisEntity);
    const auto pallet_end = body_at(s, LooseStock::kFirstEntity + static_cast<std::uint64_t>(pallet));
    const auto drum_end = body_at(s, LooseStock::kFirstEntity + static_cast<std::uint64_t>(pallet + 1));
    return {truck.x, truck.z, pallet_end.x, pallet_end.z, drum_end.y, drum_end.z};
}

} // namespace

void run_forklift() {
    // Parked, it stays put on its wheels.
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        const auto parked = body_at(s, Forklift::kChassisEntity);
        (void)s.advance_frame(10.0);
        const auto after = body_at(s, Forklift::kChassisEntity);
        const double crept = std::hypot(after.x - parked.x, after.z - parked.z);
        std::cout << "  forklift: parked 10 s, it moved " << crept << " m, its frame at y " << after.y << "\n";
        require(crept < 0.01 && std::abs(after.y) < 0.05, "forklift: parked, it stands on its wheels and stays put");
        // Parked, the engine is still and the blades stand clear of the
        // ground (3.5 cm, a little more at the tips), not scraping it.
        const auto still = s.forklift_state(0);
        std::cout << "  forklift: parked, the engine at " << still.engine_rpm << " rpm, the blades " << still.fork_gap_m
                  << " m over the ground\n";
        require(still.engine_rpm < 1.0 && still.gear == 0 && still.fork_gap_m > 0.02 && still.fork_gap_m < 0.08 &&
                    still.fork_drag_mps == 0.0 && still.fork_ground == Simulation::kStaticDeckEntityId,
                "forklift: parked, the engine still and the blades clear of the ground under them");
        require(!s.forklift_state().at_controls && !s.request_forklift_action(),
                "forklift: off the truck, its controls are not offered");
    }
    // Driven: forward, then forward and right; let go of, it stops.
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        (void)s.advance_frame(1.0);
        require(board(s), "forklift: on its floor at the wheel, DRIVE is offered");
        require(s.request_forklift_action(), "forklift: at the wheel the controls are taken");
        (void)s.advance_frame(0.2);
        require(s.forklift_state().driving, "forklift: driving");
        const auto start = body_at(s, Forklift::kChassisEntity);
        hold(s, 0.0, 1.0, 2.0);
        const auto ahead = body_at(s, Forklift::kChassisEntity);
        const double peak = s.forklift_state().speed_mps;
        // Driven forward: in forward gear, the engine turning the front
        // wheels through the transmission, the tires rolling on the grade.
        const auto driven = s.forklift_state();
        std::cout << "  forklift: driving, gear " << driven.gear << ", the engine at " << driven.engine_rpm
                  << " rpm, the front wheels at " << driven.wheel_spin_rad_s[0] << " rad/s on entity "
                  << driven.wheel_ground[0] << "\n";
        require(driven.gear == 1 && driven.engine_rpm > 500.0 && std::abs(driven.wheel_spin_rad_s[0]) > 1.0 &&
                    driven.wheel_ground[0] == Simulation::kStaticDeckEntityId &&
                    driven.wheel_ground[2] == Simulation::kStaticDeckEntityId,
                "forklift: driven, forward gear, the engine turning, the wheels rolling on the grade");
        // Facing +z, the view's right is -x.
        hold(s, -0.6, 0.8, 2.0);
        const double yaw = truck_yaw(s);
        (void)s.advance_frame(2.0);
        const auto snap = s.snapshot();
        std::cout << "  forklift: driven " << ahead.z - start.z << " m in 2 s (" << peak << " m/s), turned right to yaw "
                  << yaw << ", stopped at " << s.forklift_state().speed_mps << " m/s\n";
        require(ahead.z - start.z > 1.5 && std::abs(ahead.x - start.x) < 0.1, "forklift: the stick forward drives it forward");
        require(yaw < -0.3, "forklift: the stick right turns it right");
        require(std::abs(s.forklift_state().speed_mps) < 0.05, "forklift: the stick let go, it stops");
        require(snap.support_entity_id == Forklift::kChassisEntity && snap.death_count == 0,
                "forklift: the driver rides on its floor");
        require(s.request_forklift_action(), "forklift: Action lets go of the controls");
        (void)s.advance_frame(0.2);
        require(!s.forklift_state().driving, "forklift: let go, it is parked");
    }
    // The pallet of drums: forked, lifted, carried back and set down, the
    // drums still on it; twice, alike to the bit.
    const auto &table = LooseStock::placements();
    int pallet = -1;
    for (std::size_t i = 0; i < table.size() && pallet < 0; ++i) {
        if (table[i].kind == LooseStock::Kind::Pallet) {
            pallet = static_cast<int>(i);
        }
    }
    require(pallet >= 0, "forklift: the drums' first pallet");
    std::array<double, 6> runs[2];
    for (auto &run : runs) {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        (void)s.advance_frame(1.0);
        const auto before = body_at(s, LooseStock::kFirstEntity + static_cast<std::uint64_t>(pallet));
        run = fork_the_pallet(s, pallet);
        const double carried = before.z - run[3];
        std::cout << "  forklift: the pallet carried " << carried << " m and set down, its first drum at y " << run[4]
                  << "\n";
        require(carried > 3.0, "forklift: the pallet is carried back and set down elsewhere");
        require(run[4] > 0.45 && std::abs(run[5] - run[3]) < 0.5, "forklift: its drums ride on it and stand on it");
    }
    require(runs[0] == runs[1], "forklift: the same inputs end the same, bit for bit");
    std::cout << "PASS scraperx_sim forklift: carried_m=" << table[static_cast<std::size_t>(pallet)].z - runs[0][3] << "\n";
}
SCRAPERX_GROUP("forklift", 1020, run_forklift);
SCRAPERX_SELECT("forklift", run_forklift, run_forklift_lever, run_forklift_strain);

namespace {

// Drives the truck as a driver does, from where it is parked, facing +z, to
// the lane x = lane_x west of it: a full right turn, along until a full left
// turn (which carries the truck as far on as the right one did) will end on
// the lane, the left turn, then up the lane looking 1 m
// ahead until the truck stands at z = stop_z. Arriving is the proof the
// forks went in under the load: crooked, a blade strikes a runner and the
// truck stops short.
bool drive_to_lane(Simulation &s, const double lane_x, const double stop_z, const double pace) {
    constexpr double kWest = -0.5 * 3.14159265358979;
    int phase = 0;
    const double turn_start_z = body_at(s, Forklift::kChassisEntity).z;
    double turn_advance = 0.0;
    for (int tick = 0; tick < 40 * static_cast<int>(Simulation::kTickRateHz); ++tick) {
        const auto at = body_at(s, Forklift::kChassisEntity);
        const double yaw = truck_yaw(s);
        double steer = 0.0;   // + right (yaw falls)
        if (phase == 0) {
            steer = 1.0;
            if (yaw <= kWest + 0.2) {   // the wheel takes a moment to unwind
                turn_advance = at.z - turn_start_z;
                phase = 1;
            }
        } else if (phase == 1) {
            steer = std::clamp(2.0 * (yaw - kWest), -1.0, 1.0);
            if (at.x <= lane_x + turn_advance) {
                phase = 2;
            }
        } else if (phase == 2) {
            steer = -1.0;
            if (yaw >= -0.2) {
                phase = 3;
            }
        } else {
            if (at.z >= stop_z) {
                (void)s.set_move_input(0.0, 0.0);
                return true;
            }
            const double dx = lane_x - at.x;
            const double dz = 1.0;
            const double right = -dx * std::cos(yaw) + dz * std::sin(yaw);
            const double ahead = dx * std::sin(yaw) + dz * std::cos(yaw);
            steer = std::clamp(4.0 * std::atan2(right, ahead), -1.0, 1.0);
        }
        const double fx = std::sin(yaw);
        const double fz = std::cos(yaw);
        (void)s.set_facing(fx, fz);
        (void)s.set_move_input(fx * pace - fz * steer, fz * pace + fx * steer);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
    return false;
}

struct LiftTry {
    double peak_lift_n = 0.0;
    double parked_rear_n = 0.0;
    double least_rear_n = 1.0e9;
    double peak_pitch_rad = 0.0;
    double load_rise_m = 0.0;
    double rise_at_rear_off_m = -1.0;   // the load's rise when the rear wheels left the ground
    double held_rise_m = 0.0;           // a second after RAISE is let go
    double lowered_rear_n = 0.0;        // the forks let down again
    double lowered_pitch_rad = 0.0;
    double least_fork_gap_m = 1.0;      // the blades' least clearance over what is under them
};

// From the start: aboard, the controls taken, into the load's lane, forks in
// under it, then RAISE held for `seconds`, let go for a second, and LOWER
// held until the forks are down.
LiftTry try_lift(const std::uint64_t load_entity, const double lane_x, const double seconds) {
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    require(board(s) && s.request_forklift_action(), "forklift strain: aboard, the controls taken");
    (void)s.advance_frame(0.5);
    LiftTry result;
    const auto parked = s.forklift_state();
    result.parked_rear_n = parked.wheel_load_n[2] + parked.wheel_load_n[3];
    const auto load0 = body_at(s, load_entity);
    require(drive_to_lane(s, lane_x, load0.z - 1.85, 0.35), "forklift strain: driven in under the load");
    (void)s.advance_frame(2.0);
    // The lever, worked by hand: pulled to its travel limit and held there
    // while the lift is measured, then let go.
    require(take_lever(s, true), "forklift strain: the lever pulled to its travel limit");
    for (int tick = 0; tick < static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz)); ++tick) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto f = s.forklift_state();
        const double rear = f.wheel_load_n[2] + f.wheel_load_n[3];
        const double rise = body_at(s, load_entity).y - load0.y;
        result.peak_lift_n = std::max(result.peak_lift_n, f.lift_n);
        result.least_rear_n = std::min(result.least_rear_n, rear);
        result.peak_pitch_rad = std::max(result.peak_pitch_rad, f.pitch_rad);
        result.load_rise_m = std::max(result.load_rise_m, rise);
        result.least_fork_gap_m = std::min(result.least_fork_gap_m, f.fork_gap_m);
        if (result.rise_at_rear_off_m < 0.0 && rear < 0.05 * result.parked_rear_n) {
            result.rise_at_rear_off_m = rise;
        }
    }
    require(release_lever(s), "forklift strain: let go, the lever springs back");
    (void)s.advance_frame(1.0);
    result.held_rise_m = body_at(s, load_entity).y - load0.y;
    require(take_lever(s, false), "forklift strain: the lever pushed to its travel limit");
    (void)s.advance_frame(seconds + 1.0);
    require(release_lever(s), "forklift strain: let go, the lever springs back");
    (void)s.advance_frame(1.0);
    const auto lowered = s.forklift_state();
    result.lowered_rear_n = lowered.wheel_load_n[2] + lowered.wheel_load_n[3];
    result.lowered_pitch_rad = lowered.pitch_rad;
    return result;
}

} // namespace

// The forklift's power and its limit. The 2.8 t coil: the lift takes it up,
// hard, the rear axle unloading. The 5 t stillage of billets: more than the
// counterweight balances out on the forks -- straining at it, before it
// leaves the ground, the truck's back end rises, nose down on its front
// wheels. Let go, the ram holds; the forks let down, the truck comes back
// down onto its rear wheels.
void run_forklift_strain() {
    const auto &table = LooseStock::placements();
    std::uint64_t stillage = 0;
    std::uint64_t coil = 0;
    // The yard's, the first of each (the crane's laydown has its own).
    for (std::size_t i = 0; i < table.size(); ++i) {
        if (table[i].kind == LooseStock::Kind::BilletStillage && stillage == 0) {
            stillage = LooseStock::kFirstEntity + i;
        } else if (table[i].kind == LooseStock::Kind::CoilSkid && coil == 0) {
            coil = LooseStock::kFirstEntity + i;
        }
    }
    require(stillage != 0 && coil != 0, "forklift strain: the stillage and the coil");
    const LiftTry light = try_lift(coil, 30.0, 3.0);
    std::cout << "  forklift strain: the coil lifted " << light.load_rise_m << " m, the lift at " << light.peak_lift_n
              << " N, the rear axle " << light.parked_rear_n << " -> " << light.least_rear_n << " N; held at "
              << light.held_rise_m << " m; let down, the rear axle " << light.lowered_rear_n << " N\n";
    const LiftTry heavy = try_lift(stillage, 34.0, 3.0);
    std::cout << "  forklift strain: the stillage at " << heavy.rise_at_rear_off_m
              << " m when the rear wheels left the ground, the lift at " << heavy.peak_lift_n << " N, the rear axle "
              << heavy.parked_rear_n << " -> " << heavy.least_rear_n << " N, nose down "
              << heavy.peak_pitch_rad * 57.2958 << " deg; let down, the rear axle " << heavy.lowered_rear_n
              << " N, pitch " << heavy.lowered_pitch_rad * 57.2958 << " deg; the blades' least clearance "
              << heavy.least_fork_gap_m << " m\n";
    // The lift carries the coil's 2.7 t of steel, and holds it; at the
    // stillage it pushes past 4 t's weight and the truck, not the load,
    // gives. Let down, each stands back on its rear wheels.
    require(light.load_rise_m > 0.3 && light.peak_lift_n > 2700.0 * 9.81 &&
                light.least_rear_n < 0.6 * light.parked_rear_n,
            "forklift strain: the coil is lifted, hard, the rear axle unloading");
    require(light.held_rise_m > 0.3, "forklift strain: let go, the ram holds the coil up");
    require(heavy.peak_lift_n > 4000.0 * 9.81 && heavy.rise_at_rear_off_m >= 0.0 &&
                heavy.rise_at_rear_off_m < 0.05 && heavy.peak_pitch_rad > 0.03,
            "forklift strain: straining at the stillage, the truck's back end comes up before the load does");
    require(light.lowered_rear_n > 0.8 * light.parked_rear_n && heavy.lowered_rear_n > 0.8 * heavy.parked_rear_n &&
                std::abs(heavy.lowered_pitch_rad) < 0.02,
            "forklift strain: the forks let down, the truck stands back on its rear wheels");
    std::cout << "PASS scraperx_sim forklift strain: coil_rise_m=" << light.load_rise_m
              << " stillage_pitch_deg=" << heavy.peak_pitch_rad * 57.2958 << "\n";
}
SCRAPERX_GROUP("forklift_strain", 1030, run_forklift_strain);

// Stands on one spot to the centimetre: the hands must meet the lever's
// handle, or the hinge fights the grip.
bool stand_at(Simulation &s, const double x, const double z) {
    for (int tick = 0; tick < 270; ++tick) {
        const auto at = s.snapshot().player_position;
        const double dx = x - at.x;
        const double dz = z - at.z;
        const double gap = std::hypot(dx, dz);
        if (gap <= 0.02) {
            (void)s.set_move_input(0.0, 0.0);
            return true;
        }
        const double step = std::min(0.15, gap);
        (void)s.set_move_input(dx / gap * step, dz / gap * step);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
    return false;
}

// The lever (J-083 #401): a physical body on a hinge beside the station.
// The mast moves only while the lever is past its dead band, and let go it
// springs back to neutral.
void run_forklift_lever() {
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    require(board(s), "forklift lever: on its floor at the wheel, DRIVE offered");
    // The lever is a physical control: it works without the Action toggle.
    require(!s.forklift_state().driving, "forklift lever: the controls not taken");
    // At neutral the mast stands still.
    const double still = s.forklift_state().lift_m;
    (void)s.advance_frame(1.0);
    require(std::abs(s.forklift_state().lift_m - still) < 0.005 &&
                std::abs(s.forklift_state().lever_rad) < Forklift::kLeverDeadBandRad,
            "forklift lever: at neutral, the mast does not move");
    // Step right to the lever: dead ahead of the station, under the hands.
    const auto truck = s.forklift_state();
    const auto chassis = s.kit_body_position(s.kit_body_index(truck.chassis_entity));
    const auto rotation = s.kit_body_rotation(s.kit_body_index(truck.chassis_entity));
    const double yaw = 2.0 * std::atan2(rotation.y, rotation.w);
    const double fx = std::sin(yaw);
    const double fz = std::cos(yaw);
    require(stand_at(s, chassis.x - 0.15 * fx, chassis.z - 0.15 * fz),
            "forklift lever: stepped right to the lever");
    (void)s.set_facing(fx, fz);
    (void)s.advance_frame(0.5);
    require(s.snapshot().carry_target_entity_id == Forklift::kLeverEntity &&
                s.snapshot().carry_target_kind == 2,
            "forklift lever: GRAB is offered at the lever");
    require(s.request_pick_up(), "forklift lever: the lever taken in the hands");
    (void)s.advance_frame(0.3);
    require(s.snapshot().carrying_entity_id == Forklift::kLeverEntity,
            "forklift lever: the lever in the hands");
    // Taken hold, at neutral, the mast still does not move.
    const double held = s.forklift_state().lift_m;
    (void)s.advance_frame(0.5);
    require(std::abs(s.forklift_state().lift_m - held) < 0.005,
            "forklift lever: taken hold, at neutral, the mast does not move");
    // Step back: the hands draw the lever toward the driver, past its dead
    // band, to near its travel limit. Near, not on it: one tick at 0.15 on
    // the stick overshoots the lever 0.014 rad, and a target at 0.34 lands
    // it on its 0.35 rad limit, where the hinge fights the hands and the
    // carry tears (see take_lever). At 0.32 the mast still runs at 0.36 m/s.
    double pulled = 0.0;
    for (int tick = 0; tick < 5 * static_cast<int>(Simulation::kTickRateHz); ++tick) {
        (void)s.set_move_input(-0.15 * fx, -0.15 * fz);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        pulled = s.forklift_state().lever_rad;
        if (pulled > Forklift::kLeverTravelRad - 0.03) {
            break;
        }
    }
    (void)s.set_move_input(0.0, 0.0);
    require(pulled > Forklift::kLeverTravelRad - 0.04, "forklift lever: pulled to its travel limit");
    // Held past the dead band, the forks run up the mast at the lift's speed.
    (void)s.advance_frame(1.0);
    const double rising = s.forklift_state().lift_mps;
    const double up = s.forklift_state().lift_m;
    std::cout << "  forklift lever: pulled to " << pulled << " rad, the forks at " << rising << " m/s, " << up
              << " m up\n";
    require(up > 0.2 && std::abs(rising - Forklift::kLiftSpeedMps) < 0.05,
            "forklift lever: past the dead band, the mast follows the lever");
    // Let go: the lever springs back to neutral and the mast holds where it stands.
    require(s.request_set_down(), "forklift lever: let go");
    (void)s.advance_frame(1.0);
    const double back = s.forklift_state().lever_rad;
    const double held_up = s.forklift_state().lift_m;
    (void)s.advance_frame(1.0);
    std::cout << "  forklift lever: let go, back to " << back << " rad, the forks held at " << held_up << " m\n";
    require(std::abs(back) < Forklift::kLeverDeadBandRad, "forklift lever: let go, it springs back to neutral");
    require(std::abs(s.forklift_state().lift_m - held_up) < 0.005,
            "forklift lever: let go, the ram holds the forks");
    std::cout << "PASS scraperx_sim forklift lever: pulled_rad=" << pulled << " lifted_m=" << held_up << "\n";
}
SCRAPERX_GROUP("forklift_lever", 1025, run_forklift_lever);
