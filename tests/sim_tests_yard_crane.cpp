// The yard crane (src/sim/yard_crane.hpp): parked it stands still; its
// remote is taken from the cabinet at the mast's foot; worked, the stick
// sends the hook and the buttons hoist; a container rigged from its roof is
// ridden up and landed on the 44 m dock, alike twice to the bit; the crane's
// forklift is lifted with its driver aboard onto the 22 m dock and driven in
// onto the deck; the 8 t plate stack stalls the hoist on the ground.
#include "sim/forklift.hpp"
#include "sim/loose_stock.hpp"
#include "sim/yard_crane.hpp"
#include "sim_tests.hpp"

#include <array>
#include <cmath>
#include <iostream>

using scraperx::sim::Forklift;
using scraperx::sim::LooseStock;
using scraperx::sim::Simulation;
using scraperx::sim::YardCrane;

namespace {

scraperx::sim::Vector3 body_at(const Simulation &s, const std::uint64_t entity) {
    return s.kit_body_position(s.kit_body_index(entity));
}

// The laydown's first piece of a kind (the yard's own come before it).
std::uint64_t laydown_piece(const LooseStock::Kind kind) {
    const auto &table = LooseStock::placements();
    for (std::size_t i = 0; i < table.size(); ++i) {
        if (table[i].kind == kind && table[i].x < -50.0F && table[i].z < -100.0F && table[i].z > -125.0F) {
            return LooseStock::kFirstEntity + i;
        }
    }
    return 0;
}

// From the game's start, west of the stack's buttress, east of the laydown's
// plate stack on its sleepers, to the cabinet's open front at the foot of
// the mast, facing it.
bool walk_to_cabinet(Simulation &s) {
    if (!(walk_to(s, -25.0, -40.0, 40.0) && walk_to(s, -50.0, -100.0, 50.0) && walk_to(s, -50.0, -125.0, 30.0) &&
          walk_to(s, YardCrane::kMastX + 4.2, YardCrane::kMastZ + 5.6, 30.0, 0.1))) {
        return false;
    }
    (void)s.set_facing(0.0, -1.0);
    (void)s.advance_frame(0.3);
    return s.yard_crane_state().remote_in_reach;
}

bool take_remote(Simulation &s) {
    if (!(walk_to_cabinet(s) && s.request_yard_crane_take())) {
        return false;
    }
    (void)s.advance_frame(0.2);
    return s.yard_crane_state().has_remote;
}

void stick_for(Simulation &s, const double x, const double z, const double seconds) {
    const auto ticks = static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (int i = 0; i < ticks; ++i) {
        (void)s.set_move_input(x, z);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
}

// The remote's switch, until the crane reads `on`.
bool work(Simulation &s, const bool on) {
    if (s.yard_crane_state().operating == on) {
        return true;
    }
    if (!s.request_yard_crane_operate()) {
        return false;
    }
    (void)s.advance_frame(0.1);
    return s.yard_crane_state().operating == on;
}

// The operator sends the hook over (x, z): the stick toward it, gentler as
// the trolley nears, then nothing, until the trolley is there and the hook
// hangs still under it.
bool hook_over(Simulation &s, const double x, const double z, const double budget) {
    const auto ticks = static_cast<int>(budget * static_cast<double>(Simulation::kTickRateHz));
    int settled = 0;
    for (int tick = 0; tick < ticks; ++tick) {
        const auto crane = s.yard_crane_state();
        const double dx = x - crane.rope_top.x;
        const double dz = z - crane.rope_top.z;
        const double off = std::hypot(dx, dz);
        const double swing = std::hypot(crane.hook.x - x, crane.hook.z - z);
        settled = off < 0.15 && swing < 0.3 ? settled + 1 : 0;
        if (settled > static_cast<int>(Simulation::kTickRateHz)) {
            (void)s.set_move_input(0.0, 0.0);
            return true;
        }
        const double gain = off > 0.05 ? std::min(1.0, off / 6.0) / off : 0.0;
        (void)s.set_move_input(dx * gain, dz * gain);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
    const auto crane = s.yard_crane_state();
    std::cout << "  crane: the hook short of (" << x << ", " << z << "): the trolley at (" << crane.rope_top.x << ", "
              << crane.rope_top.z << "), the hook at (" << crane.hook.x << ", " << crane.hook.y << ", " << crane.hook.z
              << ")\n";
    return false;
}

// The hoist worked until the hook's throat is at y: at full speed, slower for
// the last metre (never into the lever's dead zone); false if the winch stops
// short (slack, stalled, at its limit) or the budget runs out.
bool hoist_to(Simulation &s, const double y, const double budget) {
    const auto ticks = static_cast<int>(budget * static_cast<double>(Simulation::kTickRateHz));
    for (int tick = 0; tick < ticks; ++tick) {
        const auto crane = s.yard_crane_state();
        const double off = y - crane.hook.y;
        if (std::abs(off) < 0.03) {
            (void)s.set_yard_crane_hoist(0.0);
            return true;
        }
        if (tick > 10 && (crane.slack || crane.hoist_stalled || crane.upper_limit)) {
            break;
        }
        (void)s.set_yard_crane_hoist(std::copysign(std::clamp(std::abs(off), 0.1, 1.0), off));
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_yard_crane_hoist(0.0);
    const auto crane = s.yard_crane_state();
    std::cout << "  crane: the hoist short of y " << y << ": the hook at " << crane.hook.y << ", pulling "
              << crane.hoist_n << " N, slack " << crane.slack << " stalled " << crane.hoist_stalled << " at the limit "
              << crane.upper_limit << "\n";
    return false;
}

// Lowers until the slack-rope switch stops the drum: the load has landed.
bool lower_to_rest(Simulation &s, const double budget) {
    const auto ticks = static_cast<int>(budget * static_cast<double>(Simulation::kTickRateHz));
    for (int tick = 0; tick < ticks; ++tick) {
        (void)s.set_yard_crane_hoist(-0.3);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        if (tick > 10 && s.yard_crane_state().slack) {
            (void)s.set_yard_crane_hoist(0.0);
            return true;
        }
    }
    (void)s.set_yard_crane_hoist(0.0);
    return false;
}

// Lowers until the load's weight is off the rope (the hoist pulling no more
// than half again the empty hook block's and shackle's own 782 kg): the load
// has landed, and the block still hangs on its rope above it, as an operator
// stops it. Lowered on to the slack-rope switch instead, the block comes to
// rest on the load, where it can foul it.
bool lower_until_landed(Simulation &s, const double budget) {
    constexpr double kEmptyHookN = 782.0 * 9.81;
    const auto ticks = static_cast<int>(budget * static_cast<double>(Simulation::kTickRateHz));
    for (int tick = 0; tick < ticks; ++tick) {
        (void)s.set_yard_crane_hoist(-0.3);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        if (tick > 10 && s.yard_crane_state().hoist_n < 1.5 * kEmptyHookN) {
            (void)s.set_yard_crane_hoist(0.0);
            (void)s.advance_frame(0.5);
            return true;
        }
    }
    (void)s.set_yard_crane_hoist(0.0);
    return false;
}

// The rigger steps to the sling's shackle where it dangles and takes it.
bool take_shackle(Simulation &s, const double budget) {
    const auto ticks = static_cast<int>(budget * static_cast<double>(Simulation::kTickRateHz));
    for (int tick = 0; tick < ticks; ++tick) {
        const auto state = s.snapshot();
        if (state.carry_target_entity_id == YardCrane::kShackleEntity) {
            (void)s.set_move_input(0.0, 0.0);
            if (!s.request_pick_up()) {
                return false;
            }
            (void)s.advance_frame(0.3);
            return s.snapshot().carrying_entity_id == YardCrane::kShackleEntity;
        }
        const auto shackle = body_at(s, YardCrane::kShackleEntity);
        const double dx = shackle.x - state.player_position.x;
        const double dz = shackle.z - state.player_position.z;
        const double d = std::hypot(dx, dz);
        const double go = d > 0.7 ? 0.4 / d : 0.0;
        (void)s.set_move_input(dx * go, dz * go);
        if (d > 1.0e-3) {
            (void)s.set_facing(dx / d, dz / d);
        }
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
    return false;
}

// Facing (fx, fz), the shackle in the hands is hooked on the eye on `load`
// (once the hands have come round to the facing).
bool hook_on(Simulation &s, const double fx, const double fz, const std::uint64_t load) {
    (void)s.set_facing(fx, fz);
    (void)s.advance_frame(1.5);
    const auto state = s.snapshot();
    if (state.rig_action != 1 || state.rig_target_entity_id != load || !s.request_rig()) {
        const auto hook = s.yard_crane_state().hook;
        const auto shackle = body_at(s, YardCrane::kShackleEntity);
        std::cout << "  crane: HOOK not offered onto " << load << ": rig " << int(state.rig_action) << " onto "
                  << state.rig_target_entity_id << ", carrying " << state.carrying_entity_id << " at ("
                  << state.player_position.x << ", " << state.player_position.y << ", " << state.player_position.z
                  << "), the shackle at (" << shackle.x << ", " << shackle.y << ", " << shackle.z << "), the hook at ("
                  << hook.x << ", " << hook.y << ", " << hook.z << ")\n";
        return false;
    }
    (void)s.advance_frame(0.5);
    return s.yard_crane_state().load_entity == load;
}

// The sling off a landed load: its end taken off the eye, and let go.
bool unrig(Simulation &s) {
    (void)s.advance_frame(1.0);
    if (s.snapshot().rig_action != 2 || !s.request_rig()) {
        return false;
    }
    (void)s.advance_frame(0.5);
    if (s.snapshot().carrying_entity_id != YardCrane::kShackleEntity || !s.request_set_down()) {
        return false;
    }
    (void)s.advance_frame(1.0);
    return s.yard_crane_state().load_entity == 0;
}

// Hoists until the body `entity` is at least y up.
bool hoist_load_to(Simulation &s, const std::uint64_t entity, const double y, const double budget) {
    const auto ticks = static_cast<int>(budget * static_cast<double>(Simulation::kTickRateHz));
    for (int tick = 0; tick < ticks; ++tick) {
        if (body_at(s, entity).y >= y) {
            (void)s.set_yard_crane_hoist(0.0);
            return true;
        }
        if (tick > 90 && s.yard_crane_state().hoist_stalled) {
            break;
        }
        (void)s.set_yard_crane_hoist(1.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_yard_crane_hoist(0.0);
    return false;
}

// A container from the laydown to the 44 m dock, rigged from its roof and
// ridden up, from the game's start; returns where it and its rider came to
// rest.
std::array<double, 6> land_container(Simulation &s) {
    const std::uint64_t box = laydown_piece(LooseStock::Kind::Container);
    require(box != 0, "crane: the laydown's container");
    (void)s.advance_frame(1.0);
    require(take_remote(s), "crane: the remote taken");
    const auto at = body_at(s, box);
    const double roof = at.y + 1.295;
    // The hook over the container's eye, let down until its shackle lies on
    // the roof; the remote let go to climb.
    require(work(s, true) && hook_over(s, at.x, at.z, 90.0), "crane: the hook over the container");
    require(hoist_to(s, roof + YardCrane::kSlingM - 0.4, 60.0), "crane: the hook let down over the roof");
    require(work(s, false), "crane: the remote let go to climb");
    // Up onto the roof from its east side; the shackle; to the eye with it.
    require(walk_to(s, at.x + 2.0, at.z + 0.5, 20.0, 0.1) && c4_hang_up(s, -1.0, 0.0, roof - 0.2, "crane: onto the roof"),
            "crane: up onto the container's roof");
    require(walk_to(s, at.x + 0.75, at.z, 6.0, 0.08) && take_shackle(s, 6.0), "crane: the sling's shackle taken");
    require(walk_to(s, at.x + 0.7, at.z, 6.0, 0.1) && hook_on(s, -1.0, 0.0, box), "crane: HOOK onto the container's eye");
    // Up with the rider aboard, over the dock, down onto it.
    require(work(s, true), "crane: worked from the roof");
    const auto dock = YardCrane::kDockSpecs[2];
    require(hoist_to(s, dock.top_y + 2.59 + YardCrane::kSlingM + 3.0, 90.0), "crane: the container hoisted");
    const auto high = s.snapshot();
    require(high.support_entity_id == box && high.player_position.y > dock.top_y + 2.5,
            "crane: the rider stands on the container, above the dock");
    require(hook_over(s, -29.5, 0.5 * (dock.z_min + dock.z_max), 180.0), "crane: the container over the dock");
    require(lower_to_rest(s, 90.0), "crane: the container let down until the rope slackens");
    const auto landed = body_at(s, box);
    const auto rider = s.snapshot().player_position;
    std::cout << "  crane: the container ridden up to y " << high.player_position.y << " and landed at (" << landed.x
              << ", " << landed.y << ", " << landed.z << "), its rider at y " << rider.y << "\n";
    require(std::abs(landed.y - (dock.top_y + 1.295)) < 0.06 && landed.x < YardCrane::kFaceX &&
                landed.x > dock.x_out && landed.z > dock.z_min && landed.z < dock.z_max,
            "crane: the container rests on the 44 m dock");
    require(work(s, false) && unrig(s), "crane: the sling unhooked and let go, the hook free");
    return {landed.x, landed.y, landed.z, rider.x, rider.y, rider.z};
}

} // namespace

void run_yard_crane() {
    // Parked, the jib holds its heading and the hook hangs still; without
    // its remote it cannot be worked.
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        (void)s.advance_frame(1.0);
        const auto first = s.yard_crane_state();
        (void)s.advance_frame(10.0);
        const auto after = s.yard_crane_state();
        const double swung = std::hypot(after.hook.x - first.hook.x, after.hook.z - first.hook.z);
        std::cout << "  crane: parked 10 s, the jib turned " << after.slew_rad - first.slew_rad << " rad, the hook moved "
                  << swung << " m, its rope " << after.hoist_m << " m pulling " << after.hoist_n << " N\n";
        require(std::abs(after.slew_rad - first.slew_rad) < 1.0e-3 && swung < 0.02, "crane: parked, it stands still");
        require(!after.has_remote && !s.request_yard_crane_operate(), "crane: without its remote it cannot be worked");
    }
    // The remote taken, the stick sends the hook (the slew and the trolley
    // share it), the buttons hoist; let go, every drive stops.
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        (void)s.advance_frame(1.0);
        require(take_remote(s), "crane: TAKE takes the remote from its cabinet");
        require(work(s, true), "crane: the remote's switch takes the crane");
        const auto start = s.yard_crane_state();
        // The hook is south of the mast: the stick east sends it east (the
        // slew), the stick north toward the mast (the trolley in). It swings
        // on its 50 m of rope; where the stick sends it is where the trolley
        // takes the rope's top.
        stick_for(s, 1.0, 0.0, 6.0);
        const auto slewed = s.yard_crane_state();
        stick_for(s, 0.0, -1.0, 6.0);
        const auto run_in = s.yard_crane_state();
        (void)s.set_yard_crane_hoist(1.0);
        const auto hits_before = s.impacts();
        const std::uint64_t seen = hits_before.empty() ? 0 : hits_before.back().sequence;
        (void)s.advance_frame(5.0);
        const double running = s.yard_crane_state().hoist_rate;
        (void)s.set_yard_crane_hoist(0.0);
        const auto hoisted = s.yard_crane_state();
        (void)s.advance_frame(3.0);
        const auto still = s.yard_crane_state();
        // The drum spools up and down (kHoistAccelMps2): started and stopped,
        // the hook block is never jerked hard enough to be heard as a hit.
        int hook_hits = 0;
        for (const auto &hit : s.impacts()) {
            hook_hits += hit.sequence > seen && hit.entity == YardCrane::kHookEntity ? 1 : 0;
        }
        std::cout << "  crane: the drum at " << running << " m/s after 5 s, " << still.hoist_rate << " stopped; "
                  << hook_hits << " hits on the hook block\n";
        require(std::abs(running - YardCrane::kHoistMps) < 1.0e-3 && still.hoist_rate == 0.0,
                "crane: the drum runs up to the hoist's speed and comes to rest when let go");
        require(hook_hits == 0, "crane: starting and stopping the hoist does not jerk the hook block");
        std::cout << "  crane: slewed " << slewed.slew_rad - start.slew_rad << " rad (the trolley "
                  << slewed.rope_top.x - start.rope_top.x << " m east), then the trolley "
                  << slewed.rope_top.z - run_in.rope_top.z << " m north (" << slewed.trolley_m - run_in.trolley_m
                  << " m in), hoisted " << hoisted.hook.y - run_in.hook.y << " m in 5 s\n";
        require(std::abs(slewed.slew_rad - start.slew_rad) > 0.1 && slewed.rope_top.x - start.rope_top.x > 2.0,
                "crane: the stick east slews the hook east");
        require(slewed.rope_top.z - run_in.rope_top.z > 2.0 && run_in.trolley_m < slewed.trolley_m,
                "crane: the stick north sends the hook north, the trolley in toward the mast");
        require(hoisted.hook.y - run_in.hook.y > 4.0, "crane: RAISE hoists the hook");
        require(std::abs(still.slew_rate) < 1.0e-6 && std::abs(still.trolley_rate) < 1.0e-6,
                "crane: let go, every drive stops");
        require(work(s, false), "crane: the switch lets go of it");
    }
    // A container from the laydown onto the 44 m dock, ridden up; twice, the
    // same to the bit. Then, from the dock, in onto the 44 m deck to the
    // forklift the crane put there: driven, its forks under the skid crate
    // before it, raised.
    double forked_y = 0.0;
    {
        Simulation first(scraperx::sim::InitialSpawn::ExteriorGrade);
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        const auto once = land_container(first);
        const auto again = land_container(s);
        require(once == again, "crane: the same inputs land the container and its rider alike, to the bit");
        const auto dock = YardCrane::kDockSpecs[2];
        const auto truck = body_at(s, Forklift::kDeck44ChassisEntity);
        std::uint64_t pallet = 0;
        const auto &table = LooseStock::placements();
        for (std::size_t i = 0; i < table.size(); ++i) {
            if (table[i].kind == LooseStock::Kind::SkidCrate && std::abs(table[i].y - dock.top_y) < 0.1F) {
                pallet = LooseStock::kFirstEntity + i;
            }
        }
        require(pallet != 0, "crane: the skid crate before the 44 m deck's forklift");
        const auto load0 = body_at(s, pallet);
        // Down off the container's roof onto the dock, in through the face,
        // round to the truck's west side and onto its floor at the wheel.
        require(walk_to(s, -27.5, once[5] + 2.0, 10.0, 0.2) && walk_to(s, -23.5, -141.0, 12.0, 0.2) &&
                    walk_to(s, truck.x - 1.4, truck.z + 0.15, 12.0, 0.1) &&
                    walk_to(s, truck.x, truck.z + 0.15, 6.0, 0.08),
                "crane: off the dock, in onto the 44 m deck, aboard its forklift");
        (void)s.set_facing(0.0, -1.0);
        (void)s.advance_frame(0.5);
        require(s.forklift_state().at_controls && s.forklift_state().chassis_entity == Forklift::kDeck44ChassisEntity &&
                    s.request_forklift_action(),
                "crane: DRIVE the 44 m deck's forklift");
        (void)s.advance_frame(0.2);
        // In until the forks are under the pallet's deck, the carriage short
        // of it; RAISE.
        int ticks = 0;
        while (body_at(s, Forklift::kDeck44ChassisEntity).z > load0.z + 1.75 && ticks++ < 900) {
            (void)s.set_move_input(0.0, -0.25);
            (void)s.set_facing(0.0, -1.0);
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
        }
        (void)s.set_move_input(0.0, 0.0);
        (void)s.advance_frame(1.5);
        (void)s.set_forklift_lift(1.0);
        (void)s.advance_frame(1.0);
        (void)s.set_forklift_lift(0.0);
        (void)s.advance_frame(0.5);
        const auto lifted = body_at(s, pallet);
        forked_y = lifted.y - load0.y;
        std::cout << "  crane: up on the 44 m deck its forklift lifted the skid crate " << forked_y << " m\n";
        require(forked_y > 0.25, "crane: up on the 44 m deck, its forklift lifts the skid crate by its pallet");
    }
    // The crane's forklift, its driver at the wheel, rigged by its guard's
    // eye, hoisted, landed on the 22 m dock and driven in onto the deck.
    double driven_x = 0.0;
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        (void)s.advance_frame(1.0);
        require(take_remote(s), "crane: the remote taken");
        const auto truck = body_at(s, Forklift::kCraneChassisEntity);
        // Facing east, its eye half a metre behind the frame's middle.
        const double eye_x = truck.x + Forklift::kEyeZ;
        const double eye_z = truck.z;
        // The hook just clear of the truck's north side, its shackle hanging
        // at knee height there: the sling then reaches the hands at the wheel
        // with half a metre to spare, so the sway of the walk aboard never
        // pulls it taut out of them.
        require(work(s, true) && hook_over(s, eye_x, eye_z - 1.1, 90.0), "crane: the hook by the forklift");
        require(hoist_to(s, 0.6 + YardCrane::kSlingM, 60.0), "crane: the hook let down by the forklift");
        require(work(s, false), "crane: the remote let go to rig");
        // The shackle taken from the ground, carried aboard to the wheel.
        require(walk_to(s, eye_x + 0.3, eye_z - 2.0, 20.0, 0.1) && take_shackle(s, 6.0), "crane: the sling's shackle taken");
        const double station_x = truck.x - 0.15;
        require(walk_to(s, station_x, truck.z, 6.0, 0.08) &&
                    s.snapshot().support_entity_id == Forklift::kCraneChassisEntity &&
                    s.snapshot().carrying_entity_id == YardCrane::kShackleEntity,
                "crane: aboard the forklift at its wheel, the shackle in hand");
        require(hook_on(s, -1.0, 0.0, Forklift::kCraneChassisEntity), "crane: HOOK onto the forklift's eye");
        // Up, the driver aboard, over the dock, down onto it.
        const auto dock = YardCrane::kDockSpecs[1];
        require(work(s, true) && hoist_load_to(s, Forklift::kCraneChassisEntity, dock.top_y + 1.5, 90.0),
                "crane: the forklift hoisted, its driver aboard");
        const auto high = s.snapshot();
        require(high.support_entity_id == Forklift::kCraneChassisEntity, "crane: the driver rides the forklift up");
        require(hook_over(s, -31.3, 0.5 * (dock.z_min + dock.z_max), 180.0), "crane: the forklift over the dock");
        require(lower_until_landed(s, 90.0), "crane: the forklift let down until its weight is off the rope");
        const auto landed = body_at(s, Forklift::kCraneChassisEntity);
        std::cout << "  crane: the forklift ridden up to y " << high.player_position.y << " and landed at (" << landed.x
                  << ", " << landed.y << ", " << landed.z << ")\n";
        require(std::abs(landed.y - dock.top_y) < 0.08 && landed.z > dock.z_min && landed.z < dock.z_max,
                "crane: the forklift stands on the 22 m dock");
        require(work(s, false) && unrig(s), "crane: the sling off the forklift's eye");
        // The hook raised clear, then the driver drives it in.
        require(work(s, true) && hoist_to(s, s.yard_crane_state().hook.y + 3.0, 20.0) && work(s, false),
                "crane: the hook raised clear of the truck");
        (void)s.set_facing(1.0, 0.0);
        (void)s.advance_frame(0.3);
        require(s.forklift_state().at_controls && s.forklift_state().index == 1 && s.request_forklift_action(),
                "crane: DRIVE the crane's forklift on the dock");
        (void)s.advance_frame(0.2);
        for (int tick = 0; tick < 6 * static_cast<int>(Simulation::kTickRateHz); ++tick) {
            (void)s.set_move_input(0.6, 0.0);
            (void)s.set_facing(1.0, 0.0);
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
        }
        (void)s.set_move_input(0.0, 0.0);
        (void)s.advance_frame(2.0);
        const auto in = body_at(s, Forklift::kCraneChassisEntity);
        driven_x = in.x;
        std::cout << "  crane: driven in to (" << in.x << ", " << in.y << ", " << in.z << ")\n";
        require(in.x > YardCrane::kFaceX + 2.0 && std::abs(in.y - dock.top_y) < 0.08 &&
                    s.snapshot().support_entity_id == Forklift::kCraneChassisEntity,
                "crane: the forklift driven off the dock onto the 22 m deck, its driver aboard");
    }
    // The 8 t plate stack: rigged, the hoist pulls all it has and stalls;
    // the plates stay down on their sleepers.
    double stalled_n = 0.0;
    {
        const std::uint64_t plates = laydown_piece(LooseStock::Kind::PlateStack);
        require(plates != 0, "crane: the laydown's plate stack");
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        (void)s.advance_frame(1.0);
        require(take_remote(s), "crane: the remote taken");
        const auto at = body_at(s, plates);
        // The hook let down beside the stack, its shackle on the ground;
        // round the stack's west end to it, and back to the eye.
        require(work(s, true) && hook_over(s, at.x, at.z + 1.5, 90.0), "crane: the hook beside the plate stack");
        require(hoist_to(s, 3.0, 60.0) && work(s, false), "crane: the hook let down beside the plates");
        require(walk_to(s, at.x - 3.0, at.z - 2.0, 20.0, 0.2) && walk_to(s, at.x - 3.0, at.z + 2.2, 10.0, 0.2) &&
                    take_shackle(s, 8.0),
                "crane: the shackle taken");
        require(walk_to(s, at.x, at.z + 1.2, 6.0, 0.1) && hook_on(s, 0.0, -1.0, plates), "crane: HOOK onto the plates");
        require(work(s, true), "crane: worked");
        (void)hoist_load_to(s, plates, at.y + 0.5, 15.0);
        const auto crane = s.yard_crane_state();
        stalled_n = crane.hoist_n;
        const double rose = body_at(s, plates).y - at.y;
        std::cout << "  crane: the plate stack rose " << rose << " m, the hoist pulling " << crane.hoist_n
                  << " N, stalled " << crane.hoist_stalled << "\n";
        require(crane.hoist_stalled && rose < 0.02 && crane.hoist_n > 0.95 * YardCrane::kHoistForceN,
                "crane: too heavy, the hoist stalls at its full pull and the plates stay down");
    }
    std::cout << "PASS scraperx_sim crane: forked_up_44_m=" << forked_y << " forklift_x=" << driven_x
              << " stall_n=" << stalled_n << "\n";
}
