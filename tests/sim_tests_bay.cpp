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

// The stock piece lying near (x, z) on the 154 m deck: its entity, or 0.
std::uint64_t stock_near(Simulation &s, const double x, const double z) {
    for (std::uint32_t b = 0; b < s.kit_body_count(); ++b) {
        const auto p = s.kit_body_position(b);
        if (std::abs(p.x - x) < 0.6 && std::abs(p.y - Bay::kDeckTop) < 0.6 && std::abs(p.z - z) < 0.6) {
            return s.kit_body_entity(b);
        }
    }
    return 0;
}

// Pick up the stock piece `entity` and carry it to (x, z), then set it down.
// True with the piece set down there.
bool pick_up(Simulation &s, const std::uint64_t entity) {
    const std::uint32_t body = s.kit_body_index(entity);
    for (int i = 0; i < static_cast<int>(30.0 * Simulation::kTickRateHz) &&
                    s.snapshot().carry_target_entity_id != entity;
         ++i) {
        const auto b = s.kit_body_position(body);
        const auto p = s.snapshot().player_position;
        const double dx = b.x - p.x, dz = b.z - p.z;
        const double d = std::max(std::hypot(dx, dz), 1.0e-3);
        (void)s.set_facing(dx / d, dz / d);
        (void)s.set_look_pitch(-0.5);
        (void)s.set_move_input(d > 0.7 ? 0.3 * dx / d : 0.0, d > 0.7 ? 0.3 * dz / d : 0.0);
        tick(s);
    }
    require(s.snapshot().carry_target_entity_id == entity, "bay door: the beam is offered");
    (void)s.request_pick_up();
    (void)s.advance_frame(0.4);
    require(s.snapshot().carrying_entity_id == entity, "bay door: the beam is picked up");
    return true;
}

// Carry the carried piece `entity` to (x, z), then set it down. True with
// the piece set down there.
bool carry_carried(Simulation &s, const std::uint64_t entity, const double x, const double z) {
    require(s.snapshot().carrying_entity_id == entity, "bay door: the beam is in hand");
    require(go(s, x, z, 90.0, false, 0.3), "bay door: the beam is carried to the door");
    (void)s.request_set_down();
    (void)s.advance_frame(1.0);
    require(s.snapshot().carrying_entity_id == 0, "bay door: the beam is set down");
    return true;
}

bool carry_to(Simulation &s, const std::uint64_t entity, const double x, const double z) {
    return pick_up(s, entity) && carry_carried(s, entity, x, z);
}

