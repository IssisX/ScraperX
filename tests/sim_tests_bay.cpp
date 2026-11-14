// The bay on the 154 m deck (bay.hpp): its leaf a walkway under its motor,
// the fan's noise over a climber's steps, and her trap: held on her palms,
// the leaf takes her but not her and a climber; the chain off its cleat,
// hooked onto the leaf's eye, carries it, and she lets go. Every leg on
// player inputs from the deck spawn.
#include "sim_tests.hpp"

#include "sim/bay.hpp"
#include "sim/matron.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {
using scraperx::sim::Bay;
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;

constexpr int kTrapHolding = 4;

void tick(Simulation &s) {
    (void)s.advance_frame(Simulation::kFixedStepSeconds);
}

// Steps toward (x, z) at a sprint, a walk or crouched until within
// `tolerance`; false when `seconds` run out.
bool go(Simulation &s, const double x, const double z, const double seconds, const bool sprint,
        const double tolerance = 0.4) {
    const auto ticks = static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (int i = 0; i < ticks; ++i) {
        const auto now = s.snapshot().player_position;
        const double dx = x - now.x;
        const double dz = z - now.z;
        const double length = std::hypot(dx, dz);
        if (length < tolerance) {
            (void)s.set_move_input(0.0, 0.0);
            (void)s.set_sprint_input(false);
            return true;
        }
        (void)s.set_move_input(dx / length, dz / length);
        (void)s.set_facing(dx / length, dz / length);
        (void)s.set_sprint_input(sprint);
        tick(s);
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.set_sprint_input(false);
    return false;
}

void stand(Simulation &s, const double seconds) {
    (void)s.set_move_input(0.0, 0.0);
    (void)s.set_sprint_input(false);
    for (int i = 0; i < static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz)); ++i) {
        tick(s);
    }
}

// Wakes her, then, crouched, west over the leaf and down the west band,
// along the south band and up the east band, back along the north band to
// stand east of the bay, while she, having lost the player at its west end,
// keeps the mouth of the way out there. True with the player at (x, z).
bool stage_across(Simulation &s, const double x, const double z) {
    require(s.advance_frame(1.0).accepted, "bay trap: settle");
    if (!go(s, 10.0, -128.5, 4.0, true, 1.0)) {
        return false;
    }
    (void)s.set_crouch_input(true);
    const bool round = go(s, -Matron::kBandX, Matron::kBandNorthZ, 60.0, false) &&
                       go(s, -Matron::kBandX, Matron::kBandSouthZ, 60.0, false) &&
                       go(s, Matron::kBandX, Matron::kBandSouthZ, 60.0, false) &&
                       go(s, Matron::kBandX, Matron::kBandNorthZ, 60.0, false) && go(s, x, z, 60.0, false, 0.2);
    return round;
}
} // namespace

// Staged across the bay, the dash she hears; true once she is down in it,
// holding the leaf on her palms.
bool stage_hold(Simulation &s) {
    require(stage_across(s, 3.0, Matron::kBandNorthZ), "bay trap: round the deck to east of the bay");
    (void)s.set_crouch_input(false);
    require(go(s, 8.0, Matron::kBandNorthZ, 4.0, true, 0.3) && go(s, 3.0, Matron::kBandNorthZ, 4.0, false, 0.3),
            "bay trap: a dash along the band, heard across the bay");
    for (int i = 0; i < 30 * static_cast<int>(Simulation::kTickRateHz); ++i) {
        tick(s);
        const auto m = s.matron_state();
        if (m.trap == kTrapHolding && m.bracing && s.bay_state().hands) {
            stand(s, 1.0);
            return true;
        }
    }
    return false;
}

// Her centre physically on the leaf: between its hinge and its west end,
// over its width, at deck height. The trip is only taken once this holds.
bool on_bay_leaf(Simulation &s) {
    const auto m = s.matron_state();
    return m.position.x > Bay::kLeafWestX && m.position.x < Bay::kLeafEastX &&
           std::abs(m.position.z - Bay::kLeafZ) < Bay::kLeafHalfWidth &&
           m.position.y > Bay::kDeckTop - 0.5 && m.position.y < Bay::kDeckTop + 2.0;
}

