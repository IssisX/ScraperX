#include "sim/parkour_route.hpp"
#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;
using scraperx::sim::TraversalState;
using scraperx::sim::kTaperInspectionEntity;
using scraperx::sim::kTaperRecoveryEntity;

double speed(const Snapshot &v) {
    return std::hypot(v.player_linear_velocity.x, v.player_linear_velocity.z);
}

void report(const char *label, const Simulation &s) {
    const auto v = s.snapshot();
    std::cout << label << " tick=" << v.tick_index << " P="
              << v.player_position.x << ',' << v.player_position.y << ',' << v.player_position.z
              << " V=" << v.player_linear_velocity.x << ',' << v.player_linear_velocity.y
              << ',' << v.player_linear_velocity.z << " support=" << v.support_entity_id
              << " grounded=" << v.player_grounded << " traversal=" << int(v.traversal_state)
              << " hands=" << v.traversal_hand_constraint_count
              << " gravity=" << v.player_gravity_factor << " crouch=" << v.player_crouched
              << " balance=" << v.player_balancing << " deaths=" << v.death_count
              << " landings=" << v.landing_count << " impact=" << v.last_impact_speed_mps
              << " work=" << v.landing_recovery_work_j
              << " jump_work=" << v.landing_jump_work_j << '\n';
}

void require(bool condition, const char *why, const Simulation &s) {
    if (condition) return;
    report(why, s);
    std::cerr << "FAIL TAPER_INSPECTION " << why << '\n';
    std::exit(EXIT_FAILURE);
}

void tick(Simulation &s, int count = 1) {
    while (count-- > 0) {
        const auto before = s.snapshot();
        const auto result = s.advance_frame(Simulation::kFixedStepSeconds);
        require(result.accepted && result.steps_advanced == 1,
                "exactly one native90Hz step accepted", s);
        const auto v = s.snapshot();
        require(v.death_count == 0 && v.player_gravity_factor == 1.0 &&
                    v.traversal_state == TraversalState::None &&
                    v.traversal_hand_constraint_count == 0 && v.jump_vault_count == 0 &&
                    !v.parachute_deployed,
                "ordinary route retains gravity without traversal, vault, chute or rescue", s);
        require(std::isfinite(v.player_position.x) && std::isfinite(v.player_position.y) &&
                    std::isfinite(v.player_position.z) &&
                    std::isfinite(v.player_linear_velocity.x) &&
                    std::isfinite(v.player_linear_velocity.y) &&
                    std::isfinite(v.player_linear_velocity.z) &&
                    std::isfinite(v.landing_recovery_work_j) &&
                    std::isfinite(v.landing_jump_work_j) &&
                    v.landing_recovery_work_j >= before.landing_recovery_work_j - .000001 &&
                    v.landing_jump_work_j >= before.landing_jump_work_j - .000001,
                "actual velocity and native contact/push-off work remain finite", s);
    }
}

void steer(Simulation &s, double x, double z, double effort_x = 0, double effort_z = 0) {
    const auto v = s.snapshot();
    const double dx = x - v.player_position.x, dz = z - v.player_position.z;
    const double input_x = dx * 1.8 + effort_x - v.player_linear_velocity.x * .28;
    const double input_z = dz * 1.8 + effort_z - v.player_linear_velocity.z * .28;
    const double scale = std::max(1.0, std::hypot(input_x, input_z));
    require(s.set_move_input(input_x / scale, input_z / scale),
            "velocity-aware ordinary movement input accepted", s);
    if (std::hypot(dx, dz) > .02)
        require(s.set_facing(dx, dz), "ordinary facing input accepted", s);
}