// From the deck, down onto the bracket east of the blast door (1.78 m down):
// walk to the staging point west of the cut's edge, then step east off the
// edge at a slow walk so the fall lands on the bracket (x -3.9..-2.9) instead
// of carrying past it into the cut. The gap in the west waist rail at the
// rungs' head is the way through (z -130.6..-129.75); the drop is at its
// centre, the Matron's own drop point. True grounded on the bracket.
bool drop_to_bracket(Simulation &s) {
    (void)s.set_crouch_input(false);
    // South of the deck-14 shaft rail (z -133), come back around it the way
    // he came: west along the south band, north along the west band, east
    // along the north band. North of it, straight to the drop point.
    if (s.snapshot().player_position.z < -133.0) {
        // With the beam in hand he carries at 0.25 m/s (a 70 kg load
        // against the 720 N carry grip: top speed scaled by
        // 1 - weight/grip, 5.5 * 0.046 = 0.25 m/s, measured), so the
        // legs back around the rail budget distance / 0.25 with margin,
        // not the 60 s an unloaded walk takes. The positions required
        // are unchanged; only the time budget fits the measured carry.
        require(go(s, -Matron::kBandX, Matron::kBandSouthZ, 140.0, false, 0.3),
                "bay door: west along the south band with the beam");
        // DIAG (temporary): where does the north carry-back stall?
        if (s.snapshot().carrying_entity_id != 0) {
            const std::uint64_t carried = s.snapshot().carrying_entity_id;
            const std::uint32_t cb = s.kit_body_index(carried);
            for (int i = 0; i < 250 * static_cast<int>(Simulation::kTickRateHz); ++i) {
                const auto p = s.snapshot().player_position;
                if (i % 90 == 0) {
                    const auto b = s.kit_body_position(cb);
                    const auto q = s.kit_body_rotation(cb);
                    // Beam long axis (local +x) rotated by q: yaw = atan2(-axis.z, axis.x).
                    const double ax = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
                    const double az = 2.0 * (q.x * q.z - q.w * q.y);
                    const auto m = s.matron_state().position;
                    std::cout << "  DIAG carry-north t=" << (i / 90) << "s player (" << p.x << ", " << p.y << ", " << p.z
                              << ") beam (" << b.x << ", " << b.y << ", " << b.z << ") yaw=" << std::atan2(-az, ax)
                              << " matron (" << m.x << ", " << m.y << ", " << m.z << ") carrying=" << carried << "\n";
                    for (std::uint32_t k = 0; k < s.kit_body_count(); ++k) {
                        const auto kp = s.kit_body_position(k);
                        if (std::hypot(kp.x - p.x, kp.z - p.z) < 3.0 && s.kit_body_entity(k) != carried) {
                            std::cout << "    near stock entity " << s.kit_body_entity(k) << " at (" << kp.x << ", "
                                      << kp.y << ", " << kp.z << ")\n";
                        }
                    }
                }
                const double dx = -Matron::kBandX - p.x, dz = Matron::kBandNorthZ - p.z;
                const double len = std::hypot(dx, dz);
                if (len < 0.3) {
                    (void)s.set_move_input(0.0, 0.0);
                    std::cout << "  DIAG carry-north reached in " << (i / 90) << "s\n";
                    break;
                }
                (void)s.set_move_input(dx / len, dz / len);
                (void)s.set_facing(dx / len, dz / len);
                tick(s);
            }
            const auto p = s.snapshot().player_position;
            require(std::hypot(p.x + Matron::kBandX, p.z - Matron::kBandNorthZ) < 0.3,
                    "bay door: north along the west band with the beam");
        } else {
            require(go(s, -Matron::kBandX, Matron::kBandNorthZ, 250.0, false, 0.3),
                    "bay door: north along the west band with the beam");
        }
        require(go(s, -6.0, Matron::kBandNorthZ, 95.0, false, 0.3),
                "bay door: east along the north band with the beam");
    }
    require(go(s, -6.0, Bay::kDropZ, 20.0, false, 0.3), "bay door: to the drop point over the bracket");
    (void)s.set_facing(1.0, 0.0);
    for (int i = 0; i < 15 * static_cast<int>(Simulation::kTickRateHz); ++i) {
        const auto p = s.snapshot().player_position;
        // The pocket floor is the same height as the bracket (152.22), so the
        // height alone cannot tell them apart: the bracket is x -3.9..-2.9.
        if (s.snapshot().player_grounded && p.y < 153.5 && p.y > 152.5 && p.x > -3.8 && p.x < -3.0) {
            (void)s.set_move_input(0.0, 0.0);
            return true;
        }
        if (p.y < 150.0) {
            (void)s.set_move_input(0.0, 0.0);
            return false;
        }
        (void)s.set_move_input(0.15, 0.0);
        (void)s.set_facing(1.0, 0.0);
        tick(s);
    }
    (void)s.set_move_input(0.0, 0.0);
    return false;
}

// Shove the door with the body: walk into it along x (`dir` -1 west, +1
// east) until its angle passes `angle` (negative opens west, 0 is shut), or
// `seconds` run out. True with the door past the angle.
bool shove_door(Simulation &s, const double dir, const double angle, const double seconds) {
    const auto ticks = static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (int i = 0; i < ticks; ++i) {
        const double a = s.bay_state().door_angle;
        if ((dir < 0 && a < angle) || (dir > 0 && a > angle)) {
            (void)s.set_move_input(0.0, 0.0);
            return true;
        }
        (void)s.set_move_input(dir, 0.0);
        (void)s.set_facing(dir, 0.0);
        tick(s);
    }
    (void)s.set_move_input(0.0, 0.0);
    return false;
}

