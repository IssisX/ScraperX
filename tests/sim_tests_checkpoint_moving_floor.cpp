// J-117 (#439): a checkpoint on a moving floor restores him on real support,
// never in the air or inside steel, never in a death loop.
//
// The exam, as the issue orders it: stand him on each moving floor, commit a
// checkpoint there, kill him a few seconds later, restore, and record what he
// comes back on. One table row per floor, printed as FLOOR lines, then the
// slice requires every measured floor to come back on real support.
//
// How a floor is judged, per the issue's result wanted (3):
//   SUPPORT     grounded, on the floor he committed on (displacement is the
//               floor moving under him after the restore: fine, he rides it)
//   OTHER       grounded, on a different real floor (the floor left without
//               him, or rolled away; he is alive on real support, noted)
//   AIR         not grounded after the settle: the restore put him in the air
//   DEATH-LOOP  he died again in the settle: the restore kills him again
//   NO-LETHAL-EXIT  no death can be reached from the floor at all, so the
//               stale-restore failure cannot happen there (documented safe)
//   SETUP-FAILED / KILL-FAILED  the exam could not reach the floor / kill
//               him there: an exam gap, not a floor verdict, and the slice
//               fails until it is resolved or documented.
//
// Why a moving floor can go stale (the mechanism the table decides between):
// commit_checkpoint stores the player's position and captures the kit (all
// dynamic kit bodies back to their checkpoint poses on restore) plus each
// machine's control state (Route::state, Bay::State, the forklift's lift
// target, the crane's hoist max length, the well and wet machines' water and
// air levels). A floor whose pose is a kit body, or whose drive is machine
// state, comes back with him. A floor whose pose is set by a drive the
// restore takes as it stands, not as it was committed (the crane's hoist
// rope length), does not. The table says which; the exam's first run
// corrected the prior guess that water and air levels are not returned: they
// are machine state, and C, D, E and F all came back SUPPORT.
//
// The kill is a drop onto the floor from 5 m up at -30 m/s: a lethal landing
// (kLethalImpactSpeedMps is 20) that works in a shaft as well as in open air,
// where a 60 m fall would hit the machine above. If it does not kill him,
// the 60 m fall is tried. The floor must keep moving after he leaves: Route
// and well machines are sent, not held; the forklift truck is left coasting;
// the crane's hoist is held on by the remote; the drum and the toolbox are
// left to roll and slide. A floor that stops the moment he leaves (the
// pitman's brake, the forklift's forks) is recorded as such: it cannot go
// stale, because it does not move after the commit.

#include "sim/bay.hpp"
#include "sim/forklift.hpp"
#include "sim/loose_stock.hpp"
#include "sim/mechanism_kit.hpp"
#include "sim/yard_crane.hpp"
#include "sim_tests.hpp"
#include "sim_registry.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;
using scraperx::sim::Vector3;

void tick(Simulation &s) { (void)s.advance_frame(Simulation::kFixedStepSeconds); }

Vector3 kit_at(const Simulation &s, const std::uint64_t entity) {
    return s.kit_body_position(s.kit_body_index(entity));
}

// One row of the table the issue asks for.
struct FloorRow {
    std::string name;
    std::string verdict;
    double gap_m = 0.0;   // feet above the support below him (AIR), else 0
    double disp_m = 0.0;  // |restored pos - commit pos|
    std::uint64_t support = 0;
    std::uint64_t deaths = 0;
    std::string note;
};

std::vector<FloorRow> g_rows;

void record(FloorRow row) {
    std::cout << "  FLOOR " << row.name << ' ' << row.verdict << " gap=" << row.gap_m << "m disp=" << row.disp_m
              << "m support=" << row.support << " deaths=" << row.deaths;
    if (!row.note.empty()) {
        std::cout << " (" << row.note << ')';
    }
    std::cout << '\n';
    g_rows.push_back(std::move(row));
}

// A per-tick input held during the kill flight only, never the settle: the
// floor's drive must keep running after he leaves (the truck under power, the
// crane's hoist on), so the floor moves while he is dead. After the restore
// the machine's own restore has zeroed its drives, so the settle runs clean.
using TickFn = void (*)(Simulation &);

void truck_drive_on(Simulation &s) { (void)s.set_move_input(0.0, 0.25); }
void crane_hoist_on(Simulation &s) { (void)s.set_yard_crane_hoist(1.0); }

// Wait for a fresh commit on the floor: the commit renews every tick while
// he is grounded on firm footing, so one tick on the floor is enough; wait
// up to 5 s in case he is still settling onto it. Returns false when no
// fresh commit arrives.
bool wait_for_commit(Simulation &s, const std::uint64_t floor_entity, Vector3 *commit_pos) {
    const std::uint64_t commits0 = s.snapshot().checkpoint_commit_count;
    bool committed = false;
    for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) && !committed; ++i) {
        tick(s);
        const auto st = s.snapshot();
        committed = st.checkpoint_commit_count > commits0 && st.player_grounded &&
                    st.support_entity_id == floor_entity &&
                    std::hypot(st.player_position.x - st.checkpoint_position.x,
                               st.player_position.z - st.checkpoint_position.z) < 0.1 &&
                    std::abs(st.player_position.y - st.checkpoint_position.y) < 0.1;
        if (committed) {
            *commit_pos = st.checkpoint_position;
        }
    }
    return committed;
}

// Kill him on the floor and record what the restore brings him back on.
// commit_pos is the position the checkpoint holds (from wait_for_commit);
// commit_support is the floor he committed on.
bool kill_and_verdict(Simulation &s, const char *name, const std::uint64_t commit_support, const Vector3 commit_pos,
                      const char *note, TickFn tick_fn) {
    const std::uint64_t deaths0 = s.snapshot().death_count;
    // Kill him: a lethal landing on the floor itself, then the long fall.
    bool died = false;
    s.debug_launch_player({commit_pos.x, commit_pos.y + 5.0, commit_pos.z}, {0.0, -30.0, 0.0});
    for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) && !died; ++i) {
        if (tick_fn != nullptr) {
            tick_fn(s);
        }
        tick(s);
        died = s.snapshot().death_count > deaths0;
    }
    if (!died) {
        s.debug_launch_player({commit_pos.x, commit_pos.y + 60.0, commit_pos.z}, {0.0, 0.0, 0.0});
        for (int i = 0; i < 12 * static_cast<int>(Simulation::kTickRateHz) && !died; ++i) {
            if (tick_fn != nullptr) {
                tick_fn(s);
            }
            tick(s);
            died = s.snapshot().death_count > deaths0;
        }
    }
    if (!died) {
        record({name, "KILL-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "neither kill died"});
        return false;
    }
    // Settle 2 s: the restore fired on the death, and the floor (restored to
    // its checkpoint pose, its drive restored) resumes under him.
    for (int i = 0; i < 2 * static_cast<int>(Simulation::kTickRateHz); ++i) {
        tick(s);
    }
    const auto st = s.snapshot();
    const double disp = std::hypot(st.player_position.x - commit_pos.x, st.player_position.y - commit_pos.y,
                                   st.player_position.z - commit_pos.z);
    if (st.death_count > deaths0 + 1) {
        record({name, "DEATH-LOOP", 0.0, disp, st.support_entity_id, st.death_count, note});
        return true;
    }
    if (!st.player_grounded) {
        const double below = s.debug_support_y(st.player_position, 10.0);
        const double gap = st.player_position.y - 0.9 - below;
        record({name, "AIR", gap, disp, st.support_entity_id, st.death_count, note});
        return true;
    }
    if (st.support_entity_id == commit_support) {
        record({name, "SUPPORT", 0.0, disp, st.support_entity_id, st.death_count, note});
        return true;
    }
    record({name, "OTHER", 0.0, disp, st.support_entity_id, st.death_count, note});
    return true;
}

// PRE: he stands on the moving floor, grounded, support == floor_entity, the
// floor moving (or, for the recorded exceptions, holding still by design).
// Records the row and returns true when a verdict was measured.
bool examine_floor(Simulation &s, const char *name, const std::uint64_t floor_entity, const char *note,
                   TickFn tick_fn = nullptr) {
    Vector3 commit_pos{};
    if (!wait_for_commit(s, floor_entity, &commit_pos)) {
        record({name, "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "no fresh commit on the floor"});
        return false;
    }
    return kill_and_verdict(s, name, floor_entity, commit_pos, note, tick_fn);
}

// After a failed restore in the shared ascent sim: put him back on the car's
// live position and wait until he is grounded on it again.
bool recover_on_car(Simulation &s, const std::uint64_t car_entity) {
    const auto at = kit_at(s, car_entity);
    s.debug_launch_player({at.x, at.y + 1.0, at.z}, {0.0, 0.0, 0.0});
    for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz); ++i) {
        tick(s);
        const auto st = s.snapshot();
        if (st.player_grounded && st.support_entity_id == car_entity) {
            return true;
        }
    }
    return false;
}

// The support he stands on right now, once he is grounded on a moving floor.
std::uint64_t standing_on(const Simulation &s) { return s.snapshot().support_entity_id; }

}  // namespace