bool walk_to(Simulation &s, double x, double z, int budget = 1800,
             int *incline_contact_ticks = nullptr) {
    double effort_x = 0, effort_z = 0;
    for (int i = 0; i < budget; ++i) {
        const auto v = s.snapshot();
        if (v.player_grounded && std::hypot(x - v.player_position.x, z - v.player_position.z) < .08 &&
            speed(v) < .12) {
            require(s.set_move_input(0, 0), "ordinary arrival releases movement", s);
            return true;
        }
        // Sustained deliberate stick effort can finish an uphill crouched
        // approach; proportional easing alone stalls against real loading.
        // This changes only ordinary input, never contact or arrival criteria.
        const double dx = x - v.player_position.x, dz = z - v.player_position.z;
        if (std::hypot(dx, dz) > .75) effort_x = effort_z = 0;
        else {
            effort_x += dx * Simulation::kFixedStepSeconds * .4;
            effort_z += dz * Simulation::kFixedStepSeconds * .4;
            const double scale = std::max(1.0, std::hypot(effort_x, effort_z) / .35);
            effort_x /= scale;
            effort_z /= scale;
        }
        steer(s, x, z, effort_x, effort_z);
        tick(s);
        if (incline_contact_ticks) {
            const auto contact = s.snapshot();
            if (contact.player_grounded && contact.support_entity_id == kTaperInspectionEntity)
                ++*incline_contact_ticks;
        }
    }
    require(s.set_move_input(0, 0), "bounded approach releases movement", s);
    return false;
}

void walk(Simulation &s, double x, double z, const char *why, int budget = 1800,
          int *incline_contact_ticks = nullptr) {
    const bool reached = walk_to(s, x, z, budget, incline_contact_ticks);
    require(reached, why, s);
}

void stable(Simulation &s, std::uint64_t support, double standing_y, const char *why) {
    require(s.set_move_input(0, 0), "stable arrival releases movement", s);
    for (int i = 0; i < 90; ++i) {
        tick(s);
        const auto v = s.snapshot();
        require(v.player_grounded && v.support_entity_id == support &&
                    !v.player_crouched && std::abs(v.player_position.y - standing_y) < .12 &&
                    std::abs(v.player_linear_velocity.y) < .12 && speed(v) < .18,
                why, s);
    }
    report(why, s);
}

void stage_cart_exit(Simulation &s) {
    // Sole staging operation for each independent normal-world campaign.
    // This is the previously measured supported ordinary cart exit, not a
    // custom spawn or fixture. Every later movement comes from normal inputs.
    require(s.debug_restart_at({-25.4203, 352.9, -141.599}),
            "real cart-exit supported staging accepted", s);
    require(s.set_move_input(0, 0) && s.set_sprint_input(false) && s.set_crouch_input(false),
            "ordinary standing input policy accepted", s);
    tick(s, 180);
    stable(s, Simulation::kTowerEntityId, 352.9, "CART_EXIT352");
}

void reach_a(Simulation &s) {
    // A first flat-ring sample can reject an instantaneous velocity assignment
    // before geometry, slopes or landing impulses complicate the force bound.
    const auto before = s.snapshot();
    require(s.set_move_input(1, 0) && s.set_facing(1, 0), "ring force input accepted", s);
    tick(s);
    const auto pushed = s.snapshot();
    const double delta_v = std::hypot(pushed.player_linear_velocity.x - before.player_linear_velocity.x,
                                    pushed.player_linear_velocity.z - before.player_linear_velocity.z);
    const double work = pushed.landing_recovery_work_j - before.landing_recovery_work_j;
    require(pushed.player_grounded && pushed.support_entity_id == Simulation::kTowerEntityId &&
                delta_v > .001 && delta_v <= .85 * 9.81 * Simulation::kFixedStepSeconds + .001 &&
                work > 0 && work <= 3000.1 * Simulation::kFixedStepSeconds,
            "flat ring acceleration earns finite0.85g/3kW contact work", s);
    walk(s, -24.12, -138.6, "ring reaches south-side girder approach");
    walk(s, -24.0, -139.5, "ordinary standing walk reaches lower girder toe");
    require(s.snapshot().player_grounded && s.snapshot().player_position.y < 353.3,
            "lower girder entry starts from physical352 footing", s);
    report("LOWER_GIRDER_TOE", s);
    int incline_contacts = 0;
    walk(s, -24.0, -164.0, "lower inclined girder reaches landingA363", 2700, &incline_contacts);
    stable(s, kTaperInspectionEntity, 363.9, "LANDING_A363");
    require(incline_contacts > 180 && s.snapshot().landing_recovery_work_j > pushed.landing_recovery_work_j,
            "lower ascent uses real narrow support and positive contact work", s);
}