// Stage the wedge: round the shaft rail on the bands to the beam on the
// south band, the beam picked up and carried back around the rail to the
// drop, set down on the bracket while the door is shoved open from the
// bracket (the door spans the bracket floor to the deck underside, so from
// the deck he cannot touch it at all), the beam picked up again and carried
// through the open doorway into the pocket and dropped at (x, z), the door
// hauled shut from the pocket. Returns the beam's entity.
std::uint64_t stage_door_shut(Simulation &s, const double x, const double z) {
    s.place_matron_home(154.0F);
    require(s.advance_frame(1.0).accepted, "bay door wedge: settle");
    const std::uint64_t beam = stock_near(s, 2.0, -174.4);
    require(beam != 0, "bay door wedge: the timber beam is on the south band");
    // Round the deck-14 shaft rail to the beam: the rail runs east-west at
    // z -133 and is crossed only around its ends, on the bands (a straight
    // walk from spawn sticks against it at (-9.2, -132.4)). West along the
    // north band, south along the west band, east along the south band.
    require(go(s, -Matron::kBandX, Matron::kBandNorthZ, 60.0, false, 0.3),
            "bay door wedge: west along the north band to the beam");
    require(go(s, -Matron::kBandX, Matron::kBandSouthZ, 60.0, false, 0.3),
            "bay door wedge: south along the west band to the beam");
    require(go(s, 2.0, Matron::kBandSouthZ, 60.0, false, 0.3),
            "bay door wedge: east along the south band to the beam");
    // Uncrouched to the beam: his steps do not wake her (she hears impacts,
    // running machines and the room-noise cue, not footsteps), and crouched
    // the 48 m walk does not fit the pick-up loop's 30 s.
    require(pick_up(s, beam), "bay door wedge: the beam is picked up");
    require(drop_to_bracket(s), "bay door wedge: down onto the bracket with the beam");
    require(s.snapshot().carrying_entity_id == beam, "bay door wedge: the beam came down with him");
    (void)s.request_set_down();
    (void)s.advance_frame(1.0);
    require(s.snapshot().carrying_entity_id == 0, "bay door wedge: the beam is set down on the bracket");
    require(shove_door(s, -1.0, -1.0, 10.0), "bay door wedge: the door is shoved open");
    require(pick_up(s, beam), "bay door wedge: the beam is picked up again");
    require(carry_carried(s, beam, x, z), "bay door wedge: the beam is carried into the pocket");
    // The beam is under its swing, so the door stops on the beam, mostly shut
    // (past -0.45 rad); the wedge tests measure from there.
    require(shove_door(s, 1.0, -0.45, 10.0), "bay door wedge: the door is hauled shut onto the beam");
    return beam;
}

// Wake her with a room-noise cue (she is asleep until then).
void wake_her(Simulation &s) {
    require(s.set_room_loudness(1.0), "bay door wedge: the room-noise cue");
    (void)s.advance_frame(1.0);
    require(s.set_room_loudness(0.0), "bay door wedge: quiet after the cue");
}