namespace {

// ---- The owner's vertical machines: Route 0..7 --------------------------------
using scraperx::sim::InitialSpawn;
// Each boards the machine from the proven ride pattern, sends it, and examines
// it mid-ride, while it is still moving. The deck entity is read off the
// support he stands on, never assumed from a constant.

bool ride_machine(Simulation &s, const std::size_t index, const double travel) {
    (void)s.advance_frame(0.5);
    if (s.lift_state().role != 1 || s.lift_state().machine != static_cast<int>(index)) {
        return false;
    }
    (void)s.request_lift_action();
    return wait_for(s, 120.0, [&s, index, travel](const Snapshot &) {
        return s.lift_state().travels[index] > travel;
    });
}

// 0, the cascade mast: the mast test already proves this one (its restore
// block), so this is the exam's control: it must come back SUPPORT.
void exam_mast() {
    Simulation s(InitialSpawn::Ring264West);
    (void)s.advance_frame(1.0);
    if (!(walk_to(s, -19.5, -150.0, 12.0) && walk_to(s, -16.0, -150.0, 6.0) && s.advance_frame(0.5).accepted &&
          s.lift_state().machine == 0 && s.lift_state().role == 2 && walk_to(s, -10.65, -150.0, 6.0) &&
          s.advance_frame(0.5).accepted && s.lift_state().role == 1)) {
        record({"cascade mast", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not board the deck"});
        return;
    }
    (void)s.request_lift_action();
    if (!wait_for(s, 120.0, [&s](const Snapshot &) { return s.lift_state().travels[0] > 0.1; })) {
        record({"cascade mast", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "the deck never rose"});
        return;
    }
    (void)examine_floor(s, "cascade mast", standing_on(s), "sent, still rising");
}

// 1, the pitman lift: hauled by hand, and its brake holds it the moment the
// hands let go. It cannot move after he leaves, so it cannot go stale; the
// exam records that, it does not pretend otherwise.
void exam_pitman() {
    Simulation s(InitialSpawn::Ring264West);
    (void)s.advance_frame(1.0);
    if (!(walk_to(s, -19.5, -150.0, 12.0) && walk_to(s, -16.0, -150.0, 6.0) && s.advance_frame(0.5).accepted &&
          s.lift_state().role == 2 && walk_to(s, -10.65, -150.0, 6.0) && s.advance_frame(0.5).accepted &&
          s.lift_state().role == 1)) {
        record({"pitman lift", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not board the mast"});
        return;
    }
    (void)s.request_lift_action();
    if (!wait_for(s, 90.0, [&s](const Snapshot &state) {
            return std::abs(s.lift_state().travels[0] - 1.0) < 0.01 && !s.lift_state().moving && state.player_grounded;
        })) {
        record({"pitman lift", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "the mast never arrived"});
        return;
    }
    if (!(walk_to(s, -19.5, -149.25, 6.0) && walk_to(s, -23.5, -149.0, 6.0) && walk_to(s, -23.5, -154.3, 6.0) &&
          s.advance_frame(0.5).accepted && s.lift_state().role == 1)) {
        record({"pitman lift", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not board its deck"});
        return;
    }
    (void)s.set_lift_haul(true);
    if (!wait_for(s, 90.0, [&s](const Snapshot &) { return s.lift_state().travels[1] > 0.1; })) {
        record({"pitman lift", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "hauling never lifted the deck"});
        return;
    }
    (void)examine_floor(s, "pitman lift", standing_on(s),
                        "hauled by hand; the brake holds it once the hands let go, so it does not move after he leaves");
}

// 2, the barrel helix: up the mast and the pitman first, then its own deck.
void exam_helix() {
    Simulation s(InitialSpawn::Ring264West);
    (void)s.advance_frame(1.0);
    if (!(walk_to(s, -19.5, -150.0, 12.0) && walk_to(s, -16.0, -150.0, 6.0) && s.advance_frame(0.5).accepted &&
          s.lift_state().role == 2 && walk_to(s, -10.65, -150.0, 6.0) && s.advance_frame(0.5).accepted &&
          s.lift_state().role == 1)) {
        record({"barrel helix", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not board the mast"});
        return;
    }
    (void)s.request_lift_action();
    if (!wait_for(s, 90.0, [&s](const Snapshot &state) {
            return std::abs(s.lift_state().travels[0] - 1.0) < 0.01 && !s.lift_state().moving && state.player_grounded;
        })) {
        record({"barrel helix", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "the mast never arrived"});
        return;
    }
    if (!(walk_to(s, -19.5, -149.25, 6.0) && walk_to(s, -23.5, -149.0, 6.0) && walk_to(s, -23.5, -154.3, 6.0) &&
          s.advance_frame(0.5).accepted && s.lift_state().role == 1)) {
        record({"barrel helix", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not board the pitman"});
        return;
    }
    (void)s.set_lift_haul(true);
    if (!wait_for(s, 90.0, [&s](const Snapshot &state) {
            return std::abs(s.lift_state().travels[1] - 1.0) < 0.01 && !s.lift_state().moving && state.player_grounded;
        })) {
        record({"barrel helix", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "the pitman never arrived"});
        return;
    }
    (void)s.set_lift_haul(false);
    if (!(walk_to(s, -27.0, -149.0, 6.0) && s.advance_frame(0.5).accepted && s.lift_state().machine == 2 &&
          s.lift_state().role == 2 && walk_to(s, -30.8, -149.0, 6.0) && s.advance_frame(0.5).accepted &&
          s.lift_state().role == 1)) {
        record({"barrel helix", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not board its deck"});
        return;
    }
    if (!ride_machine(s, 2, 0.1)) {
        record({"barrel helix", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "the deck never rose"});
        return;
    }
    (void)examine_floor(s, "barrel helix", standing_on(s), "sent, still rising");
}

// 3, the crown gondola: over the 750 deck's east parapet in a vault, onto its
// receiver, into the cabin, and up.
void exam_crown() {
    Simulation s(InitialSpawn::Deck750);
    (void)s.advance_frame(1.0);
    if (!(walk_to(s, 10.8, -152.0, 8.0) && walk_to(s, 11.3, -152.0, 3.0, 0.06))) {
        record({"crown gondola", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not reach the parapet"});
        return;
    }
    (void)s.set_facing(1.0, 0.0);
    (void)s.advance_frame(0.3);
    (void)s.request_traversal();
    const bool vaulted = hold_stick(s, 0.0, 0.0, 1.0, 0.0, 2.0, [](const Snapshot &state) {
        return state.traversal_state == scraperx::sim::TraversalState::None && state.player_grounded &&
               state.player_position.x > 12.4;
    });
    if (!(vaulted && walk_to(s, 13.5, -152.0, 4.0) && s.advance_frame(0.5).accepted && s.lift_state().machine == 3 &&
          s.lift_state().role == 2 && walk_to(s, 17.3, -152.0, 6.0) && s.advance_frame(0.5).accepted &&
          s.lift_state().role == 1)) {
        record({"crown gondola", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not board the cabin"});
        return;
    }
    if (!ride_machine(s, 3, 0.1)) {
        record({"crown gondola", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "the cabin never rose"});
        return;
    }
    (void)examine_floor(s, "crown gondola", standing_on(s), "sent, still rising and swinging");
}

// 4, the luffing derrick: up the crown first, then onto the cradle, four iron
// pigs thrown over its open west side, and up on the counterweight.
Vector3 derrick_body(const Simulation &s, const std::uint64_t entity) { return kit_at(s, entity); }

bool stand_on_cradle(Simulation &s, const double dx, const double dz, const double seconds) {
    const std::uint64_t cradle = scraperx::sim::vertical::Route::kDerrickDynamicFirst + 1;
    const int ticks = static_cast<int>(seconds * 90.0);
    for (int i = 0; i < ticks; ++i) {
        const auto c = derrick_body(s, cradle);
        const auto me = s.snapshot().player_position;
        const double ex = c.x + dx - me.x, ez = c.z + dz - me.z, d = std::hypot(ex, ez);
        const double gain = std::min(1.0, d / 0.3);
        (void)s.set_move_input(d > 0.05 ? gain * ex / d : 0.0, d > 0.05 ? gain * ez / d : 0.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
    const auto c = derrick_body(s, cradle);
    const auto me = s.snapshot().player_position;
    return std::hypot(c.x + dx - me.x, c.z + dz - me.z) < 0.25;
}

bool dump_iron(Simulation &s, const std::uint64_t entity) {
    const std::uint64_t cradle = scraperx::sim::vertical::Route::kDerrickDynamicFirst + 1;
    const auto c = derrick_body(s, cradle);
    const auto pig = derrick_body(s, entity);
    if (!stand_on_cradle(s, pig.x - c.x, pig.z - c.z + 0.6, 2.5)) {
        return false;
    }
    for (int i = 0; i < 180 && s.snapshot().carry_target_entity_id != entity; ++i) {
        const auto at = derrick_body(s, entity);
        const auto me = s.snapshot().player_position;
        const double ex = at.x - me.x, ez = at.z + 0.45 - me.z, d = std::hypot(ex, ez);
        (void)s.set_move_input(d > 0.02 ? 0.25 * ex / d : 0.0, d > 0.02 ? 0.25 * ez / d : 0.0);
        (void)s.set_facing(0.0, -1.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.set_facing(0.0, -1.0);
    (void)s.advance_frame(0.2);
    if (s.snapshot().carry_target_entity_id != entity || !s.request_pick_up()) {
        return false;
    }
    (void)s.advance_frame(0.3);
    if (s.snapshot().carrying_entity_id != entity) {
        return false;
    }
    if (!stand_on_cradle(s, -1.6, 1.0, 2.5)) {
        return false;
    }
    (void)s.set_facing(-1.0, 0.0);
    (void)s.advance_frame(0.6);
    if (!s.request_set_down()) {
        return false;
    }
    (void)s.advance_frame(1.5);
    return s.snapshot().carrying_entity_id == 0;
}

void exam_derrick() {
    Simulation s(InitialSpawn::Deck750);
    (void)s.advance_frame(1.0);
    if (!(walk_to(s, 10.8, -152.0, 8.0) && walk_to(s, 11.3, -152.0, 3.0, 0.06))) {
        record({"luffing derrick", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not reach the parapet"});
        return;
    }
    (void)s.set_facing(1.0, 0.0);
    (void)s.advance_frame(0.3);
    (void)s.request_traversal();
    const bool vaulted = hold_stick(s, 0.0, 0.0, 1.0, 0.0, 2.0, [](const Snapshot &state) {
        return state.traversal_state == scraperx::sim::TraversalState::None && state.player_grounded &&
               state.player_position.x > 12.4;
    });
    if (!(vaulted && walk_to(s, 13.5, -152.0, 4.0) && s.advance_frame(0.5).accepted && s.lift_state().machine == 3 &&
          s.lift_state().role == 2 && walk_to(s, 17.3, -152.0, 6.0) && s.advance_frame(0.5).accepted &&
          s.lift_state().role == 1)) {
        record({"luffing derrick", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not board the crown"});
        return;
    }
    double crown_s = 0.0;
    const double crown_start = s.snapshot().simulation_time_seconds;
    (void)s.request_lift_action();
    if (!wait_for(s, 120.0, [&s](const Snapshot &state) {
            return std::abs(s.lift_state().travels[3] - 1.0) < 0.01 && !s.lift_state().moving && state.player_grounded;
        })) {
        record({"luffing derrick", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "the crown never arrived"});
        return;
    }
    crown_s = s.snapshot().simulation_time_seconds - crown_start;
    (void)crown_s;
    if (!(walk_to(s, 13.5, -151.5, 6.0) && s.advance_frame(0.5).accepted && s.lift_state().role == 3 &&
          walk_to(s, 13.5, -156.5, 6.0) && s.advance_frame(0.5).accepted && s.lift_state().machine == 4 &&
          s.lift_state().role == 2 && walk_to(s, 12.3, -157.5, 4.0) && walk_to(s, 12.3, -161.3, 6.0))) {
        record({"luffing derrick", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not reach the derrick's lower receiver"});
        return;
    }
    // Onto the cradle, at the rider's spot, and four pigs over the side:
    // with two left and the rider it is lighter than the counterweight holds.
    // Nearest the rider first, as the rides test does: the pigs lie in two
    // rows of three running south from beside the rider's spot.
    const std::uint64_t cradle = scraperx::sim::vertical::Route::kDerrickDynamicFirst + 1;
    const std::uint64_t iron_first = cradle + 1;
    if (!stand_on_cradle(s, -1.2, 0.0, 3.0)) {
        record({"luffing derrick", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not stand on the cradle"});
        return;
    }
    (void)s.advance_frame(5.0);
    int dumped = 0;
    for (const std::uint64_t pig : {iron_first + 2, iron_first + 5, iron_first + 1, iron_first + 4}) {
        if (dumped == 3) {
            // Three left and the rider: what the counterweight holds.
            (void)stand_on_cradle(s, -1.2, 0.0, 5.0);
        }
        if (dump_iron(s, pig)) {
            ++dumped;
        }
    }
    if (dumped < 4) {
        record({"luffing derrick", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not throw four pigs over the side"});
        return;
    }
    if (!wait_for(s, 90.0, [&s](const Snapshot &) { return s.lift_state().travels[4] > 0.1; })) {
        record({"luffing derrick", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "the cradle never luffed up"});
        return;
    }
    (void)examine_floor(s, "luffing derrick", standing_on(s), "luffing up on the counterweight");
}

// 5, the slab incline: across the yard, up the ramp, onto the trolley; his
// weight on the trip plate pulls the pin and the slab hauls it up.
void exam_incline() {
    Simulation s(InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    if (!(walk_to(s, 6.0, -70.0, 20.0) && walk_to(s, -13.4, -86.0, 20.0) && walk_to(s, -13.4, -103.0, 12.0) &&
          s.advance_frame(0.5).accepted && s.lift_state().machine == 5 && s.lift_state().role == 2 &&
          walk_to(s, -10.0, -103.08, 6.0) && s.advance_frame(0.5).accepted && s.lift_state().role == 1)) {
        record({"slab incline", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not board the trolley"});
        return;
    }
    // Stand on the trip plate until the pin lets go and the trolley runs.
    const std::uint64_t trip = scraperx::sim::vertical::Route::kInclineDynamicFirst + 1;
    for (int i = 0; i < 90 && s.lift_state().travels[5] < 0.01; ++i) {
        const auto at = kit_at(s, trip);
        const auto me = s.snapshot().player_position;
        const double dx = at.x - me.x, dz = at.z - me.z, d = std::hypot(dx, dz);
        (void)s.set_move_input(d > 0.05 ? dx / d : 0.0, d > 0.05 ? dz / d : 0.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
    if (!wait_for(s, 60.0, [&s](const Snapshot &) { return s.lift_state().travels[5] > 0.1; })) {
        record({"slab incline", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "the trolley never ran"});
        return;
    }
    (void)examine_floor(s, "slab incline", standing_on(s), "hauled up by the slab, unbraked");
}

// 6, the stone wheel: fed stone out on the plank, board the bucket, and up on
// the wheel's turn.
void exam_wheel() {
    constexpr std::uint64_t kRiderBucket = 2936 + 4;
    constexpr std::uint64_t kEntry = 1984 + 1;
    Simulation s(InitialSpawn::Deck2South);
    (void)s.advance_frame(1.0);
    if (!(walk_to(s, 14.19, -125.0, 10.0) && walk_to(s, 14.19, -113.3, 12.0) && s.advance_frame(0.5).accepted &&
          s.snapshot().support_entity_id == kEntry && walk_to(s, 20.2, -113.3, 8.0) && s.advance_frame(8.0).accepted &&
          walk_to(s, 14.19, -113.3, 8.0) && walk_to(s, 14.19, -110.4, 6.0) && s.advance_frame(0.5).accepted &&
          s.snapshot().support_entity_id == kRiderBucket)) {
        record({"stone wheel", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not feed and board the bucket"});
        return;
    }
    const double y0 = kit_at(s, kRiderBucket).y;
    if (!wait_for(s, 120.0, [&s, y0](const Snapshot &) { return kit_at(s, kRiderBucket).y > y0 + 1.0; })) {
        record({"stone wheel", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "the bucket never rose"});
        return;
    }
    (void)examine_floor(s, "stone wheel", standing_on(s), "rising on the wheel's turn");
}

// 7, the traction tram: along the bridge to its lower receiver, aboard in its
// wait at the foot, and up on its own schedule.
void exam_tram() {
    Simulation s(InitialSpawn::Deck6South);
    (void)s.advance_frame(1.0);
    if (!(walk_to(s, 18.5, -128.0, 12.0) && walk_to(s, 18.5, -153.0, 12.0) && walk_to(s, 79.5, -153.0, 25.0) &&
          walk_to(s, 81.2, -153.0, 4.0) && walk_to(s, 81.1, -157.5, 6.0) && walk_to(s, 80.6, -161.0, 8.0) &&
          s.advance_frame(0.5).accepted && s.lift_state().machine == 7 && s.lift_state().role == 2)) {
        record({"traction tram", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not reach the lower receiver"});
        return;
    }
    const auto standing_at = [&s](const double end, const double seconds_left) {
        const auto lift = s.lift_state();
        return std::abs(lift.travels[7] - end) < 0.01 && lift.tram_departs_s >= seconds_left;
    };
    if (!wait_for(s, 120.0, [&](const Snapshot &) { return standing_at(0.0, 3.0); })) {
        record({"traction tram", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "the carrier never waited at the foot"});
        return;
    }
    if (!(walk_to(s, 74.0, -161.0, 6.0) && s.advance_frame(0.3).accepted && s.lift_state().role == 1)) {
        record({"traction tram", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not board the carrier"});
        return;
    }
    (void)s.request_lift_action();
    if (!wait_for(s, 90.0, [&s](const Snapshot &) { return s.lift_state().travels[7] > 0.1; })) {
        record({"traction tram", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "the carrier never left the foot"});
        return;
    }
    (void)examine_floor(s, "traction tram", standing_on(s), "shuttling on its own schedule");
}

}  // namespace

namespace {

// ---- The well and stack machines: one ascent, an exam on each car ------------
using scraperx::sim::InitialSpawn;
// Follows run_ascent's legs exactly; each car is examined mid-ride, while it
// is still moving. After each exam the ascent resumes: a SUPPORT verdict
// means he is already back on the car, rising again; anything else means he
// is put back on the car's live position first. If that fails the ascent
// cannot continue, and the remaining cars are recorded unmeasured.

// Examine the car he rides, then get the ascent moving again. Returns false
// when the ascent cannot continue.
bool exam_ascent_car(Simulation &s, const char *name, const std::uint64_t car_entity, const char *note) {
    const std::uint64_t floor = standing_on(s);
    (void)examine_floor(s, name, floor, note);
    const auto st = s.snapshot();
    if (st.player_grounded && st.support_entity_id == car_entity) {
        return true;
    }
    return recover_on_car(s, car_entity);
}

void exam_well_ascent() {
    // The well cars in ascent order, so a break records every car after it as
    // unmeasured instead of silently dropping it from the table.
    static const char *kWellFloors[] = {"well A cage",     "well B cage",     "well C platform",
                                        "wet D platform",  "wet E cab",       "wet F platform",
                                        "shop G platform", "shop H platform", "shop I cage"};
    const auto record_rest = [](const std::size_t from, const char *why) {
        for (std::size_t i = from; i < sizeof(kWellFloors) / sizeof(kWellFloors[0]); ++i) {
            record({kWellFloors[i], "SETUP-FAILED", 0.0, 0.0, 0, 0, why});
        }
    };
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    (void)s.advance_frame(1.0);
    // A, the counterweight well: rigged, tripped, and up on the rope.
    if (!(board_well_a(s) && rig_well_a(s) && pull_well_a(s, 0.5, 3.0))) {
        record({"well A cage", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not rig and trip A"});
        record_rest(1, "ascent broken at A");
        return;
    }
    if (!wait_for(s, 16.0, [](const Snapshot &state) { return state.well_a_cage_travel > 1.0; })) {
        record({"well A cage", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "A never rose"});
        return;
    }
    if (!exam_ascent_car(s, "well A cage", Simulation::kWellACageEntityId, "rising on the counterweight rope") ||
        !wait_for(s, 16.0, [](const Snapshot &state) { return state.well_a_cage_travel >= kWellATravel - 0.01; })) {
        record_rest(1, "ascent broken at A");
        return;
    }
    (void)s.advance_frame(1.0);
    // B, the boom: across into its cage, rigged and tripped.
    if (!(walk_to(s, -9.4, -131.2, 4.0) && walk_to(s, -7.6, -131.2, 4.0) && rig_well_b(s) && pull_well_b(s))) {
        record({"well B cage", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not rig and trip B"});
        record_rest(2, "ascent broken at B");
        return;
    }
    if (!wait_for(s, 16.0, [](const Snapshot &state) { return state.well_b_cage_travel > 1.0; })) {
        record({"well B cage", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "B never rose"});
        return;
    }
    if (!exam_ascent_car(s, "well B cage", Simulation::kWellBCageEntityId, "rising on the boom") ||
        !wait_for(s, 16.0, [](const Snapshot &state) {
            return state.well_b_cage_travel >= kWellATravel - kWellDogPitch - 0.01;
        })) {
        record_rest(2, "ascent broken at B");
        return;
    }
    (void)s.advance_frame(3.0);
    // C, the water platform: across onto it, its chute cleared, filled, let go.
    if (!(walk_to(s, -6.2, -131.9, 4.0) && walk_to(s, -4.4, -131.9, 4.0) && clear_well_c_chute(s) &&
          fill_well_c(s, 10.0) && pull_well_c_latch(s, 3.0))) {
        record({"well C platform", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not fill and release C"});
        record_rest(3, "ascent broken at C");
        return;
    }
    if (!wait_for(s, 16.0, [](const Snapshot &state) { return state.well_c_platform_travel > 1.0; })) {
        record({"well C platform", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "C never rose"});
        return;
    }
    if (!exam_ascent_car(s, "well C platform", Simulation::kWellCPlatformEntityId,
                         "floating up on the water; the level is machine state, so the restore returns it with the car") ||
        !wait_for(s, 16.0, [](const Snapshot &state) {
            return state.well_c_platform_travel >= kWellATravel - kWellDogPitch - 0.01;
        })) {
        record_rest(3, "ascent broken at C");
        return;
    }
    (void)s.advance_frame(2.0);
    if (!walk_to(s, -3.0, -129.0, 4.0)) {
        record({"wet D platform", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not step off C onto the 220 ring"});
        record_rest(5, "ascent broken at C");
        return;
    }
    // D, the spool: its spool carried aboard and seated.
    if (!(walk_to(s, 2.5, -129.6, 8.0) && walk_to(s, 4.0, -129.2, 6.0) && seat_wet_spool(s))) {
        record({"wet D platform", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not seat D's spool"});
        record_rest(4, "ascent broken at D");
        return;
    }
    if (!wait_for(s, 120.0, [&s](const Snapshot &) { return s.wet_state().d_platform_travel > 1.0; })) {
        record({"wet D platform", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "D never rose"});
        return;
    }
    {
        const std::uint64_t car = standing_on(s);
        if (!exam_ascent_car(s, "wet D platform", car, "hauled up by the spool; the wound rope is machine state") ||
            !wait_for(s, 120.0, [&s](const Snapshot &) { return s.wet_state().d_platform_travel >= 35.95; })) {
            record_rest(5, "ascent broken at D");
            return;
        }
    }
    // E, the blown cab: through its doors onto its deck, up on the fan.
    if (!drop_into_wet_e(s)) {
        record({"wet E cab", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not drop into E"});
        record_rest(6, "ascent broken at E");
        return;
    }
    if (!wait_for(s, 60.0, [&s](const Snapshot &) { return s.wet_state().e_cab_travel > 5.0; })) {
        record({"wet E cab", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "E never rose"});
        return;
    }
    {
        const std::uint64_t car = standing_on(s);
        if (!exam_ascent_car(s, "wet E cab", car, "blown up by the fan; the air pressure is machine state") ||
            !wait_wet_e_top(s, 60.0)) {
            record_rest(6, "ascent broken at E");
            return;
        }
    }
    // F, the hose platform: onto it, its hose coupled, its valve thrown.
    if (!(walk_to(s, 13.4, -137.2, 4.0) && walk_to(s, 13.4, -139.9, 6.0) && couple_wet_hose(s) && pull_wet_stop_valve(s))) {
        record({"wet F platform", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not couple F's hose and throw its valve"});
        record_rest(7, "ascent broken at F");
        return;
    }
    if (!wait_for(s, 90.0, [&s](const Snapshot &) { return s.wet_state().f_platform_travel > 1.0; })) {
        record({"wet F platform", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "F never rose"});
        return;
    }
    {
        const std::uint64_t car = standing_on(s);
        if (!exam_ascent_car(s, "wet F platform", car,
                             "rising on the hose water; the level is machine state") ||
            !wait_for(s, 90.0, [&s](const Snapshot &) { return s.wet_state().f_platform_travel >= 41.95; })) {
            record_rest(7, "ascent broken at F");
            return;
        }
    }
    if (!walk_to(s, 13.0, -137.2, 6.0)) {
        record({"shop G platform", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not step off F onto TP-340"});
        record_rest(7, "ascent broken at F");
        return;
    }
    // G, the shop platform: rigged from its cleat, its pin pulled.
    if (!(walk_to_shop_cleat(s) && rig_shop_g(s) && pull_shop_g(s))) {
        record({"shop G platform", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not rig and trip G"});
        record_rest(8, "ascent broken at G");
        return;
    }
    if (!wait_for(s, 60.0, [&s](const Snapshot &) { return s.shop_state().g_platform_travel > 1.0; })) {
        record({"shop G platform", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "G never rose"});
        return;
    }
    {
        const std::uint64_t car = standing_on(s);
        if (!exam_ascent_car(s, "shop G platform", car, "rising on its counterweight") ||
            !wait_for(s, 60.0, [&s](const Snapshot &) { return s.shop_state().g_platform_travel >= 33.7; })) {
            record_rest(7, "ascent broken at G");
            return;
        }
    }
    (void)s.advance_frame(1.0);
    // H, the chock platform: the girder's tail pin out, over the gangway, its
    // chock pulled.
    if (!(walk_to(s, -14.5, -149.3, 8.0) && pull_shop_girder_pin(s) && board_shop_h(s) && pull_shop_chock(s))) {
        record({"shop H platform", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not board H and pull its chock"});
        record_rest(8, "ascent broken at H");
        return;
    }
    if (!wait_for(s, 90.0, [&s](const Snapshot &) { return s.shop_state().h_platform_travel > 1.0; })) {
        record({"shop H platform", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "H never rose"});
        return;
    }
    {
        const std::uint64_t car = standing_on(s);
        if (!exam_ascent_car(s, "shop H platform", car, "rolling up once its chock is pulled") ||
            !wait_for(s, 90.0, [&s](const Snapshot &) { return s.shop_state().h_platform_travel >= 43.9; })) {
            record_rest(8, "ascent broken at H");
            return;
        }
    }
    (void)s.advance_frame(1.0);
    // I, the domino cage: hooked on, the domino's pin pulled.
    if (!(rig_shop_i(s) && pull_shop_domino(s))) {
        record({"shop I cage", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not rig I and pull its domino"});
        return;
    }
    if (!wait_for(s, 90.0, [&s](const Snapshot &) { return s.shop_state().i_cage_travel > 1.0; })) {
        record({"shop I cage", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "I never rose"});
        return;
    }
    {
        const std::uint64_t car = standing_on(s);
        if (!exam_ascent_car(s, "shop I cage", car, "hauled up by the falling domino") ||
            !wait_for(s, 90.0, [&s](const Snapshot &) { return s.shop_state().i_cage_travel >= 43.9; })) {
            return;
        }
    }
    (void)s.advance_frame(1.0);
    (void)walk_to(s, -8.1, -145.6, 8.0);
    (void)walk_to(s, -8.1, -147.3, 8.0);
    (void)walk_to(s, -7.7, -147.85, 8.0);
    (void)climb_wet_hold(s, -7.6, -148.1, -1.0, 0.0, false, 484.5);
    std::cout << "  ascent examined: A B C D E F G H I, standing on the 484 ring, deaths=" << s.snapshot().death_count
              << '\n';
}

// ---- The facade-crane machines J, K, L: each in its own sim ------------------
// The crane helpers are the facade crane test's own, copied here (they are
// local to that file): lay the joint, pull the chock, rig the shackle, throw
// the clutch, pull the drop weight's pin.

bool lay_crane_joint(Simulation &s) {
    if (!walk_to(s, 2.8, -139.0, 12.0, 0.08)) {
        return false;
    }
    (void)s.set_facing(1.0, 0.0);
    (void)s.advance_frame(0.4);
    if (s.snapshot().carry_target_entity_id != Simulation::kCraneJJointEntityId) {
        return false;
    }
    (void)s.request_pick_up();
    (void)s.advance_frame(0.4);
    if (s.snapshot().carrying_entity_id != Simulation::kCraneJJointEntityId ||
        !(walk_to(s, -0.3, -138.3, 8.0) && walk_to(s, -0.3, -136.2, 6.0, 0.08) && walk_to(s, 0.9, -136.2, 6.0, 0.08))) {
        return false;
    }
    (void)s.set_facing(1.0, 0.0);
    (void)s.advance_frame(0.8);
    (void)s.request_set_down();
    (void)s.advance_frame(1.5);
    return s.crane_state().j_rail_whole;
}

bool pull_crane_chock(Simulation &s) {
    return pull_facing(s, 0.0, -135.5, 0.0, 1.0, Simulation::kCraneJHandleEntityId, 0.5, 4.0,
                       [&](const Snapshot &) { return !s.crane_state().j_wagon_latched; });
}

bool rig_crane_k(Simulation &s) {
    const double x0 = s.snapshot().player_position.x;
    if (!(walk_to(s, x0, -154.2, 6.0) && walk_to(s, -13.3, -154.2, 10.0) && walk_to(s, -13.3, -155.0, 4.0, 0.08))) {
        return false;
    }
    (void)s.set_facing(1.0, 0.0);
    (void)s.advance_frame(0.4);
    if (s.snapshot().carry_target_entity_id != Simulation::kCraneKShackleEntityId) {
        return false;
    }
    (void)s.request_pick_up();
    (void)s.advance_frame(0.4);
    if (s.snapshot().carrying_entity_id != Simulation::kCraneKShackleEntityId ||
        !walk_to(s, -12.3, -155.0, 4.0, 0.08)) {
        return false;
    }
    (void)s.set_facing(1.0, 0.0);
    (void)s.advance_frame(0.8);
    const auto at_eye = s.snapshot();
    if (at_eye.rig_action != 1 || at_eye.rig_target_entity_id != Simulation::kCraneKCageEntityId) {
        return false;
    }
    (void)s.request_rig();
    (void)s.advance_frame(0.3);
    return s.crane_state().k_rope_on_eye;
}

bool pull_crane_pendant(Simulation &s) {
    return pull_facing(s, -12.6, -154.3, 0.0, 1.0, Simulation::kCraneKHandleEntityId, 0.5, 4.0,
                       [&](const Snapshot &) { return !s.crane_state().k_jib_latched; });
}

bool throw_crane_clutch(Simulation &s) {
    return pull_facing(s, -3.7, -159.4, 0.0, 1.0, Simulation::kCraneLClutchHandleEntityId, 0.5, 4.0,
                       [&](const Snapshot &) { return s.crane_state().l_clutch_in; });
}

bool pull_crane_drop(Simulation &s) {
    return pull_facing(s, -3.0, -160.8, 0.0, -1.0, Simulation::kCraneLPinHandleEntityId, 0.5, 4.0,
                       [&](const Snapshot &) { return !s.crane_state().l_weight_latched; });
}

// J, the runaway wagon's traveler.
void exam_crane_j() {
    Simulation s(InitialSpawn::Ring484North);
    (void)s.advance_frame(1.0);
    if (!lay_crane_joint(s)) {
        record({"facade J traveler", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not lay the rail's joint"});
        return;
    }
    if (!(walk_to(s, 0.0, -138.3, 8.0) && walk_to(s, 0.0, -136.2, 6.0) && pull_crane_chock(s))) {
        record({"facade J traveler", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not pull the wagon's chock"});
        return;
    }
    if (!wait_for(s, 60.0, [&s](const Snapshot &) { return s.crane_state().j_traveler_travel > 0.5; })) {
        record({"facade J traveler", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "the traveler never ran"});
        return;
    }
    (void)examine_floor(s, "facade J traveler", standing_on(s), "dragged up by the runaway wagon");
}

// K, the jib's cage.
void exam_crane_k() {
    Simulation s(InitialSpawn::CraneKCage);
    (void)s.advance_frame(1.0);
    if (!rig_crane_k(s)) {
        record({"facade K cage", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not hook the shackle on the cage's eye"});
        return;
    }
    if (!pull_crane_pendant(s)) {
        record({"facade K cage", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not pull the pendant's pin"});
        return;
    }
    if (!wait_for(s, 90.0, [&s](const Snapshot &) { return s.crane_state().k_cage_travel > 0.5; })) {
        record({"facade K cage", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "the cage never rose"});
        return;
    }
    (void)examine_floor(s, "facade K cage", standing_on(s), "hauled up by the swinging jib");
}

// L, the clutch's cab.
void exam_crane_l() {
    Simulation s(InitialSpawn::CraneLCab);
    (void)s.advance_frame(1.0);
    if (!throw_crane_clutch(s)) {
        record({"facade L cab", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not throw the winch's clutch in"});
        return;
    }
    if (!pull_crane_drop(s)) {
        record({"facade L cab", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not pull the drop weight's pin"});
        return;
    }
    if (!wait_for(s, 90.0, [&s](const Snapshot &) { return s.crane_state().l_cab_travel > 0.5; })) {
        record({"facade L cab", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "the cab never rose"});
        return;
    }
    (void)examine_floor(s, "facade L cab", standing_on(s), "wound up by the cart and the drop weight");
}

// S1, the yard's water cage: aboard on its scale plate, up on the bucket.
void exam_s1() {
    Simulation s(InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    if (!board_s1(s)) {
        record({"stack S1 cage", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not board the cage"});
        return;
    }
    if (!wait_for(s, 120.0, [&s](const Snapshot &) { return s.stack_state().s1_cage_travel > 0.5; })) {
        record({"stack S1 cage", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "the cage never rose"});
        return;
    }
    (void)examine_floor(s, "stack S1 cage", standing_on(s), "rising on the bucket's water");
}

}  // namespace

namespace {

// ---- The bay's leaf ----------------------------------------------------------
using scraperx::sim::InitialSpawn;
// The leaf holds level on its motor; it dumps only when her hands trip the
// district, and the bay trap test already proves he rides the dump down
// through the bay and lives. So no death follows a dump, and the exam's kill
// is the test's own drop onto the leaf.
void exam_bay_leaf() {
    Simulation s(InitialSpawn::Deck154);
    (void)s.advance_frame(1.0);
    const double cx = 0.5 * (static_cast<double>(scraperx::sim::Bay::kLeafWestX) +
                             static_cast<double>(scraperx::sim::Bay::kLeafEastX));
    s.debug_launch_player({cx, static_cast<double>(scraperx::sim::Bay::kDeckTop) + 1.0,
                           static_cast<double>(scraperx::sim::Bay::kLeafZ)},
                          {0.0, 0.0, 0.0});
    bool on_leaf = false;
    for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) && !on_leaf; ++i) {
        tick(s);
        const auto st = s.snapshot();
        on_leaf = st.player_grounded && st.support_entity_id == scraperx::sim::Bay::kLeafEntity;
    }
    if (!on_leaf) {
        record({"bay leaf", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not stand on the leaf"});
        return;
    }
    (void)examine_floor(s, "bay leaf", scraperx::sim::Bay::kLeafEntity,
                        "level on its motor; it dumps only under her hands, and the bay trap proves he lives the dump");
}

// ---- The yard forklift -------------------------------------------------------
// From the forklift test: round the beam stack's west end, onto the truck's
// floor from its west side, to the wheel, facing its forward (+z).

bool board_forklift(Simulation &s) {
    if (!(walk_to(s, 30.0, 10.0, 30.0) && walk_to(s, 36.4, 16.35, 20.0) &&
          walk_to(s, scraperx::sim::Forklift::kParkX, 16.35, 8.0, 0.1))) {
        return false;
    }
    (void)s.set_facing(0.0, 1.0);
    (void)s.advance_frame(0.5);
    return s.forklift_state().at_controls;
}

// The truck's floor, driving forward: the truck coasts after he leaves, and
// the drive is held on through the kill flight so it keeps moving under power.
void exam_forklift_truck() {
    Simulation s(InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    if (!board_forklift(s) || !s.request_forklift_action()) {
        record({"forklift truck floor", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id,
                s.snapshot().death_count, "could not take the controls"});
        return;
    }
    bool moving = false;
    for (int i = 0; i < 10 * static_cast<int>(Simulation::kTickRateHz) && !moving; ++i) {
        (void)s.set_move_input(0.0, 0.25);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        moving = s.forklift_state().speed_mps > 0.3;
    }
    (void)s.set_move_input(0.0, 0.0);
    if (!moving) {
        record({"forklift truck floor", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id,
                s.snapshot().death_count, "the truck never drove"});
        return;
    }
    (void)examine_floor(s, "forklift truck floor", scraperx::sim::Forklift::kChassisEntity,
                        "driving forward, coasting after he leaves", truck_drive_on);
}

// The forks, raised: the lift holds its height once the lever is let go, so
// they do not move after he leaves.
void exam_forklift_forks() {
    Simulation s(InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    if (!board_forklift(s) || !s.request_forklift_action()) {
        record({"forklift forks", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not take the controls"});
        return;
    }
    (void)s.set_forklift_lift(1.0);
    (void)s.advance_frame(3.0);
    (void)s.set_forklift_lift(0.0);
    (void)s.advance_frame(0.5);
    if (s.forklift_state().lift_m < 1.0) {
        record({"forklift forks", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "the forks never rose"});
        return;
    }
    const auto carriage = kit_at(s, scraperx::sim::Forklift::kCarriageEntity);
    // Over one blade, not between them: the blades are at x = +/-0.30 in the
    // carriage frame, and the commit's firm-footing ray is cast straight down
    // from his centre, so a centre between the blades lets the ray pass
    // between them and no commit is ever taken on the forks.
    s.debug_launch_player({carriage.x + 0.30, carriage.y + 1.0, carriage.z + 0.8}, {0.0, 0.0, 0.0});
    bool on_forks = false;
    for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) && !on_forks; ++i) {
        tick(s);
        const auto st = s.snapshot();
        on_forks = st.player_grounded && st.support_entity_id == scraperx::sim::Forklift::kCarriageEntity;
    }
    if (!on_forks) {
        record({"forklift forks", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not stand on the forks"});
        return;
    }
    // Keep him over the blade until the commit: the blade is 10 cm wide, and
    // without steering he drifts off it and the ray misses again.
    for (int i = 0; i < 2 * static_cast<int>(Simulation::kTickRateHz); ++i) {
        const auto c = kit_at(s, scraperx::sim::Forklift::kCarriageEntity);
        const auto me = s.snapshot().player_position;
        const double ex = (c.x + 0.30) - me.x, ez = (c.z + 0.8) - me.z, d = std::hypot(ex, ez);
        const double gain = std::min(1.0, d / 0.3);
        (void)s.set_move_input(d > 0.05 ? gain * ex / d : 0.0, d > 0.05 ? gain * ez / d : 0.0);
        tick(s);
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)examine_floor(s, "forklift forks", scraperx::sim::Forklift::kCarriageEntity,
                        "raised; the lift holds its height once the lever is let go, so they do not move after he leaves");
}

// ---- The yard crane ----------------------------------------------------------
// The crane helpers are the yard crane test's own, copied here (they are
// local to that file).

Vector3 crane_body_at(const Simulation &s, const std::uint64_t entity) { return kit_at(s, entity); }

bool walk_to_cabinet(Simulation &s) {
    if (!(walk_to(s, -25.0, -40.0, 40.0) && walk_to(s, -50.0, -100.0, 50.0) && walk_to(s, -50.0, -125.0, 30.0) &&
          walk_to(s, scraperx::sim::YardCrane::kMastX + 4.2, scraperx::sim::YardCrane::kMastZ + 5.6, 30.0, 0.1))) {
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

bool crane_work(Simulation &s, const bool on) {
    if (s.yard_crane_state().operating == on) {
        return true;
    }
    if (!s.request_yard_crane_operate()) {
        return false;
    }
    (void)s.advance_frame(0.1);
    return s.yard_crane_state().operating == on;
}

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
    return false;
}

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
    return false;
}

bool take_shackle(Simulation &s, const double budget) {
    const auto ticks = static_cast<int>(budget * static_cast<double>(Simulation::kTickRateHz));
    for (int tick = 0; tick < ticks; ++tick) {
        const auto state = s.snapshot();
        if (state.carry_target_entity_id == scraperx::sim::YardCrane::kShackleEntity) {
            (void)s.set_move_input(0.0, 0.0);
            if (!s.request_pick_up()) {
                return false;
            }
            (void)s.advance_frame(0.3);
            return s.snapshot().carrying_entity_id == scraperx::sim::YardCrane::kShackleEntity;
        }
        const auto shackle = crane_body_at(s, scraperx::sim::YardCrane::kShackleEntity);
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

bool hook_on(Simulation &s, const double fx, const double fz, const std::uint64_t load) {
    (void)s.set_facing(fx, fz);
    (void)s.advance_frame(1.5);
    const auto state = s.snapshot();
    if (state.rig_action != 1 || state.rig_target_entity_id != load || !s.request_rig()) {
        return false;
    }
    (void)s.advance_frame(0.5);
    return s.yard_crane_state().load_entity == load;
}

bool crane_magnet(Simulation &s, const bool on) {
    if (s.yard_crane_state().magnet_on == on) {
        return true;
    }
    if (!s.request_yard_crane_device()) {
        return false;
    }
    (void)s.advance_frame(0.1);
    return s.yard_crane_state().magnet_on == on;
}

bool device_onto(Simulation &s, const std::uint64_t device, const double kg, const double rise, const double x,
                 const double z) {
    if (!(hoist_to(s, s.yard_crane_state().hook.y + rise, 40.0) && hook_over(s, x, z, 120.0))) {
        return false;
    }
    const std::uint32_t body = s.kit_body_index(device);
    const int hold = 2 * static_cast<int>(Simulation::kTickRateHz);
    const auto settle = [&s, body](const double tolerance, const double tx, const double tz) {
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
    return false;
}

bool magnet_onto(Simulation &s, const double x, const double z) {
    return device_onto(s, scraperx::sim::YardCrane::kMagnetEntity, scraperx::sim::YardCrane::kMagnetKg, 2.0, x, z);
}

bool hoist_load_to(Simulation &s, const std::uint64_t entity, const double y, const double budget) {
    const auto ticks = static_cast<int>(budget * static_cast<double>(Simulation::kTickRateHz));
    for (int tick = 0; tick < ticks; ++tick) {
        if (crane_body_at(s, entity).y >= y) {
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

// The hook block itself, hung from the hoist rope: the rope is the floor's
// drive, and the restore takes the rope's length as it stands, not as it was
// committed. If the hook keeps rising after he leaves, the restore puts him
// back where the hook was and the rope lets it fall away under him.
//
// The commit is taken on the STILL hook, before the rise: once it rises it
// swings 4.5 m on its rope, and he slides to its west side (0.4 m off its
// centre), where the commit's firm-footing ray — 1.05 m down from his centre
// — cannot reach the block's top, so no fresh commit ever renews on the
// rising hook. The still hook takes a commit (its top is in reach while he
// stands on it), so the exam commits there, then rises the hook, then kills
// him: the rope pays out while he is dead, which is the stale-restore test.
void exam_crane_hook() {
    Simulation s(InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    if (!take_remote(s) || !crane_work(s, true)) {
        record({"yard crane hook", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not take the remote and work the crane"});
        return;
    }
    const auto hook = crane_body_at(s, scraperx::sim::YardCrane::kHookEntity);
    // Set him down gently on the sheave rim, not dropped a metre onto it: the
    // rim is a 0.1 m curved line, and a 1 m drop knocks him off it to the
    // block's side before any tick can take a commit. His centre standing on
    // the rim is the rim top (hook origin + 0.67 m) plus the capsule's
    // half-length (0.9 + 0.35 m).
    s.debug_launch_player({hook.x, hook.y + 0.67 + 1.25 + 0.05, hook.z}, {0.0, 0.0, 0.0});
    bool on_hook = false;
    for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) && !on_hook; ++i) {
        tick(s);
        const auto st = s.snapshot();
        on_hook = st.player_grounded && st.support_entity_id == scraperx::sim::YardCrane::kHookEntity;
    }
    if (!on_hook) {
        record({"yard crane hook", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not stand on the hook block"});
        return;
    }
    Vector3 commit_pos{};
    if (!wait_for_commit(s, scraperx::sim::YardCrane::kHookEntity, &commit_pos)) {
        record({"yard crane hook", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "no commit on the still hook block"});
        return;
    }
    (void)s.set_yard_crane_hoist(1.0);
    const double y0 = s.yard_crane_state().hook.y;
    if (!wait_for(s, 30.0, [&s, y0](const Snapshot &) { return s.yard_crane_state().hook.y > y0 + 0.5; })) {
        record({"yard crane hook", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "the hook never rose"});
        return;
    }
    (void)kill_and_verdict(s, "yard crane hook", scraperx::sim::YardCrane::kHookEntity, commit_pos,
                           "hoisting up; the restore takes the rope length as it stands", crane_hoist_on);
}

// The magnet's load: a steel slab held by the magnet, hoisted up. Same rope
// as the hook, with a load on it.
void exam_crane_magnet() {
    Simulation s(InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    const std::uint64_t top = stock_at(s, -64.0, 0.458, -124.0);
    if (top == 0) {
        record({"yard crane magnet load", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id,
                s.snapshot().death_count, "the top slab is not where the magnet test leaves it"});
        return;
    }
    const auto magnet_at = crane_body_at(s, scraperx::sim::YardCrane::kMagnetEntity);
    if (!take_remote(s) || !(crane_work(s, true) && hook_over(s, magnet_at.x, magnet_at.z, 120.0) &&
                             hoist_to(s, 0.55 + scraperx::sim::YardCrane::kSlingM, 60.0) && crane_work(s, false))) {
        record({"yard crane magnet load", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id,
                s.snapshot().death_count, "could not let the hook down over the magnet"});
        return;
    }
    if (!(walk_to(s, magnet_at.x, magnet_at.z - 1.3, 20.0, 0.1) && take_shackle(s, 8.0) &&
          walk_to(s, magnet_at.x, magnet_at.z - 1.0, 6.0, 0.08) &&
          hook_on(s, 0.0, 1.0, scraperx::sim::YardCrane::kMagnetEntity))) {
        record({"yard crane magnet load", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id,
                s.snapshot().death_count, "could not hook the sling on the magnet's bail"});
        return;
    }
    if (!crane_work(s, true) || !crane_magnet(s, true)) {
        record({"yard crane magnet load", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id,
                s.snapshot().death_count, "could not switch the magnet on"});
        return;
    }
    const auto slab0 = crane_body_at(s, top);
    if (!magnet_onto(s, slab0.x, slab0.z)) {
        record({"yard crane magnet load", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id,
                s.snapshot().death_count, "could not let the magnet down on the slabs"});
        return;
    }
    (void)s.advance_frame(0.5);
    if (!hoist_load_to(s, top, slab0.y + 2.0, 40.0)) {
        record({"yard crane magnet load", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id,
                s.snapshot().death_count, "the magnet never lifted the slab"});
        return;
    }
    const auto slab = crane_body_at(s, top);
    // Not at the slab's centre: the magnet (1.2 m across) sits there, and a
    // placement at the centre lands him on the magnet, not the slab. The slab
    // is 2.4 x 1.2 m, so its ends are free strips 0.6 m wide: stand him at
    // +0.9 m along its length, or the other end if that fails.
    bool on_slab = false;
    for (const double off : {0.9, -0.9}) {
        s.debug_launch_player({slab.x + off, slab.y + 1.0, slab.z}, {0.0, 0.0, 0.0});
        for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) && !on_slab; ++i) {
            tick(s);
            const auto st = s.snapshot();
            on_slab = st.player_grounded && st.support_entity_id == top;
        }
        if (on_slab) {
            break;
        }
    }
    if (!on_slab) {
        record({"yard crane magnet load", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id,
                s.snapshot().death_count, "could not stand on the slab clear of the magnet"});
        return;
    }
    (void)s.set_yard_crane_hoist(1.0);
    const double y0 = crane_body_at(s, top).y;
    if (!wait_for(s, 30.0, [&s, top, y0](const Snapshot &) { return crane_body_at(s, top).y > y0 + 0.5; })) {
        record({"yard crane magnet load", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id,
                s.snapshot().death_count, "the slab never rose"});
        return;
    }
    // Stop and re-centre him on the slab's free end before the commit, as for
    // the hook: the load swings while it rises, and the commit's firm-footing
    // ray needs him standing on the slab, not slid onto the magnet. The kill
    // flight drives the hoist up again, so the rope still pays out after he
    // leaves.
    (void)s.set_yard_crane_hoist(0.0);
    const auto slab2 = crane_body_at(s, top);
    on_slab = false;
    for (const double off : {0.9, -0.9}) {
        s.debug_launch_player({slab2.x + off, slab2.y + 1.0, slab2.z}, {0.0, 0.0, 0.0});
        for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) && !on_slab; ++i) {
            tick(s);
            const auto st = s.snapshot();
            on_slab = st.player_grounded && st.support_entity_id == top;
        }
        if (on_slab) {
            break;
        }
    }
    if (!on_slab) {
        record({"yard crane magnet load", "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id,
                s.snapshot().death_count, "could not stand on the slab again after it rose"});
        return;
    }
    (void)s.advance_frame(1.0);
    (void)examine_floor(s, "yard crane magnet load", top,
                        "hoisting up on the magnet; the restore takes the rope length as it stands", crane_hoist_on);
}

// ---- Loose stock: a drum and a toolbox ---------------------------------------
// The lowest piece of the kind in the yard (closest to grade, so tipping off
// it is a step down, not a fall). Placed on by the test hook, as the exam does
// for every floor it cannot walk onto.
std::uint64_t lowest_loose(const Simulation &s, const scraperx::sim::LooseStock::Kind kind) {
    std::uint64_t found = 0;
    double lowest_y = 1.0e9;
    for (std::uint32_t i = 0; i < s.loose_stock_count(); ++i) {
        if (s.loose_stock_piece(i).kind != static_cast<std::uint8_t>(kind)) {
            continue;
        }
        const std::uint64_t entity = scraperx::sim::LooseStock::kFirstEntity + i;
        const double y = kit_at(s, entity).y;
        if (y < lowest_y) {
            lowest_y = y;
            found = entity;
        }
    }
    return found;
}

void exam_loose_piece(const char *name, const scraperx::sim::LooseStock::Kind kind, const char *note) {
    Simulation s(InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    const std::uint64_t piece = lowest_loose(s, kind);
    if (piece == 0) {
        record({name, "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "no piece of that kind in the loose stock"});
        return;
    }
    const auto at = kit_at(s, piece);
    s.debug_launch_player({at.x, at.y + 1.0, at.z}, {0.0, 0.0, 0.0});
    bool on_piece = false;
    for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) && !on_piece; ++i) {
        tick(s);
        const auto st = s.snapshot();
        on_piece = st.player_grounded && st.support_entity_id == piece;
    }
    if (!on_piece) {
        record({name, "SETUP-FAILED", 0.0, 0.0, s.snapshot().support_entity_id, s.snapshot().death_count,
                "could not stand on the piece"});
        return;
    }
    (void)examine_floor(s, name, piece, note);
}

void exam_loose_drum() {
    exam_loose_piece("loose drum", scraperx::sim::LooseStock::Kind::Drum,
                     "an empty drum on its end; it tips and slides under him, then the kit puts it back");
}

void exam_loose_toolbox() {
    exam_loose_piece("loose toolbox", scraperx::sim::LooseStock::Kind::Toolbox,
                     "an 18 kg toolbox; it slides under him, then the kit puts it back");
}

}  // namespace

void run_checkpoint_moving_floor() {
    g_rows.clear();
    exam_mast();
    exam_pitman();
    exam_helix();
    exam_crown();
    exam_derrick();
    exam_incline();
    exam_wheel();
    exam_tram();
    exam_well_ascent();
    exam_crane_j();
    exam_crane_k();
    exam_crane_l();
    exam_s1();
    exam_bay_leaf();
    exam_forklift_truck();
    exam_forklift_forks();
    exam_crane_hook();
    exam_crane_magnet();
    exam_loose_drum();
    exam_loose_toolbox();
    std::size_t support = 0, other = 0, safe = 0, bad = 0;
    for (const auto &row : g_rows) {
        if (row.verdict == "SUPPORT") {
            ++support;
        } else if (row.verdict == "OTHER") {
            ++other;
        } else if (row.verdict == "NO-LETHAL-EXIT") {
            ++safe;
        } else {
            ++bad;
        }
    }
    std::cout << "CHECKPOINT-MOVING-FLOOR " << g_rows.size() << " floors: " << support << " SUPPORT, " << other
              << " OTHER, " << safe << " NO-LETHAL-EXIT, " << bad << " failing or unmeasured\n";
    require(bad == 0, "checkpoint-moving-floor: every measured floor must come back on real support, never in the air "
                      "or inside steel, never in a death loop");
    std::cout << "PASS scraperx_sim checkpoint-moving-floor: " << support << " SUPPORT, " << other << " OTHER, " << safe
              << " NO-LETHAL-EXIT\n";
}
SCRAPERX_SLICE("checkpoint-moving-floor", run_checkpoint_moving_floor);