void gap_jump(Simulation &s) {
    walk(s, -25.9, -164.0, "landingA provides physical jump run-up");
    require(s.set_facing(1, 0) && s.set_move_input(1, 0), "ordinary gap run-up input accepted", s);
    for (int i = 0; i < 120 && s.snapshot().player_position.x < -23.95; ++i) tick(s);
    const auto launch = s.snapshot();
    report("GAP_TAKEOFF", s);
    require(launch.player_grounded && launch.support_entity_id == kTaperInspectionEntity &&
                launch.player_position.x >= -24.02 && launch.player_position.x < -23.7 &&
                launch.player_linear_velocity.x > 2.0,
            "gap takeoff is a real supported running stance before the edge", s);
    require(s.request_jump(), "single ordinary gap Jump accepted", s);
    bool airborne = false, unsupported_middle = false, landed = false;
    double peak = launch.player_position.y;
    for (int i = 0; i < 540; ++i) {
        const auto before = s.snapshot();
        steer(s, -20.15, -164.0);
        tick(s);
        const auto v = s.snapshot();
        peak = std::max(peak, v.player_position.y);
        airborne = airborne || (!v.player_grounded && v.support_entity_id == 0 &&
                                v.player_linear_velocity.y > .5);
        if (v.player_position.x > -23.0 && v.player_position.x < -21.8 &&
            v.player_position.y > 364.1 && !before.player_grounded && !v.player_grounded) {
            unsupported_middle = true;
            require(v.support_entity_id == 0 &&
                        v.player_linear_velocity.y <= before.player_linear_velocity.y + .01,
                    "unsupported gap flight loses vertical speed under native gravity", s);
        }
        if (airborne && v.player_grounded && v.support_entity_id == kTaperInspectionEntity &&
            v.player_position.x > -21.1 && std::abs(v.player_position.y - 363.9) < .12) {
            landed = true;
            break;
        }
    }
    require(airborne && unsupported_middle && landed && peak > launch.player_position.y + .8,
            "single Jump crosses the real2m unsupported gap onto landingB", s);
    const double jump_work = s.snapshot().landing_jump_work_j - launch.landing_jump_work_j;
    require(jump_work > 0 && jump_work <= .5 * 85.0 * 5.5 * 5.5 + .1,
            "gap Jump is earned by finite85kg push-off work", s);
    walk(s, -20.15, -164.0, "landingB permits ordinary braking after gap Jump");
    stable(s, kTaperInspectionEntity, 363.9, "LANDING_B363");
}

void finish_upper(Simulation &s, bool from_rejoin = false) {
    if (!from_rejoin) walk(s, -20.35, -164.0, "landingB aligns with upper girder");
    int incline_contacts = 0;
    walk(s, -20.35, -139.05, "upper girder reaches actual turning platform374", 2700, &incline_contacts);
    stable(s, kTaperInspectionEntity, 374.9, "UPPER_TURN374");
    require(incline_contacts > 180, "upper ascent retains actual inclined support contact", s);
    walk(s, -20.35, -139.5, "turning platform aligns with final return beam");
    walk(s, -22.5, -139.5, "return beam steps onto existing WorldSolid51 floor374.25");
    stable(s, Simulation::kWorldSolidEntityId, 375.15, "EXISTING_FLOOR374_25");
    const auto arrival = s.snapshot();
    walk(s, -22.5, -143.0, "existing upper floor permits ordinary onward walking");
    stable(s, Simulation::kWorldSolidEntityId, 375.15, "ONWARD_UPPER_FLOOR");
    require(std::hypot(s.snapshot().player_position.x - arrival.player_position.x,
                       s.snapshot().player_position.z - arrival.player_position.z) > 3.0,
            "upper arrival remains playable beyond first contact", s);
}