// The door's widest opening (most negative angle) over `seconds` of her push.
double push_open(Simulation &s, const double seconds) {
    double max_open = 0.0;
    for (int i = 0; i < static_cast<int>(seconds * Simulation::kTickRateHz); ++i) {
        tick(s);
        max_open = std::min(max_open, s.bay_state().door_angle);
    }
    return max_open;
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
    // ---- J-055 geometry: the doorway and the low opening in the west wall,
    //      and the pocket floor behind it --------------------------------------
    {
        Simulation s(InitialSpawn::Deck154);
        const std::uint32_t frame = s.kit_body_index(Bay::kFrameEntity);
        const auto at = s.kit_body_position(frame);
        // The frame body sits at the origin with identity rotation, so a
        // part's offset is its world centre; the collision shapes are the
        // parts themselves, so no part in a volume means the volume is open.
        require(std::abs(at.x) < 1e-6 && std::abs(at.y) < 1e-6 && std::abs(at.z) < 1e-6,
                "bay door geometry: the frame body sits at the origin");
        const auto overlaps = [&](const double x0, const double y0, const double z0, const double x1,
                                  const double y1, const double z1) {
            for (std::uint32_t p = 0; p < s.kit_body_part_count(frame); ++p) {
                const auto part = s.kit_body_part(frame, p);
                if (part.shape != 0) {
                    continue;
                }
                const double px0 = part.offset.x - part.half.x;
                const double px1 = part.offset.x + part.half.x;
                const double py0 = part.offset.y - part.half.y;
                const double py1 = part.offset.y + part.half.y;
                const double pz0 = part.offset.z - part.half.z;
                const double pz1 = part.offset.z + part.half.z;
                if (px0 < x1 && x0 < px1 && py0 < y1 && y0 < py1 && pz0 < z1 && z0 < pz1) {
                    return true;
                }
            }
            return false;
        };
        // The doorway and the opening, shrunk 1 cm a side so the piers that
        // frame them (touching the boundary exactly) do not count.
        const double inset = 0.01;
        require(!overlaps(Bay::kWestX - 0.05 + inset, Bay::kBracketTop + inset, Bay::kDoorZ0 + inset,
                          Bay::kWestX - inset, Bay::kDeckTop - 0.02 - inset, Bay::kDoorZ1 - inset),
                "bay door geometry: no frame part reaches into the doorway");
        require(!overlaps(Bay::kWestX - 0.05 + inset, Bay::kBracketTop + inset, Bay::kOpeningZ0 + inset,
                          Bay::kWestX - inset, Bay::kOpeningTop - inset, Bay::kOpeningZ1 - inset),
                "bay door geometry: no frame part reaches into the low opening");
        // The pocket floor and the ledge: a solid part under a point just
        // below their top surface.
        const auto solid_at = [&](const double x, const double y, const double z) {
            for (std::uint32_t p = 0; p < s.kit_body_part_count(frame); ++p) {
                const auto part = s.kit_body_part(frame, p);
                if (part.shape != 0) {
                    continue;
                }
                if (part.offset.x - part.half.x <= x && x <= part.offset.x + part.half.x &&
                    part.offset.y - part.half.y <= y && y <= part.offset.y + part.half.y &&
                    part.offset.z - part.half.z <= z && z <= part.offset.z + part.half.z) {
                    return true;
                }
            }
            return false;
        };
        require(solid_at(-4.9, Bay::kBracketTop - 0.01, -129.2),
                "bay door geometry: the pocket floor is solid behind the wall");
        require(solid_at(-3.45, Bay::kBracketTop - 0.01, -127.9),
                "bay door geometry: the ledge is solid at the low opening");
        std::cout << "PASS scraperx_sim bay door geometry: doorway " << (Bay::kDoorZ1 - Bay::kDoorZ0)
                  << " m, opening " << (Bay::kOpeningZ1 - Bay::kOpeningZ0) << " m, pocket floor solid\n";
    }
    // ---- J-055: the wedge holds the door against her push ------------------
    // The beam across the swing near the hinge: she leans and it stays shut.
    {
        Simulation s(InitialSpawn::Deck154);
        const std::uint64_t beam = stage_door_shut(s, -4.3, -130.9);
        wake_her(s);
        const double max_open = push_open(s, 20.0);
        const auto beam_at = s.kit_body_position(s.kit_body_index(beam));
        const auto her = s.matron_state().position;
        std::cout << "  bay door wedge jam: the door opened to " << max_open << " rad under her push; the beam at ("
                  << beam_at.x << ", " << beam_at.z << "); she at (" << her.x << ", " << her.z << ")\n";
        require(max_open > -0.5, "bay door wedge jam: wedged, the door stays nearly shut under her push");
        require(std::abs(her.x + 3.9) < 2.0 && std::abs(her.z + 130.6) < 2.0,
                "bay door wedge jam: she is at the doorway pushing, not elsewhere");
        std::cout << "PASS scraperx_sim bay door wedge jam: max_open_rad=" << max_open << "\n";
    }
    // ---- J-055: the wedge slips -------------------------------------------
    // The same beam, clear of the jambs: she leans, the beam is pushed west
    // across the floor, and the door comes open.
    {
        Simulation s(InitialSpawn::Deck154);
        (void)stage_door_shut(s, -4.6, -130.6);
        wake_her(s);
        const double max_open = push_open(s, 20.0);
        const auto her = s.matron_state().position;
        std::cout << "  bay door wedge slip: the door opened to " << max_open << " rad under her push; she at ("
                  << her.x << ", " << her.z << ")\n";
        require(max_open < -1.0, "bay door wedge slip: slipped, the door comes open under her push");
        require(std::abs(her.x + 3.9) < 2.0 && std::abs(her.z + 130.6) < 2.0,
                "bay door wedge slip: she is at the doorway pushing, not elsewhere");
        std::cout << "PASS scraperx_sim bay door wedge slip: max_open_rad=" << max_open << "\n";
    }
    // ---- J-055: the beam out while she pushes ------------------------------
    // Wedged; then, while she pushes, he carries the beam clear, and the
    // door follows the net torque open.
    {
        Simulation s(InitialSpawn::Deck154);
        const std::uint64_t beam = stage_door_shut(s, -4.3, -130.9);
        wake_her(s);
        const double held = push_open(s, 5.0);
        require(held > -0.5, "bay door beam out: wedged, the door holds while she pushes");
        require(carry_to(s, beam, -5.5, -128.0), "bay door beam out: the beam is pulled clear");
        const double max_open = push_open(s, 10.0);
        std::cout << "  bay door beam out: held at " << held << " rad, then open to " << max_open << " rad\n";
        require(max_open < -1.0, "bay door beam out: the beam out, the door follows her push open");
        std::cout << "PASS scraperx_sim bay door beam out: held_rad=" << held << " open_rad=" << max_open << "\n";
    }
    // ---- J-055: the door and the beam persist through save and load --------
    {
        Simulation s(InitialSpawn::Deck154);
        const std::uint64_t beam = stage_door_shut(s, -4.3, -130.9);
        const double angle_before = s.bay_state().door_angle;
        const auto beam_before = s.kit_body_position(s.kit_body_index(beam));
        const std::vector<std::uint8_t> saved = s.save_game();
        Simulation t(InitialSpawn::Deck154);
        t.place_matron_home(154.0F);
        require(t.load_game(saved.data(), saved.size()), "bay door reload: the save loads");
        (void)t.advance_frame(0.5);
        const double angle_after = t.bay_state().door_angle;
        const auto beam_after = t.kit_body_position(t.kit_body_index(beam));
        std::cout << "  bay door reload: angle " << angle_before << " -> " << angle_after << " rad; the beam ("
                  << beam_before.x << ", " << beam_before.z << ") -> (" << beam_after.x << ", " << beam_after.z << ")\n";
        require(std::abs(angle_after - angle_before) < 0.05, "bay door reload: the door angle persists");
        require(std::hypot(beam_after.x - beam_before.x, beam_after.z - beam_before.z) < 0.1,
                "bay door reload: the beam position persists");
        std::cout << "PASS scraperx_sim bay door reload: angle=" << angle_after << "\n";
    }
    // ---- J-055: he hauls the door shut by its far edge ---------------------
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        require(s.advance_frame(1.0).accepted, "bay door haul: settle");
        require(drop_to_bracket(s), "bay door haul: down onto the bracket");
        require(shove_door(s, -1.0, -1.0, 10.0), "bay door haul: the door is shoved open");
        const double open_angle = s.bay_state().door_angle;
        require(open_angle < -0.8, "bay door haul: the door stands open");
        // Step to the door's far edge and take it.
        require(go(s, -3.6, -130.4, 10.0, false, 0.3), "bay door haul: to the door's far edge");
        (void)s.set_facing(-1.0, 0.0);
        (void)s.request_traversal();
        (void)s.advance_frame(0.4);
        require(s.bay_state().door_grabbed, "bay door haul: the grab button takes the door");
        // Walk it shut (east), then let go.
        bool shut = false;
        for (int i = 0; i < 10 * static_cast<int>(Simulation::kTickRateHz) && !shut; ++i) {
            (void)s.set_move_input(1.0, 0.0);
            (void)s.set_facing(1.0, 0.0);
            tick(s);
            shut = s.bay_state().door_angle > -0.05;
        }
        (void)s.set_move_input(0.0, 0.0);
        const double shut_angle = s.bay_state().door_angle;
        std::cout << "  bay door haul: open " << open_angle << " rad, hauled to " << shut_angle << " rad\n";
        require(shut, "bay door haul: hauled, the door comes shut");
        std::cout << "PASS scraperx_sim bay door haul: shut_angle=" << shut_angle << "\n";
    }
}
SCRAPERX_GROUP("bay", 970, run_bay);
SCRAPERX_SELECT("bay", run_bay);
