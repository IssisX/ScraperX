// The yard crane (src/sim/yard_crane.hpp): parked it stands still; its
// remote is taken from the cabinet at the mast's foot; worked, the stick
// sends the hook and the buttons hoist; a container rigged from its roof is
// ridden up and landed on the 44 m dock, alike twice to the bit; the crane's
// forklift is lifted with its driver aboard onto the 22 m dock and driven in
// onto the deck; the 8 t plate stack stalls the hoist on the ground.
#include "sim/bay.hpp"
#include "sim/forklift.hpp"
#include "sim/loose_stock.hpp"
#include "sim/yard_crane.hpp"
#include "sim_tests.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <iostream>

using scraperx::sim::Bay;
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
        // Then, as a player does, step in toward the wheel until DRIVE is
        // offered: the container now lands where the rider's planted feet
        // carry it, and the walk above can end with the hands on the truck's
        // overhead guard.
        for (int i = 0; i < 4 * 90 && !(s.forklift_state().at_controls &&
                                         s.forklift_state().chassis_entity == Forklift::kDeck44ChassisEntity);
             ++i) {
            // Hands that caught the truck's overhead guard let go first.
            if (s.snapshot().traversal_state != scraperx::sim::TraversalState::None) {
                (void)s.request_release();
            }
            const auto t = body_at(s, Forklift::kDeck44ChassisEntity);
            const auto me = s.snapshot().player_position;
            const double ex = t.x - me.x, ez = t.z + 0.15 - me.z, d = std::hypot(ex, ez);
            (void)s.set_move_input(d > 0.03 ? std::min(0.3, 2.0 * d) * ex / d : 0.0,
                                   d > 0.03 ? std::min(0.3, 2.0 * d) * ez / d : 0.0);
            (void)s.set_facing(0.0, -1.0);
            (void)s.advance_frame(scraperx::sim::Simulation::kFixedStepSeconds);
        }
        (void)s.set_move_input(0.0, 0.0);
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
        // Lowered as the game lowers it, on to the slack-rope switch: the
        // block may come down on the truck, and must not foul it.
        require(lower_to_rest(s, 90.0), "crane: the forklift let down until the rope slackens");
        const auto landed = body_at(s, Forklift::kCraneChassisEntity);
        std::cout << "  crane: the forklift ridden up to y " << high.player_position.y << " and landed at (" << landed.x
                  << ", " << landed.y << ", " << landed.z << ")\n";
        require(std::abs(landed.y - dock.top_y) < 0.08 && landed.z > dock.z_min && landed.z < dock.z_max,
                "crane: the forklift stands on the 22 m dock");
        require(work(s, false) && unrig(s), "crane: the sling off the forklift's eye");
        // The hook raised clear, then the driver drives it in.
        require(work(s, true) && hoist_to(s, s.yard_crane_state().hook.y + 3.0, 20.0) && work(s, false),
                "crane: the hook raised clear of the truck");
        const auto after_hoist = body_at(s, Forklift::kCraneChassisEntity);
        std::cout << "  crane: the hook raised 3 m off the landed truck, the truck moved " << after_hoist.y - landed.y
                  << " m up\n";
        require(std::abs(after_hoist.y - landed.y) < 0.02 && s.yard_crane_state().load_entity == 0,
                "crane: the hook raised off the landed truck does not lift it");
        (void)s.set_facing(1.0, 0.0);
        (void)s.advance_frame(0.3);
        require(s.forklift_state().at_controls && s.forklift_state().index == 1 && s.request_forklift_action(),
                "crane: DRIVE the crane's forklift on the dock");
        (void)s.advance_frame(0.2);
        // In along the dock's middle, steered for a point on it 2 m ahead (it
        // lands turned as it hung, and the dock is 4.4 m wide), until 2.5 m
        // in off the face.
        const double mid_z = 0.5 * (dock.z_min + dock.z_max);
        for (int tick = 0; tick < 8 * static_cast<int>(Simulation::kTickRateHz); ++tick) {
            const auto at = body_at(s, Forklift::kCraneChassisEntity);
            if (at.x > YardCrane::kFaceX + 2.5) {
                break;
            }
            const auto q = s.kit_body_rotation(s.kit_body_index(Forklift::kCraneChassisEntity));
            const double yaw = 2.0 * std::atan2(q.y, q.w);
            const double fx = std::sin(yaw);
            const double fz = std::cos(yaw);
            const double right = -2.0 * fz + (mid_z - at.z) * fx;
            const double ahead = 2.0 * fx + (mid_z - at.z) * fz;
            const double steer = std::clamp(2.0 * std::atan2(right, ahead), -1.0, 1.0);
            (void)s.set_facing(fx, fz);
            (void)s.set_move_input(fx * 0.6 - fz * steer, fz * 0.6 + fx * steer);
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

namespace {

// A piece of stock lying at (x, y, z), its middle within a few centimeters:
// the steel laid for the magnet (LooseStock::lay_crane_steel) and the
// laydown's plate stack.
std::uint64_t stock_at(const Simulation &s, const double x, const double y, const double z) {
    for (std::uint32_t i = 0; i < s.kit_body_count(); ++i) {
        const std::uint64_t entity = s.kit_body_entity(i);
        const auto p = s.kit_body_position(i);
        if (scraperx::sim::kit::is_stock_entity(entity) && std::abs(p.x - x) < 0.3 && std::abs(p.y - y) < 0.1 &&
            std::abs(p.z - z) < 0.3) {
            return entity;
        }
    }
    return 0;
}

// The device switch, until the magnet reads `on`.
bool magnet(Simulation &s, const bool on) {
    if (s.yard_crane_state().magnet_on == on) {
        return true;
    }
    if (!s.request_yard_crane_device()) {
        return false;
    }
    (void)s.advance_frame(0.1);
    return s.yard_crane_state().magnet_on == on;
}

// Worked, the device on the sling (`device`, of `kg`) hoisted `rise` m, sent
// over (x, z) and let down on to what is there until the sling goes slack:
// it sits on it (the hook block still hangs over it).
bool device_onto(Simulation &s, const std::uint64_t device, const double kg, const double rise, const double x,
                 const double z) {
    if (!(hoist_to(s, s.yard_crane_state().hook.y + rise, 40.0) && hook_over(s, x, z, 120.0))) {
        return false;
    }
    // The operator watches the device, not the trolley: once it hangs still,
    // the trolley is set again by how far off the spot it hangs, and it is
    // let down only once it hangs still within 0.1 m of it.
    const std::uint32_t body = s.kit_body_index(device);
    // Still for 2 s: the hook block swings on 50 m of rope, 15 s a swing, and
    // stands still for a moment at either end of it.
    const int hold = 2 * static_cast<int>(Simulation::kTickRateHz);
    const auto settle = [&s, body, hold](const double tolerance, const double tx, const double tz) {
        int still = 0;
        for (int tick = 0; tick < 90 * static_cast<int>(Simulation::kTickRateHz) && still < hold; ++tick) {
            const auto m = s.kit_body_position(body);
            const auto v = s.kit_body_velocity(body);
            still = std::hypot(m.x - tx, m.z - tz) < tolerance && std::hypot(v.x, v.z) < 0.02 ? still + 1 : 0;
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
        }
        return still >= hold;
    };
    bool there = settle(0.6, x, z);
    for (int pass = 0; there && pass < 3; ++pass) {
        const auto m = s.kit_body_position(body);
        if (std::hypot(m.x - x, m.z - z) < 0.1) {
            break;
        }
        const auto top = s.yard_crane_state().rope_top;
        there = hook_over(s, top.x + (x - m.x), top.z + (z - m.z), 60.0) && settle(0.6, x, z);
    }
    const auto over = s.kit_body_position(body);
    if (!there || std::hypot(over.x - x, over.z - z) >= 0.1) {
        std::cout << "  crane: " << device << " not still over (" << x << ", " << z << "): at (" << over.x << ", "
                  << over.z << ")\n";
        return false;
    }
    const double hanging = kg * 9.81;
    for (int tick = 0; tick < 40 * static_cast<int>(Simulation::kTickRateHz); ++tick) {
        (void)s.set_yard_crane_hoist(-0.3);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        if (tick > 10 && s.yard_crane_state().sling_n < 0.2 * hanging) {
            (void)s.set_yard_crane_hoist(0.0);
            (void)s.advance_frame(0.3);
            return true;
        }
    }
    (void)s.set_yard_crane_hoist(0.0);
    const auto crane = s.yard_crane_state();
    const auto m = body_at(s, device);
    std::cout << "  crane: " << device << " not landed over (" << x << ", " << z << "): at (" << m.x << ", " << m.y
              << ", " << m.z << "), the sling pulling " << crane.sling_n << " N, the hook at y " << crane.hook.y << "\n";
    return false;
}

bool magnet_onto(Simulation &s, const double x, const double z) {
    return device_onto(s, YardCrane::kMagnetEntity, YardCrane::kMagnetKg, 2.0, x, z);
}

// The grips' two lists at a save's end ([n][n loads][n][n ratings]): how many
// grips it holds, 0 if it does not end in them.
std::size_t saved_grips(const std::vector<std::uint8_t> &bytes) {
    for (std::size_t n = 1; n <= 16 && bytes.size() >= 8 + 8 * n; ++n) {
        std::uint32_t loads = 0;
        std::uint32_t ratings = 0;
        std::memcpy(&loads, bytes.data() + bytes.size() - (8 + 8 * n), 4);
        std::memcpy(&ratings, bytes.data() + bytes.size() - (4 + 4 * n), 4);
        if (loads == n && ratings == n) {
            return n;
        }
    }
    return 0;
}

// The save as an older build wrote it: without the world's last `drop` kit
// bodies (their entities and states) and with only its first `keep` grips (0:
// from before the grips, its lists ending at the lifts' targets). The layout
// is Simulation::save_game's: the format key, the tick, the checkpoint's
// position, crouch, commits, deaths and carried entity, then the bodies'
// entities and states.
std::vector<std::uint8_t> older_save(const std::vector<std::uint8_t> &bytes, const std::size_t drop,
                                     const std::size_t keep) {
    const std::size_t n = saved_grips(bytes);
    const std::size_t section = bytes.size() - (8 + 8 * n);
    const std::size_t at = 8 + 8 + sizeof(JPH::RVec3) + 1 + 8 + 8 + 8;
    std::uint32_t entities = 0;
    std::memcpy(&entities, bytes.data() + at, 4);
    const std::size_t states_at = at + 4 + 8 * std::size_t{entities};
    std::uint32_t states = 0;
    std::memcpy(&states, bytes.data() + states_at, 4);
    const std::size_t rest_at = states_at + 4 + sizeof(scraperx::sim::kit::Kit::BodyState) * std::size_t{states};
    require(n > 0 && states == entities && drop <= entities && rest_at <= section,
            "saves: the save's body lists read as the writer lays them out");
    const auto fewer = static_cast<std::uint32_t>(entities - drop);
    const auto *bytes_of_fewer = reinterpret_cast<const std::uint8_t *>(&fewer);
    std::vector<std::uint8_t> out(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(at));
    out.insert(out.end(), bytes_of_fewer, bytes_of_fewer + 4);
    out.insert(out.end(), bytes.begin() + static_cast<std::ptrdiff_t>(at + 4),
               bytes.begin() + static_cast<std::ptrdiff_t>(at + 4 + 8 * std::size_t{fewer}));
    out.insert(out.end(), bytes_of_fewer, bytes_of_fewer + 4);
    out.insert(out.end(), bytes.begin() + static_cast<std::ptrdiff_t>(states_at + 4),
               bytes.begin() + static_cast<std::ptrdiff_t>(states_at + 4 + sizeof(scraperx::sim::kit::Kit::BodyState) *
                                                                                  std::size_t{fewer}));
    out.insert(out.end(), bytes.begin() + static_cast<std::ptrdiff_t>(rest_at),
               bytes.begin() + static_cast<std::ptrdiff_t>(section));
    if (keep == 0 || keep > n) {
        return out;
    }
    const auto count = static_cast<std::uint32_t>(keep);
    const std::uint8_t *loads = bytes.data() + section + 4;
    const std::uint8_t *ratings = loads + 4 * n + 4;
    const auto *bytes_of_count = reinterpret_cast<const std::uint8_t *>(&count);
    out.insert(out.end(), bytes_of_count, bytes_of_count + 4);
    out.insert(out.end(), loads, loads + 4 * keep);
    out.insert(out.end(), bytes_of_count, bytes_of_count + 4);
    out.insert(out.end(), ratings, ratings + 4 * keep);
    return out;
}

} // namespace

// The yard crane's lifting magnet (yard_crane.hpp): rigged by HOOKing the
// sling on its bail; switched on from the remote, lowered on to the two slabs,
// it takes the top one (steel flat under its whole face) and lifts it alone;
// off, it lets the slab fall where it hangs. A beam a quarter of its face wide
// it holds by the points over it, and so less hard. The 8 t plate stack tears
// off it. A save made while it holds a slab up loads holding it; one made
// before the grips (without them at its end) loads holding nothing.
void run_yard_crane_magnet() {
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    const std::uint64_t top = stock_at(s, -64.0, 0.458, -124.0);
    const std::uint64_t bottom = stock_at(s, -64.0, 0.154, -124.0);
    const std::uint64_t beam = stock_at(s, -72.0, 0.154, -126.5);
    const std::uint64_t plates = laydown_piece(LooseStock::Kind::PlateStack);
    require(top != 0 && bottom != 0 && beam != 0 && plates != 0, "magnet: the slabs, the beam and the plate stack");
    const auto at = body_at(s, YardCrane::kMagnetEntity);
    require(std::abs(at.y - 0.244) < 0.02, "magnet: it lies on its face in the yard");
    // The crane's steel and its two devices are the kit's last six bodies
    // but those built since, after them: the machines' weights, the Matron's
    // throwables and the bay's five (bay.hpp), each built after what was
    // before it, so saves from before them line up.
    const auto count = s.kit_body_count();
    constexpr std::uint32_t kBayBodies = 5;
    const auto weights =
        static_cast<std::uint32_t>(LooseStock::kMachineWeights + LooseStock::kMatronThrowables) + kBayBodies;
    require(count > 6 + weights && s.kit_body_entity(count - 1) == Bay::kShackleEntity &&
                scraperx::sim::kit::is_stock_entity(s.kit_body_entity(count - 1 - kBayBodies)) &&
                s.kit_body_entity(count - 1 - weights) == YardCrane::kSpreaderEntity &&
                s.kit_body_entity(count - 2 - weights) == YardCrane::kMagnetEntity &&
                scraperx::sim::kit::is_stock_entity(s.kit_body_entity(count - 6 - weights)),
            "magnet: the crane's steel, the magnet and the spreader, then the machines' weights, her throwables and the bay, are the kit's last bodies");
    // As the world began: a build from before the grips saved it so, without
    // those six bodies and the weights after them.
    const std::vector<std::uint8_t> as_begun = s.save_game();
    const auto spreader0 = body_at(s, YardCrane::kSpreaderEntity);
    require(take_remote(s), "magnet: the remote taken");
    // The hook let down over the magnet until its shackle lies on the bail;
    // the shackle taken from beside it, and HOOKed on the bail's eye.
    require(work(s, true) && hook_over(s, at.x, at.z, 120.0) && hoist_to(s, 0.55 + YardCrane::kSlingM, 60.0) &&
                work(s, false),
            "magnet: the hook let down over the magnet");
    require(walk_to(s, at.x, at.z - 1.3, 20.0, 0.1) && take_shackle(s, 8.0), "magnet: the sling's shackle taken");
    require(walk_to(s, at.x, at.z - 1.0, 6.0, 0.08) && hook_on(s, 0.0, 1.0, YardCrane::kMagnetEntity),
            "magnet: HOOK onto the magnet's bail");
    require(work(s, true) && s.yard_crane_state().device == 1 && !s.yard_crane_state().magnet_on,
            "magnet: worked, the device on the sling is the magnet, off");
    // On, and on to the slabs: it takes the top one.
    require(magnet(s, true), "magnet: MAGNET ON");
    const auto slab0 = body_at(s, top);
    require(magnet_onto(s, slab0.x, slab0.z), "magnet: let down on to the slabs");
    (void)s.advance_frame(0.5);
    const auto took = s.yard_crane_state();
    std::cout << "  magnet: on the slabs it holds " << took.holding_entity << " (the top " << top << "), to "
              << took.hold_rated_n << " N\n";
    require(took.holding_entity == top && std::abs(took.hold_rated_n - YardCrane::kMagnetRatedN) < 1.0,
            "magnet: on, it takes the top slab, its whole face on it");
    require(hoist_load_to(s, top, slab0.y + 1.5, 30.0), "magnet: the top slab lifted");
    (void)s.set_yard_crane_hoist(0.0);
    (void)s.advance_frame(1.0);
    const auto lifted = body_at(s, top);
    const auto stayed = body_at(s, bottom);
    std::cout << "  magnet: the top slab up to y " << lifted.y << ", the bottom one at " << stayed.y << ", the hold "
              << s.yard_crane_state().hold_n << " N\n";
    require(lifted.y > slab0.y + 1.4 && std::abs(stayed.y - 0.154) < 0.02,
            "magnet: the top slab rises alone, the bottom one stays");
    // Saved holding it up: a fresh world loads holding it there; one loaded
    // from a save without the grips at its end holds nothing.
    const std::vector<std::uint8_t> bytes = s.save_game();
    {
        Simulation loaded(scraperx::sim::InitialSpawn::ExteriorGrade);
        require(loaded.load_game(bytes.data(), bytes.size()), "magnet: the save loads");
        (void)loaded.advance_frame(2.0);
        (void)s.advance_frame(2.0);
        const auto here = body_at(s, top);
        const auto there = body_at(loaded, top);
        std::cout << "  magnet: 2 s after the load the slab hangs at y " << there.y << " (" << here.y
                  << " in the world saved)\n";
        require(loaded.yard_crane_state().holding_entity == top && loaded.yard_crane_state().magnet_on &&
                    std::abs(there.y - here.y) < 0.05,
                "magnet: loaded, it holds the slab where it was saved");
        require(saved_grips(bytes) == 2, "magnet: the save ends in its two grips (the magnet's, the spreader's)");
        // As the spreader's build saved it: no machines' weights. Loaded, the
        // slab still hangs on the magnet.
        const std::vector<std::uint8_t> two = older_save(bytes, weights, 2);
        Simulation spreader_build(scraperx::sim::InitialSpawn::ExteriorGrade);
        require(spreader_build.load_game(two.data(), two.size()), "magnet: a save from before the machines' weights loads");
        (void)spreader_build.advance_frame(2.0);
        require(spreader_build.yard_crane_state().holding_entity == top &&
                    std::abs(body_at(spreader_build, top).y - here.y) < 0.05,
                "magnet: loaded from a save from before the machines' weights, it holds the slab");
        // As the magnet's build saved it: no spreader, one grip. Loaded, the
        // slab still hangs on the magnet and the spreader lies as built.
        const std::vector<std::uint8_t> one = older_save(bytes, 1 + weights, 1);
        Simulation magnet_build(scraperx::sim::InitialSpawn::ExteriorGrade);
        require(magnet_build.load_game(one.data(), one.size()), "magnet: a save from before the spreader loads");
        (void)magnet_build.advance_frame(2.0);
        const auto spreader = body_at(magnet_build, YardCrane::kSpreaderEntity);
        require(magnet_build.yard_crane_state().holding_entity == top &&
                    std::abs(body_at(magnet_build, top).y - here.y) < 0.05 &&
                    std::hypot(spreader.x - spreader0.x, spreader.z - spreader0.z) < 0.05 &&
                    std::abs(spreader.y - spreader0.y) < 0.05,
                "magnet: loaded from a save from before the spreader, it holds the slab, the spreader as built");
        // As a build from before the grips saved the world as it began: none
        // of the six bodies, no grips. Loaded, they lie as built, held by
        // nothing.
        const std::vector<std::uint8_t> before = older_save(as_begun, 6 + weights, 0);
        Simulation older(scraperx::sim::InitialSpawn::ExteriorGrade);
        require(older.load_game(before.data(), before.size()), "magnet: a save from before the grips loads");
        (void)older.advance_frame(2.0);
        const auto laid = body_at(older, top);
        const auto lying = body_at(older, YardCrane::kMagnetEntity);
        require(older.yard_crane_state().holding_entity == 0 && std::abs(laid.y - slab0.y) < 0.02 &&
                    std::hypot(laid.x - slab0.x, laid.z - slab0.z) < 0.02 && std::abs(lying.y - at.y) < 0.02 &&
                    std::hypot(lying.x - at.x, lying.z - at.z) < 0.02,
                "magnet: loaded from a save from before the grips, the slabs and the magnet lie as built");
    }
    // Off: the slab falls where it hangs.
    require(magnet(s, false), "magnet: MAGNET OFF");
    (void)s.advance_frame(2.0);
    const auto fell = body_at(s, top);
    std::cout << "  magnet: off, the slab fell to y " << fell.y << "\n";
    require(s.yard_crane_state().holding_entity == 0 && fell.y < lifted.y - 1.0,
            "magnet: off, it lets the slab fall where it hangs");
    // A beam, 0.3 m wide under a 1.2 m face: held by the points over it.
    const auto beam0 = body_at(s, beam);
    require(magnet(s, true) && magnet_onto(s, beam0.x, beam0.z), "magnet: on, let down on to the beam");
    (void)s.advance_frame(0.5);
    const auto on_beam = s.yard_crane_state();
    std::cout << "  magnet: on the beam it holds " << on_beam.holding_entity << " to " << on_beam.hold_rated_n << " N\n";
    require(on_beam.holding_entity == beam && on_beam.hold_rated_n < 0.6 * YardCrane::kMagnetRatedN &&
                on_beam.hold_rated_n > 0.3 * YardCrane::kMagnetRatedN,
            "magnet: a beam narrower than its face it holds by the points over it, less hard");
    require(hoist_load_to(s, beam, beam0.y + 1.0, 30.0) && magnet(s, false), "magnet: the beam lifted and let go");
    (void)s.advance_frame(2.0);
    // The 8 t plate stack, its whole face on it: torn off as the hoist pulls.
    const auto stack0 = body_at(s, plates);
    require(magnet(s, true) && magnet_onto(s, stack0.x, stack0.z), "magnet: on, let down on to the plate stack");
    (void)s.advance_frame(0.5);
    require(s.yard_crane_state().holding_entity == plates, "magnet: it takes the plate stack");
    (void)hoist_to(s, s.yard_crane_state().hook.y + 1.0, 10.0);
    (void)s.advance_frame(1.0);
    const double rose = body_at(s, plates).y - stack0.y;
    std::cout << "  magnet: hoisted, the plate stack rose " << rose << " m and is held by "
              << s.yard_crane_state().holding_entity << "\n";
    require(s.yard_crane_state().holding_entity == 0 && rose < 0.05,
            "magnet: the 8 t plate stack tears off it and stays down");
    // A container's roof is thin sheet: a tenth of the hold, and the empty
    // box tears off it too (the spreader's to lift).
    const std::uint64_t box = laydown_piece(LooseStock::Kind::Container);
    const auto box0 = body_at(s, box);
    require(magnet(s, false) && hoist_to(s, s.yard_crane_state().hook.y + 1.0, 10.0) && magnet(s, true) &&
                device_onto(s, YardCrane::kMagnetEntity, YardCrane::kMagnetKg, 4.0, box0.x, box0.z),
            "magnet: on, let down on to a container's roof");
    (void)s.advance_frame(0.5);
    const auto on_box = s.yard_crane_state();
    std::cout << "  magnet: on the container's roof it holds " << on_box.holding_entity << " to "
              << on_box.hold_rated_n << " N\n";
    require(on_box.holding_entity == box && on_box.hold_rated_n <= YardCrane::kMagnetSheetShare * YardCrane::kMagnetRatedN + 1.0,
            "magnet: a container's sheet roof it holds a tenth as hard");
    (void)hoist_to(s, s.yard_crane_state().hook.y + 1.0, 10.0);
    (void)s.advance_frame(1.0);
    const double box_rose = body_at(s, box).y - box0.y;
    require(s.yard_crane_state().holding_entity == 0 && box_rose < 0.05,
            "magnet: the empty container tears off its face and stays down");
    std::cout << "PASS scraperx_sim magnet: slab_held_n=" << took.hold_rated_n << " beam_held_n="
              << on_beam.hold_rated_n << " stack_rose_m=" << rose << " box_held_n=" << on_box.hold_rated_n << "\n";
}

// The yard crane's container spreader (yard_crane.hpp): rigged by HOOKing the
// sling on its lug from beside the frame; LOCK in the air turns nothing. Let
// down on the laydown's container it sits on the box's corner castings, the
// rope slack; LOCK, and the box rises with it. In the air UNLOCK is refused
// (the interlock); set down, it lets go, and raised the frame leaves the box.
void run_yard_crane_spreader() {
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    const std::uint64_t box = laydown_piece(LooseStock::Kind::Container);
    require(box != 0, "spreader: the laydown's container");
    const auto box0 = body_at(s, box);
    const auto at = body_at(s, YardCrane::kSpreaderEntity);
    require(take_remote(s), "spreader: the remote taken");
    // The hook let down by the frame's side, its shackle on the ground there.
    require(work(s, true) && hook_over(s, at.x + 1.7, at.z, 120.0) && hoist_to(s, 3.4, 60.0) && work(s, false),
            "spreader: the hook let down by the spreader");
    // Round the slabs, the magnet and the beams to it; the shackle taken, and
    // HOOKed on the lug from beside the frame.
    require(walk_to(s, -60.5, -126.0, 20.0, 0.2) && walk_to(s, -60.5, -120.5, 20.0, 0.2) &&
                walk_to(s, at.x + 2.4, at.z - 1.0, 40.0, 0.1) && take_shackle(s, 8.0),
            "spreader: the sling's shackle taken");
    require(walk_to(s, at.x + 1.65, at.z, 6.0, 0.08) && hook_on(s, -1.0, 0.0, YardCrane::kSpreaderEntity),
            "spreader: HOOK onto the spreader's lug");
    require(work(s, true) && s.yard_crane_state().device == 2, "spreader: worked, the device on the sling is the spreader");
    // Not on a box: LOCK turns nothing.
    require(s.request_yard_crane_device(), "spreader: the device switch, worked");
    (void)s.advance_frame(0.3);
    require(s.yard_crane_state().holding_entity == 0 && !s.yard_crane_state().spreader_seated,
            "spreader: off a box, LOCK turns nothing");
    // Up over the container and down on to it.
    require(device_onto(s, YardCrane::kSpreaderEntity, YardCrane::kSpreaderKg, 4.5, box0.x, box0.z),
            "spreader: let down on to the container");
    (void)s.advance_frame(0.5);
    const auto sat = s.yard_crane_state();
    const auto frame = body_at(s, YardCrane::kSpreaderEntity);
    std::cout << "  spreader: on the container at (" << frame.x << ", " << frame.y << ", " << frame.z << "), seated "
              << sat.spreader_seated << ", landed " << sat.spreader_landed << ", the sling " << sat.sling_n << " N\n";
    require(sat.spreader_seated && sat.spreader_landed, "spreader: seated on the container's castings, the rope slack");
    require(s.request_yard_crane_device(), "spreader: LOCK");
    (void)s.advance_frame(0.3);
    const auto locked = s.yard_crane_state();
    require(locked.holding_entity == box && std::abs(locked.hold_rated_n - 4.0 * YardCrane::kTwistlockRatedN) < 1.0,
            "spreader: LOCK turns its twistlocks in the box's castings");
    // Up with it.
    require(hoist_load_to(s, box, box0.y + 2.0, 40.0), "spreader: the container lifted on the spreader");
    (void)s.set_yard_crane_hoist(0.0);
    (void)s.advance_frame(1.0);
    const auto high = body_at(s, box);
    std::cout << "  spreader: the container up to y " << high.y << ", the hold " << s.yard_crane_state().hold_n
              << " N\n";
    // In the air: UNLOCK is refused.
    (void)s.request_yard_crane_device();
    (void)s.advance_frame(0.5);
    require(s.yard_crane_state().holding_entity == box && body_at(s, box).y > box0.y + 1.5,
            "spreader: in the air, UNLOCK is refused and the box stays on it");
    // Set down: UNLOCK lets go, and the frame raised leaves the box.
    const double hanging = YardCrane::kSpreaderKg * 9.81;
    bool down = false;
    for (int tick = 0; tick < 40 * static_cast<int>(Simulation::kTickRateHz) && !down; ++tick) {
        (void)s.set_yard_crane_hoist(-0.3);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        down = tick > 10 && s.yard_crane_state().sling_n < 0.2 * hanging;
    }
    (void)s.set_yard_crane_hoist(0.0);
    (void)s.advance_frame(1.0);
    require(down && s.yard_crane_state().spreader_landed, "spreader: the container set down, the rope slack");
    require(s.request_yard_crane_device(), "spreader: UNLOCK");
    (void)s.advance_frame(0.3);
    require(s.yard_crane_state().holding_entity == 0, "spreader: set down, UNLOCK lets go of the box");
    const auto landed = body_at(s, box);
    require(hoist_to(s, s.yard_crane_state().hook.y + 2.0, 20.0), "spreader: the frame raised off the box");
    (void)s.advance_frame(1.0);
    const auto left = body_at(s, box);
    std::cout << "  spreader: set down at y " << landed.y << ", left at " << left.y << " with the frame raised\n";
    require(std::abs(left.y - landed.y) < 0.02 && std::abs(landed.y - box0.y) < 0.05,
            "spreader: raised, the frame leaves the box where it was set down");
    std::cout << "PASS scraperx_sim spreader: lifted_to_y=" << high.y << " set_down_y=" << landed.y << "\n";
}