void fall_to(Simulation &s, double x, double z, double standing_y, const char *why) {
    const auto entry = s.snapshot();
    bool unsupported = false, descending = false, caught = false;
    for (int i = 0; i < 900; ++i) {
        steer(s, x, z);
        tick(s);
        const auto v = s.snapshot();
        unsupported = unsupported || (!v.player_grounded && v.support_entity_id == 0);
        descending = descending || (!v.player_grounded && v.player_linear_velocity.y < -3.0);
        if (unsupported && v.player_grounded && v.support_entity_id == kTaperRecoveryEntity &&
            std::abs(v.player_position.y - standing_y) < .15) {
            caught = true;
            break;
        }
    }
    require(unsupported && descending && caught &&
                s.snapshot().landing_count > entry.landing_count &&
                s.snapshot().landing_support_entity_id == kTaperRecoveryEntity &&
                s.snapshot().player_position.y < entry.player_position.y - 2.0,
            why, s);
    walk(s, x, z, "recovery deck permits ordinary finite braking");
    tick(s, 120);
    stable(s, kTaperRecoveryEntity, standing_y, why);
}

void middle_miss_service_recovery() {
    Simulation s;
    stage_cart_exit(s);
    reach_a(s);
    const double jump_work = s.snapshot().landing_jump_work_j;
    fall_to(s, -22.4, -164.0, 361.4, "MIDDLE_MISS_REAL_APRON360_5");
    require(s.snapshot().landing_jump_work_j == jump_work,
            "walking miss reaches lower apron without a jump or fabricated gap support", s);
    // Keep clear of the bearing's two posts until aligned with the service
    // girder's south toe. Header clearance is then tested on the actual lane.
    walk(s, -22.4, -157.7, "recovery apron reaches south service approach");
    walk(s, -19.55, -157.7, "ordinary recovery aligns with service girder toe");
    walk(s, -19.55, -157.55, "apron joins actual service girder360.5 toe");
    const bool walked_through = walk_to(s, -19.55, -160.2, 270);
    report("SERVICE_HEADER_STANDING_BLOCKED", s);
    require(!walked_through && !s.snapshot().player_crouched && s.snapshot().player_grounded &&
                s.snapshot().player_position.z > -159.5 && s.snapshot().player_position.z < -157.6,
            "real bearing header blocks the ordinary standing capsule", s);
    require(s.set_crouch_input(true), "ordinary crouch input accepted", s);
    tick(s, 30);
    walk(s, -19.55, -160.2, "short capsule passes underneath real service header");
    require(s.snapshot().player_crouched && s.snapshot().player_grounded &&
                s.snapshot().support_entity_id == kTaperInspectionEntity,
            "crouched passage keeps actual inclined service support", s);
    report("SERVICE_HEADER_CROUCHED_PASS", s);
    require(s.set_crouch_input(false), "ordinary stand input accepted beyond header", s);
    tick(s, 30);
    require(!s.snapshot().player_crouched && s.snapshot().player_position.z < -159.8,
            "standing clearance returns beyond the physical header", s);
    walk(s, -19.55, -164.0, "longer service path rejoins landingB through actual girder");
    stable(s, kTaperInspectionEntity, 363.9, "SERVICE_REJOIN_LANDING_B");
    finish_upper(s);
}