// At the trip handle, take it and step back east at `pull` of full stick
// until the guard opens past its interlock (or `seconds` pass), then let go.
// True once the guard has been seen open. The player stays crouched: he
// took it crouched, so the handle stays where it hung until he steps back.
// Fast: no long settle, so it can catch her on the leaf before she is off it.
// The approach is 1.3, east of the sheave post (0.82-0.88): the player's
// capsule cannot stand at 0.9, and 1.3 is still inside the 1.20 m reach.
bool trip_bay_now(Simulation &s, const double pull, const double seconds) {
    if (!walk_to(s, 1.3, Bay::kTripSheave2Z, 15.0, 0.15)) {
        return false;
    }
    (void)s.set_facing(-1.0, 0.0);
    (void)s.advance_frame(2.0 * Simulation::kFixedStepSeconds);
    const auto facing = s.snapshot();
    if (facing.carry_target_entity_id != Bay::kTripHandleEntity || facing.carry_target_kind != 2) {
        return false;
    }
    (void)s.request_pick_up();
    bool holding = false;
    for (int i = 0; i < 12 && !holding; ++i) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        holding = s.snapshot().carrying_entity_id == Bay::kTripHandleEntity;
    }
    if (!holding) {
        return false;
    }
    bool tripped = false;
    const auto ticks = static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < ticks && !tripped; ++tick) {
        (void)s.set_move_input(0.7 * pull, 0.0);
        (void)s.set_facing(-1.0, 0.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        tripped = s.bay_state().guard_angle < -0.08;
    }
    // One more tick with the guard held open: the district's pre_step runs
    // before the physics step, so it only sees the open guard (and stops the
    // drive) on the tick after the guard opens.
    if (tripped) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
    if (s.snapshot().carrying_entity_id != 0) {
        (void)s.request_set_down();
    }
    return tripped;
}

// Lure her west across the leaf, circle east around the deck, and wait
// crouched at the trip handle. True once the player is at the handle with
// it offered, and she is far from the bay (out of her trap's west zone, so
// a noise from the east draws her across the leaf instead of to its bracket).
bool stage_dump(Simulation &s) {
    require(s.advance_frame(1.0).accepted, "bay dump: settle");
    // Wake her first: a dash along the north band right by her locker (she
    // sleeps; a dash out west would not reach her). Then, crouched and quiet,
    // west over the leaf and down the west band: she follows the movement
    // she can see, and keeps the mouth of the way out there.
    const bool woke = go(s, 10.0, Matron::kBandNorthZ, 4.0, true, 1.0);
    if (!woke) {
        return false;
    }
    (void)s.set_crouch_input(true);
    const bool west = go(s, -Matron::kBandX, Matron::kBandNorthZ, 60.0, false);
    if (!west) {
        return false;
    }
    // Retreat south down the west band, LOUD: a dash to the south band, so
    // she hears it from where she guards and comes further west after it.
    // (Crouched and quiet, she stops at the way out's mouth, ~6 m off the
    // bay, still inside her trap's west zone.)
    (void)s.set_crouch_input(false);
    const bool south = go(s, -Matron::kBandX, Matron::kBandSouthZ, 10.0, true, 0.5);
    if (!south) {
        return false;
    }
    // Wait until she is far WEST of the bay: more than 12.5 m from its west
    // edge, on the west side (out of her trap's west zone). Distance alone
    // would also pass with her still east at her locker.
    bool far = false;
    for (int i = 0; i < 30 * static_cast<int>(Simulation::kTickRateHz) && !far; ++i) {
        tick(s);
        const auto m = s.matron_state();
        far = m.position.x < Bay::kWestX &&
              std::hypot(m.position.x - Bay::kWestX, m.position.z - Bay::kLeafZ) > 12.5;
    }
    if (!far) {
        return false;
    }
    // Circle east around the deck, crouched and quiet: south band east, east
    // band north, to the trip handle. (Crouched: the south dash left the
    // player loud, and a loud circle east draws her south after him.)
    (void)s.set_crouch_input(true);
    const bool se = go(s, -Matron::kBandX, Matron::kBandSouthZ, 60.0, false);
    if (!se) {
        return false;
    }
    const bool ee = go(s, Matron::kBandX, Matron::kBandSouthZ, 60.0, false);
    if (!ee) {
        return false;
    }
    const bool ne = go(s, Matron::kBandX, Matron::kBandNorthZ, 60.0, false);
    if (!ne) {
        return false;
    }
    const bool to_handle = go(s, 1.3, Bay::kTripSheave2Z, 20.0, false, 0.15);
    if (!to_handle) {
        return false;
    }
    // Crouched at the handle, facing it (west), ready to take it.
    (void)s.set_facing(-1.0, 0.0);
    (void)s.advance_frame(0.5);
    return s.snapshot().carry_target_entity_id == Bay::kTripHandleEntity &&
           s.snapshot().carry_target_kind == 2;
}

