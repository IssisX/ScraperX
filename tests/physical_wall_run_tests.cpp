#include "sim/simulation.hpp"
#include "sim/physical_foot_push.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace scraperx::sim;
namespace {
void require(bool ok, const char *why, const Simulation &sim) {
    if (ok) return;
    const auto s = sim.snapshot();
    std::cerr << "FAIL WALL_RUN " << why << " tick=" << s.tick_index
              << " p=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
              << " v=" << s.player_linear_velocity.x << ',' << s.player_linear_velocity.y
              << " plants=" << s.wall_stride_count << " active=" << s.foot_push_active
              << " reason=" << int(s.foot_push_stop_reason) << '\n';
    std::exit(EXIT_FAILURE);
}
Snapshot tick(Simulation &sim) {
    require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted,"ordinary fixed tick",sim);
    const auto s = sim.snapshot();
    require(s.death_count == 0 && s.player_gravity_factor == 1 && s.traversal_hand_constraint_count == 0,
            "ordinary gravity, survival and no fabricated hands",sim);
    require(s.wall_stride_count <= 2 && s.foot_push_peak_load_n <= PhysicalFootPush::force_bound_n()+.01 &&
            s.foot_push_command_work_bound_j <= PhysicalFootPush::command_budget_j()+.01 &&
            s.foot_push_stroke_m <= PhysicalFootPush::maximum_stroke_m()+.000001 &&
            s.foot_push_elapsed_seconds <= PhysicalFootPush::duration_seconds()+.000001,
            "two-start ceiling and unchanged per-plant physical bounds",sim);
    return s;
}
void approach(Simulation &sim, bool fast = false) {
    require(sim.debug_restart_at({fast ? -48.0 : -46.0,.9,-38.45}) && sim.set_move_input(0,0) &&
            sim.set_sprint_input(false) && sim.set_facing(1,0),"one initial actual yard staging",sim);
    for (unsigned i=0;i<30;++i) tick(sim);
    require(sim.snapshot().player_grounded && sim.snapshot().support_entity_id == 1,
            "actual grade before earned approach",sim);
    require(sim.set_move_input(1,0) && sim.set_sprint_input(true),"ordinary sprint",sim);
    bool running = false;
    for (unsigned i=0;i<180;++i) {
        const auto s = tick(sim);
        require(s.wall_stride_count == 0,"ground movement grants no wall plant",sim);
        if (s.player_grounded && s.player_linear_velocity.x > (fast ? 7.5 : 6.7)) { running = true; break; }
    }
    require(running && sim.set_move_input(.9682458,.25) && sim.request_jump(),
            "paid sprint-jump with deliberate along-wall and inward intent",sim);
    const auto s = tick(sim);
    require(!s.player_grounded && s.player_linear_velocity.y > 4,"actual takeoff",sim);
}
struct Episode {
    bool first = false, unloaded = false, second = false;
    std::uint64_t first_source = 0, unloaded_tick = 0, first_start = 0;
    double first_work = 0, second_work = 0, first_x = 0;
    void record(const Snapshot &s, const Simulation &sim) {
        if (s.wall_stride_count == 1) first_work = std::max(first_work,s.foot_push_command_work_bound_j);
        if (s.wall_stride_count == 2) second_work = std::max(second_work,s.foot_push_command_work_bound_j);
        require(first_work+second_work <= 2*PhysicalFootPush::command_budget_j()+.02,
                "combined work contains two original finite ledgers",sim);
        if (!first && s.wall_stride_count == 1 && s.foot_push_last_load_n > 0) {
            require(s.wall_stride_active && !s.wall_stride_pushing_off &&
                    s.foot_push_support_entity_id == Simulation::kWorldSolidEntityId &&
                    s.wall_stride_normal.z < -.9,"first real lateral wall plant",sim);
            first = true; first_source = s.foot_push_source_contact_tick;
            first_start = s.foot_push_start_count; first_x = s.player_position.x;
            std::cout << "WALL_FIRST tick=" << s.tick_index << " contact=" << first_source << '\n';
        }
        if (first && !second && s.wall_stride_count == 1 && !s.foot_push_active) {
            require(s.foot_push_stop_reason == unsigned(PhysicalFootPush::StopReason::Cleared),
                    "planned retirement is not a disguised traction failure",sim);
            if (!unloaded) { unloaded_tick = s.tick_index; unloaded = true; }
        }
        if (!second && s.wall_stride_count == 2 && s.foot_push_last_load_n > 0) {
            require(first && unloaded && s.foot_push_source_contact_tick >= unloaded_tick &&
                    s.foot_push_source_contact_tick > first_source &&
                    s.foot_push_start_count == first_start+1 &&
                    s.foot_push_support_entity_id == Simulation::kWorldSolidEntityId &&
                    s.wall_stride_normal.z < -.9 && s.player_position.x > first_x+.15,
                    "distinct later actual wall plant follows a solve with no first row",sim);
            second = true;
            std::cout << "WALL_SECOND tick=" << s.tick_index << " contact=" << s.foot_push_source_contact_tick
                      << " gap_tick=" << unloaded_tick << " load_n=" << s.foot_push_last_load_n << '\n';
        }
    }
    Snapshot step(Simulation &sim) { const auto s=tick(sim);record(s,sim);return s; }
    void wait(Simulation &sim, unsigned target) {
        for (unsigned i=0;i<180;++i) {
            step(sim);
            if ((target==1 && first) || (target==2 && unloaded) || (target==3 && second)) return;
        }
        require(false,"bounded actual wall-contact phase",sim);
    }
};
void land(Simulation &sim, Episode &episode, std::uint64_t count) {
    bool landed = false;
    for (unsigned i=0;i<180;++i) {
        const auto s=episode.step(sim);
        require(s.wall_stride_count == count,"held input cannot add an unauthorized plant",sim);
        if (s.player_grounded && s.support_entity_id == 1) { landed=true;break; }
    }
    require(landed && !sim.snapshot().foot_push_active,"actual grade receives finite wall departure",sim);
    const auto at=sim.snapshot();
    std::cout << "WALL_LAND tick=" << at.tick_index << " p=" << at.player_position.x << ','
              << at.player_position.y << ',' << at.player_position.z << " tangent_mps="
              << std::hypot(at.player_linear_velocity.x,at.player_linear_velocity.z) << '\n';
    require(sim.set_move_input(0,-1) && sim.set_facing(0,-1) && sim.set_sprint_input(false),
            "ordinary onward movement",sim);
    for (unsigned i=0;i<45;++i) tick(sim);
    require(sim.snapshot().player_grounded && sim.snapshot().support_entity_id == 1 &&
            sim.snapshot().player_position.z < at.player_position.z-.3,
            "actual onward ground play",sim);
}
} // namespace
int main(int argc,char **argv) {
    std::cout << std::unitbuf;
    const std::string mode=argc>1?argv[1]:"flow";
    Simulation sim;approach(sim,mode=="fast");Episode episode;
    if (mode=="flow" || mode=="fast" || mode=="jump") {
        episode.wait(sim,3);
        if (mode=="jump") {
            const auto before=sim.snapshot();
            require(sim.set_move_input(0,0) && sim.request_jump(),"deliberate wall-run push-off",sim);
            const auto s=episode.step(sim);
            require(s.wall_stride_count == 2 && s.foot_push_start_count == before.foot_push_start_count &&
                    s.foot_push_source_contact_tick == before.foot_push_source_contact_tick &&
                    s.foot_push_initial_distance_m == before.foot_push_initial_distance_m &&
                    s.foot_push_elapsed_seconds > before.foot_push_elapsed_seconds &&
                    s.foot_push_command_work_bound_j >= before.foot_push_command_work_bound_j,
                    "Jump spends the same second row without a replant or refill",sim);
        }
        land(sim,episode,2);
    } else if (mode=="cancel" || mode=="restart") {
        episode.wait(sim,2);
        if (mode=="restart") {
            require(sim.restart_checkpoint(),"retry during unloaded handoff",sim);
            for (unsigned i=0;i<30;++i) tick(sim);
            require(sim.snapshot().wall_stride_count == 0 && !sim.snapshot().foot_push_active &&
                    sim.snapshot().player_grounded,"retry clears pending second plant",sim);
        } else {
            // Drop always cancels pending Jump intent. Its return value says
            // whether a current attachment was released; this gap has none.
            require(!sim.request_release(),"unloaded handoff has no current attachment",sim);
            land(sim,episode,1);
        }
    } else if (mode=="reverse") {
        episode.wait(sim,1);
        require(sim.set_move_input(-1,0),"reverse during first plant",sim);
        land(sim,episode,1);
    } else require(false,"known wall-run mode",sim);
    std::cout << "WALL_RUN PASS mode=" << mode << " first_work_j=" << episode.first_work
              << " second_work_j=" << episode.second_work << '\n';
}