void downhill_jump_positive_work() {
    Simulation s;
    // Supported staging on the real static lower girder, not a velocity write.
    require(s.debug_restart_at({-24, 358.6, -151.25}),
            "downhill jump starts above the real taper girder", s);
    tick(s, 180);
    require(s.snapshot().player_grounded &&
                s.snapshot().support_entity_id == kTaperInspectionEntity,
            "downhill jump has actual girder footing", s);
    require(s.set_move_input(0, 1) && s.set_facing(0, 1),
            "ordinary stick requests downhill movement", s);
    tick(s, 60);
    const auto before = s.snapshot();
    require(before.player_grounded && before.support_entity_id == kTaperInspectionEntity &&
                before.player_linear_velocity.y < -.5 &&
                std::hypot(before.support_point_linear_velocity.x,
                    before.support_point_linear_velocity.z) < .000001 &&
                std::abs(before.support_point_linear_velocity.y) < .000001,
            "real downhill contact retains descending velocity on static support", s);
    require(s.set_move_input(0, 0) && s.request_jump(),
            "single supported Jump with neutral air steering", s);
    tick(s);
    const auto after = s.snapshot();
    require(!after.player_grounded && after.player_linear_velocity.y > 5.3 &&
                std::hypot(after.player_linear_velocity.x - before.player_linear_velocity.x,
                    after.player_linear_velocity.z - before.player_linear_velocity.z) < .002,
            "downhill Jump preserves horizontal momentum and earns ordinary free flight", s);
    // Only gravity follows the isolated push on this static support. Positive
    // work after reversal is the upward kinetic energy, not its signed change
    // from the previous downward energy. The 0.05J band covers float stepping.
    const double takeoff_y = after.player_linear_velocity.y +
        double(9.81F * float(Simulation::kFixedStepSeconds));
    const double positive_work = .5 * 85.0 * takeoff_y * takeoff_y;
    const double receipt = after.landing_jump_work_j - before.landing_jump_work_j;
    require(std::abs(takeoff_y - 5.5) < .002 &&
                std::abs(receipt - positive_work) < .05 &&
                receipt > 0 && receipt <= .5 * 85.0 * 5.5 * 5.5 + .05,
            "downward braking cannot subsidize the upward push-off receipt", s);
    std::cout << "PASS TAPER_INSPECTION downhill_jump before_vy="
              << before.player_linear_velocity.y << " takeoff_vy=" << takeoff_y
              << " positive_work_j=" << positive_work << " receipt_j=" << receipt << '\n';
}

void upper_miss_catch_recovery() {
    Simulation s;
    stage_cart_exit(s);
    reach_a(s);
    gap_jump(s);
    walk(s, -20.35, -164.0, "upper miss aligns with main girder");
    walk(s, -20.35, -144.0, "upper miss reaches exposed girder above catch deck", 2700);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == kTaperInspectionEntity &&
                s.snapshot().player_position.y > 372.5,
            "upper miss starts on real elevated girder support", s);
    fall_to(s, -18.5, -144.0, 367.4, "UPPER_MISS_REAL_CATCH366_5");
    walk(s, -23.15, -144.0, "catch deck permits return towards tongue");
    walk(s, -23.15, -155.4, "catch deck connects to narrow supported return tongue");
    walk(s, -20.35, -155.4, "tongue crossbeam physically rejoins upper girder");
    require(s.snapshot().player_grounded &&
                (s.snapshot().support_entity_id == kTaperInspectionEntity ||
                 s.snapshot().support_entity_id == kTaperRecoveryEntity) &&
                std::abs(s.snapshot().player_position.y - 367.46) < .15,
            "upper recovery returns at actual matching girder/crossbeam height", s);
    report("UPPER_RECOVERY_REAL_REJOIN", s);
    finish_upper(s, true);
}
} // namespace

int main(int argc, char **argv) {
    const std::string mode = argc == 2 ? argv[1] : "all";
    if (mode == "all" || mode == "jump-work") downhill_jump_positive_work();
    if (mode == "jump-work") {
        Simulation s;
        require(s.debug_restart_at({-25.9, 363.9, -164}),
                "related gap regression stages only on actual landingA", s);
        tick(s, 180);
        gap_jump(s); // Existing run-up, unsupported flight and receiving-footing gate.
        std::cout << "PASS TAPER_INSPECTION related_actual_gap_jump\n";
    }
    if (mode == "all" || mode == "primary") {
        Simulation s;
        stage_cart_exit(s);
        reach_a(s);
        gap_jump(s);
        finish_upper(s);
        std::cout << "PASS TAPER_INSPECTION primary352_to_existing374.25 ordinary_inputs=1\n";
    }
    if (mode == "all" || mode == "middle") {
        middle_miss_service_recovery();
        std::cout << "PASS TAPER_INSPECTION middle_miss_apron_crouched_service_rejoin\n";
    }
    if (mode == "all" || mode == "upper") {
        upper_miss_catch_recovery();
        std::cout << "PASS TAPER_INSPECTION upper_miss_catch_tongue_rejoin\n";
    }
    if (mode != "all" && mode != "primary" && mode != "middle" && mode != "upper" && mode != "jump-work") {
        std::cerr << "FAIL TAPER_INSPECTION unknown mode\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