void run_bay() {
    // ---- the walkway: level under its motor, a climber over and back; the
    //      fan's noise over the steps beside it ---------------------------------
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        require(s.advance_frame(1.0).accepted, "bay walkway: settle");
        (void)s.set_crouch_input(true);
        stand(s, 4.0);
        const auto found = s.bay_state();
        require(found.leaf_open && found.leaf_running && found.fan_running && found.fan_fraction > 0.95 &&
                    !found.hooked && found.trips == 0,
                "bay walkway: as found, the leaf level under its motor and the fan running");
        require(found.cover > 0.999, "bay walkway: away from the fan, nothing of a step is covered");
        double low_angle = 0.0;
        double low_y = 1.0e9;
        double cover = 1.0;
        const auto watch = [&] {
            low_angle = std::min(low_angle, s.bay_state().leaf_angle);
            low_y = std::min(low_y, s.snapshot().player_position.y);
            if (std::abs(s.snapshot().player_position.x + 1.0) < 0.3) {
                cover = std::min(cover, s.bay_state().cover);
            }
        };
        for (const double x : {0.4, -4.6, 0.4}) {
            bool there = false;
            for (int i = 0; i < 20 * static_cast<int>(Simulation::kTickRateHz) && !there; ++i) {
                steer_toward(s, x, Matron::kBandNorthZ);
                tick(s);
                watch();
                there = std::hypot(s.snapshot().player_position.x - x, s.snapshot().player_position.z - Matron::kBandNorthZ) < 0.2;
            }
            require(there, "bay walkway: over the leaf and back");
        }
        std::cout << "  bay walkway: the leaf at most " << low_angle << " rad down under the crossing, the player no lower than "
                  << low_y << " m; the fan leaves " << cover << " of a step on the leaf\n";
        require(low_angle > -0.01 && low_y > Bay::kDeckTop, "bay walkway: crossed, the leaf stays level on its stop");
        require(cover < 0.3, "bay walkway: on the leaf beside the running fan, three quarters of a step is covered");
        require(s.matron_state().state == 0, "bay walkway: crouched over it, she sleeps on");
        std::cout << "PASS scraperx_sim bay walkway: low_angle=" << low_angle << " cover=" << cover << "\n";
    }
    // ---- her trap: she holds the leaf; stepped onto, it goes down -------------
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        require(stage_hold(s), "bay trap: across the bay from her, the player heard: she holds the leaf from below");
        const auto held = s.bay_state();
        const auto her = s.matron_state();
        std::cout << "  bay trap: her hands at (" << held.hand.x << ", " << held.hand.y << ", " << held.hand.z << "), "
                  << held.support_n << " N up; the leaf at " << held.leaf_angle << " rad\n";
        require(her.bracing && her.traps == 1 && held.hands && !held.leaf_running && held.trips == 1 &&
                    held.support_n > 0.0 && std::abs(held.leaf_angle) < 0.02,
                "bay trap: her hands in it stop its motor, and the leaf rests on her palms, level");
        // Its fan, on the same district, coasts down: nothing covers a step now.
        const double cover_running = held.cover;
        bool fan_still = false;
        for (int i = 0; i < 20 * static_cast<int>(Simulation::kTickRateHz) && !fan_still; ++i) {
            tick(s);
            fan_still = s.bay_state().fan_fraction < 1.0e-3;
        }
        const double cover_still = s.bay_state().cover;
        require(fan_still && s.matron_state().bracing, "bay trap: the fan coasts to a stop while she holds the leaf");
        require(cover_running < 0.8 && cover_still > 0.999,
                "bay trap: the fan stopped, the steps beside it are no longer covered");
        // Onto the leaf: past what her hands can hold it goes down, onto her.
        const auto deaths = s.snapshot().death_count;
        double off_hinge = 0.0;
        double low_angle = 0.0;
        bool fell = false;
        for (int i = 0; i < 10 * static_cast<int>(Simulation::kTickRateHz) && !fell; ++i) {
            const auto p = s.snapshot().player_position;
            if (p.y > Bay::kDeckTop) {
                steer_toward(s, -3.0, Matron::kBandNorthZ, 0.5);
            } else {
                (void)s.set_move_input(0.0, 0.0);
            }
            tick(s);
            const auto b = s.bay_state();
            if (b.leaf_angle < -0.02 && off_hinge == 0.0) {
                off_hinge = Bay::kLeafEastX - p.x;
            }
            low_angle = std::min(low_angle, b.leaf_angle);
            fell = s.snapshot().player_grounded && s.snapshot().player_position.y < 150.0;
        }
        std::cout << "  bay trap: the leaf went with the player " << off_hinge << " m out from its hinge, down to "
                  << low_angle << " rad; the player down at " << s.snapshot().player_position.y << " m\n";
        require(off_hinge > 0.8 && off_hinge < 2.5 && low_angle < Bay::kLeafHangAngle + 0.05,
                "bay trap: stepped onto, the leaf goes down past what she can hold");
        require(fell && s.snapshot().death_count == deaths, "bay trap: the player goes down through the bay and lives");
        // She lets go, climbs out after them; the district starts again by
        // itself, and the motor brings the leaf up level.
        bool out = false;
        bool level = false;
        for (int i = 0; i < 30 * static_cast<int>(Simulation::kTickRateHz) && !(out && level); ++i) {
            tick(s);
            const auto m = s.matron_state();
            out = out || (m.trap == 0 && !m.bracing && m.position.y > Bay::kDeckTop);
            level = s.bay_state().leaf_open;
        }
        const auto after = s.bay_state();
        require(out && s.matron_state().alive, "bay trap: she lets go and climbs out by the bracket's rungs");
        require(level && after.restarts == 1 && after.fan_running,
                "bay trap: its hands out and guard shut, the district starts again and the leaf comes up level");
        std::cout << "PASS scraperx_sim bay trap: support_n=" << held.support_n << " off_hinge_m=" << off_hinge
                  << " restarts=" << after.restarts << "\n";
    }
    // ---- her trap beaten: the chain off its cleat onto the leaf's eye --------
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        require(stage_hold(s), "bay chain: she holds the leaf");
        const std::vector<std::uint8_t> holding = s.save_game();
        require(walk_to(s, 0.4, -129.5, 6.0, 0.08), "bay chain: to the cleat on the rail's end post");
        (void)s.set_facing(-1.0, 0.0);
        (void)s.advance_frame(0.4);
        require(s.snapshot().rig_action == 2 && s.snapshot().rig_target_entity_id == Bay::kShackleEntity && s.request_rig(),
                "bay chain: TAKE CHAIN is offered at the cleat, and taken");
        (void)s.advance_frame(0.4);
        require(s.snapshot().carrying_entity_id == Bay::kShackleEntity, "bay chain: the shackle in hand");
        // A step onto the leaf at its hinge (her hands hold that), the eye in reach.
        require(walk_to(s, -0.6, Bay::kEyeZ, 6.0, 0.08), "bay chain: a step onto the leaf by its hinge");
        (void)s.set_facing(-1.0, 0.0);
        (void)s.advance_frame(0.4);
        require(s.snapshot().rig_action == 1 && s.snapshot().rig_target_entity_id == Bay::kLeafEntity && s.request_rig(),
                "bay chain: HOOK ON is offered at the leaf's eye, and done");
        (void)s.advance_frame(0.3);
        require(s.bay_state().hooked, "bay chain: the chain on the leaf's eye");
        // The chain has it: she lets go; the district starts again; the leaf
        // carries the player over the bay's far half, where she could not
        // have held it.
        bool let_go = false;
        bool restarted = false;
        double chain_n = 0.0;
        double held_low = 0.0;
        for (int i = 0; i < 20 * static_cast<int>(Simulation::kTickRateHz) && !(let_go && restarted); ++i) {
            tick(s);
            let_go = let_go || !s.matron_state().bracing;
            const auto b = s.bay_state();
            if (let_go && !b.leaf_running) {
                chain_n = std::max(chain_n, b.chain_n);
                held_low = std::min(held_low, b.leaf_angle);
            }
            restarted = b.restarts > 0 && b.leaf_open;
        }
        std::cout << "  bay chain: her hands out, the chain holds the leaf at " << held_low << " rad with " << chain_n
                  << " N\n";
        require(let_go && chain_n > 1000.0 && held_low > -0.05,
                "bay chain: hooked, she lets go, and the chain holds the leaf up, its motor still stopped");
        require(restarted, "bay chain: her hands out, the district starts again and drives the leaf level");
        // Over its far half, where she could not have held it with the
        // player on it, and back.
        double low_y = 1.0e9;
        for (const double x : {-3.0, 0.6}) {
            bool there = false;
            for (int i = 0; i < 10 * static_cast<int>(Simulation::kTickRateHz) && !there; ++i) {
                steer_toward(s, x, Matron::kBandNorthZ, 0.6);
                tick(s);
                low_y = std::min(low_y, s.snapshot().player_position.y);
                there = std::abs(s.snapshot().player_position.x - x) < 0.2;
            }
            require(there, "bay chain: out over the leaf's far half and back");
        }
        require(low_y > Bay::kDeckTop && s.snapshot().death_count == 0,
                "bay chain: hooked and driven, the leaf carries the player over the bay");
        // A save made while she held it, loaded: the climb as it was when she
        // burst out (the checkpoint is not renewed while she is out), the bay
        // with it: the leaf up under its motor, the chain on its cleat, her
        // hands out of it.
        Simulation t(InitialSpawn::Deck154);
        t.place_matron_home(154.0F);
        require(t.load_game(holding.data(), holding.size()), "bay chain: the save made while she held it loads");
        (void)t.advance_frame(0.5);
        const auto loaded = t.bay_state();
        require(loaded.leaf_open && loaded.fan_running && !loaded.hands && !loaded.hooked && !t.matron_state().bracing &&
                    t.matron_state().trap == 0,
                "bay chain: loaded, the bay as at her release: the leaf driven level, the chain on its cleat");
        std::cout << "PASS scraperx_sim bay chain: chain_n=" << chain_n << " held_rad=" << held_low << "\n";
    }
    // ---- the dump: her bay trap turned back on her --------------------------
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        require(stage_dump(s), "bay dump: her west, the player at the trip handle");
        // A dash west from the handle, loud enough to draw her across, then
        // back to the handle at a sprint to wait, crouched, for her centre
        // on the leaf. The dash goes WEST to -5.0, not east: she guards far
        // west, fooled twice by the staging, so a far noise east only turns
        // her head (kWaryNoiseM 12 m). A noise at -5.0 is inside 12 m of
        // her post, so she hunts east after it, across the leaf.
        (void)s.set_crouch_input(false);
        require(go(s, -5.0, Matron::kBandNorthZ, 4.0, true, 0.3), "bay dump: the lure west");
        require(go(s, 1.3, Bay::kTripSheave2Z, 4.0, true, 0.15), "bay dump: back to the handle");
        (void)s.set_crouch_input(true);
        (void)s.set_facing(-1.0, 0.0);
        bool on_leaf = false;
        for (int i = 0; i < 60 * static_cast<int>(Simulation::kTickRateHz) && !on_leaf; ++i) {
            tick(s);
            on_leaf = on_bay_leaf(s);
        }
        require(on_leaf, "bay dump: she came onto the leaf");
        const auto at_trip = s.matron_state();
        const double damage_before = at_trip.damage_ns;
        std::cout << "  bay dump: she is on the leaf at (" << at_trip.position.x << ", " << at_trip.position.z
                  << "), damage " << damage_before << " N s\\n";
        const bool tripped = trip_bay_now(s, 1.0, 3.0);
        const auto tripped_state = s.bay_state();
        require(tripped, "bay dump: the trip opened the guard");
        require(!tripped_state.leaf_running, "bay dump: the district is out");
        std::cout << "  bay dump: guard " << tripped_state.guard_angle << " rad, leaf " << tripped_state.leaf_angle
                  << " rad, motor out\\n";
        // The leaf falls under her; she drops a full deck to the 143 m deck.
        // A fall's landing knocks her down (kHardLandingMps) rather than
        // adding damage_ns, which only counts blows; so the drop hurting her
        // is read as her going Down on the landing, not as a damage number.
        double low_angle = 0.0;
        double low_y = 1.0e9;
        bool down = false;
        bool was_down = false;
        for (int i = 0; i < 10 * static_cast<int>(Simulation::kTickRateHz) && !down; ++i) {
            tick(s);
            const auto b = s.bay_state();
            low_angle = std::min(low_angle, b.leaf_angle);
            const auto m = s.matron_state();
            low_y = std::min(low_y, m.position.y);
            down = m.position.y < 146.0;
            was_down = was_down || m.state == static_cast<std::uint8_t>(Matron::State::Down);
        }
        // Let her land and settle on the deck below before reading where she
        // is; watch whether the landing puts her Down (the drop loop ends
        // when she is below 146, before she lands, so Down is read here).
        (void)s.set_move_input(0.0, 0.0);
        (void)s.set_sprint_input(false);
        for (int i = 0; i < 2 * static_cast<int>(Simulation::kTickRateHz); ++i) {
            tick(s);
            was_down = was_down || s.matron_state().state == static_cast<std::uint8_t>(Matron::State::Down);
        }
        const auto landed = s.matron_state();
        std::cout << "  bay dump: the leaf down to " << low_angle << " rad; she down at " << landed.position.y
                  << " m, damage " << landed.damage_ns << " N s (was " << damage_before << ")\\n";
        require(down && low_angle < Bay::kLeafHangAngle + 0.05,
                "bay dump: the leaf falls under her and she drops a full deck");
        require(landed.alive, "bay dump: the fall is a dump, not a kill");
        require(was_down, "bay dump: the drop put her down");
        require(landed.position.y < 146.0 && landed.position.y > 140.0,
                "bay dump: she lands on the 143 m deck");
        // The dump puts her in the trap's climb-out state: the leaf that
        // dropped her is her own trap's leaf, so its way out is hers.
        require(landed.trap == 5, "bay dump: the leaf's dump puts her in the trap's climb-out state");
        // She recovers only by a real climb: up to the 154 m deck, no
        // position write. She has no lead up after the drop (the player is
        // quiet on the deck above), so draw her with a loud dash on the
        // 154 m deck first. The draw is at the corner pipe's deck rail: the
        // riser ladder drew her to its top's z without a climb, so she is
        // drawn to the pipe she climbs hand over hand (matron_pipe.cpp).
        (void)s.set_crouch_input(false);
        require(go(s, Matron::kPipeDeckX, Matron::kPipeDeckZ, 4.0, true, 0.3), "bay dump: the draw to the pipe");
        require(go(s, 1.3, Bay::kTripSheave2Z, 4.0, true, 0.15), "bay dump: back to the handle");
        (void)s.set_crouch_input(true);
        bool back = false;
        for (int i = 0; i < 150 * static_cast<int>(Simulation::kTickRateHz) && !back; ++i) {
            tick(s);
            back = s.matron_state().position.y > Bay::kDeckTop;
        }
        require(back, "bay dump: she climbs back to the 154 m deck by a real climb");
        require(s.matron_state().alive, "bay dump: she climbs back alive");
        std::cout << "PASS scraperx_sim bay dump: guard_rad=" << tripped_state.guard_angle
                  << " leaf_rad=" << low_angle << " low_y=" << low_y
                  << " damage_ns=" << landed.damage_ns << " trap=" << static_cast<int>(landed.trap) << "\\n";
    }
    // ---- she refuses an unrelated ladder in an ordinary hunt --------------
    // The gate in follow_climb holds everywhere else: at a ladder's foot in
    // an ordinary hunt (no trap, no Turn) she looks up it and does not take
    // hold of it, even with a lead above her.
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        require(s.advance_frame(1.0).accepted, "bay climb refusal: settle");
        // Put her on the 143 m deck at the C3 ladder's foot. The player
        // dashes on the deck above, loud enough to draw her up, renewed
        // inside the 9 s lose-track window so she keeps hunting.
        s.debug_place_matron({-6.11, 143.03, -134.01});
        double max_y = -1.0e9;
        const int tick_hz = static_cast<int>(Simulation::kTickRateHz);
        for (int i = 0; i < 40 * tick_hz; ++i) {
            if (i % (5 * tick_hz) == 0) {
                const double tx = ((i / (5 * tick_hz)) % 2 == 0) ? 10.0 : -10.0;
                (void)go(s, tx, Matron::kBandNorthZ, 2.0, true, 0.5);
            }
            tick(s);
            max_y = std::max(max_y, s.matron_state().position.y);
        }
        const auto her = s.matron_state();
        const double foot_d = std::hypot(her.position.x - (-6.0), her.position.z - (-134.10));
        std::cout << "  bay climb refusal: max_y=" << max_y << " m, at foot d=" << foot_d
                  << " m, climbs=" << her.climbs << ", trap=" << static_cast<int>(her.trap)
                  << ", stage=" << static_cast<int>(her.stage) << "\\n";
        require(max_y < 146.0, "bay climb refusal: she never climbs the ladder");
        require(her.climbs == 0, "bay climb refusal: she takes no climb");
        require(foot_d < 1.5, "bay climb refusal: she stands at the ladder's foot");
        require(her.alive, "bay climb refusal: she lives");
        std::cout << "PASS scraperx_sim bay climb refusal: max_y=" << max_y << "\\n";
    }
    // ---- tripped early, it does nothing --------------------------------------
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        require(s.advance_frame(1.0).accepted, "bay trip early: settle");
        // Trip the handle before she has committed to the leaf.
        (void)s.set_crouch_input(true);
        const bool tripped = trip_bay_now(s, 1.0, 3.0);
        const auto after_trip = s.bay_state();
        require(tripped, "bay trip early: the trip opened the guard");
        require(!after_trip.leaf_running, "bay trip early: the district is out");
        std::cout << "  bay trip early: guard " << after_trip.guard_angle << " rad, leaf "
                  << after_trip.leaf_angle << " rad\\n";
        // The leaf falls with nobody on it. Lure her, and watch: she is not
        // dropped, because she never committed.
        (void)s.set_crouch_input(false);
        require(go(s, 3.0, Matron::kBandNorthZ, 4.0, true, 0.3), "bay trip early: the lure east");
        double low_y = 1.0e9;
        for (int i = 0; i < 20 * static_cast<int>(Simulation::kTickRateHz); ++i) {
            tick(s);
            low_y = std::min(low_y, s.matron_state().position.y);
        }
        const auto her = s.matron_state();
        std::cout << "  bay trip early: she no lower than " << low_y << " m, alive=" << her.alive << "\\n";
        require(her.alive, "bay trip early: she lives");
        require(low_y > 150.0, "bay trip early: tripped early, she is not dropped");
        require(s.snapshot().death_count == 0, "bay trip early: nobody died");
        std::cout << "PASS scraperx_sim bay trip early: low_y=" << low_y << "\\n";
    }
}
SCRAPERX_GROUP("bay", 970, run_bay);
SCRAPERX_SELECT("bay", run_bay);
