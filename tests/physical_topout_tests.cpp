#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace scraperx::sim;

namespace {
void require(bool condition, const char *message, const Simulation &s) {
    if (condition) return;
    const auto v=s.snapshot();
    std::cerr<<"FAIL PHYSICAL_TOPOUT "<<message<<" tick="<<v.tick_index
        <<" position="<<v.player_position.x<<','<<v.player_position.y<<','<<v.player_position.z
        <<" hands="<<v.traversal_hand_constraint_count<<" foot="<<v.foot_push_active
        <<" phase="<<int(v.traversal_phase)<<" shared="<<v.hand_foot_overlap_solved_ticks
        <<" contact="<<v.foot_contact_tick<<" source_contact="<<v.foot_push_source_contact_tick
        <<" hand_n="<<v.actual_hand_load_n<<" foot_n="<<v.actual_foot_load_n<<'\n';
    std::exit(EXIT_FAILURE);
}
void tick(Simulation&s) {
    require(s.advance_frame(Simulation::kFixedStepSeconds).accepted,"native tick",s);
    require(s.snapshot().player_gravity_factor==1&&s.snapshot().death_count==0,
            "gravity stays active without hidden recovery",s);
}
void stage(Simulation&s) {
    require(s.debug_restart_at({26.68,87.92,-132}),"single existing production mantle approach",s);
    for(int i=0;i<45;++i)tick(s);
    require(s.snapshot().player_grounded,"actual approach footing",s);
    require(s.set_facing(-1,0)&&s.set_move_input(0,0),"ordinary facing and neutral input",s);
}
Snapshot topout() {
    Simulation s;
    stage(s);
    const auto before=s.snapshot();
    require(s.request_traversal(),"ordered Action",s);tick(s);
    require(s.snapshot().traversal_plan_kind==1&&s.snapshot().traversal_plan_generation>0&&
            s.snapshot().traversal_hand_constraint_count==2&&!s.snapshot().foot_push_active,
            "validated plan starts with real hands and no fabricated foot",s);
    bool shared=false,positive_loads=false,preserved_foot=false;
    int elapsed=1;
    for(;elapsed<153;++elapsed) {
        tick(s);const auto v=s.snapshot();
        if(v.foot_push_active) {
            require(v.foot_push_source_contact_tick>before.foot_contact_tick,
                    "high foot uses a new solved contact after the Action",s);
            require(v.foot_push_support_entity_id==Simulation::kTowerEntityId,
                    "real foot matches accepted receiving body",s);
            require(v.foot_push_peak_load_n<=3500.01&&v.foot_push_stroke_m<=.300001&&
                    v.foot_push_command_work_bound_j<=1285.62501,
                    "existing finite foot limits",s);
        }
        if(v.hand_foot_last_solve_tick>before.hand_foot_last_solve_tick) {
            shared=true;
            positive_loads|=v.hand_foot_last_hand_load_n>0&&v.hand_foot_last_foot_load_n>0;
            require(v.hand_foot_overlap_solved_ticks>before.hand_foot_overlap_solved_ticks,
                    "shared state is an actual completed solve",s);
            const double total=v.actual_hand_load_n+v.actual_foot_load_n;
            if(total>0)require(std::abs(v.actual_hand_load_fraction+v.actual_foot_load_fraction-1)<1e-9,
                              "observed load fractions normalize actual forces",s);
            std::cout<<"SHARED tick="<<v.hand_foot_last_solve_tick<<" contact="<<v.foot_push_source_contact_tick
                <<" hand_n="<<v.hand_foot_last_hand_load_n<<" foot_n="<<v.hand_foot_last_foot_load_n<<'\n';
        }
        if(v.traversal_hand_constraint_count==0&&v.accepted_traversal_count>before.accepted_traversal_count) {
            preserved_foot=v.foot_push_active;
            require(v.player_grounded&&v.support_entity_id==Simulation::kTowerEntityId&&v.player_position.y>88.7,
                    "hands release only at the intended real footing",s);
            break;
        }
    }
    require(elapsed<153&&shared&&positive_loads&&preserved_foot,
            "responsive physical hand-foot transfer preserves valid finite foot",s);
    for(int i=0;i<90;++i)tick(s);
    require(s.snapshot().checkpoint_footing_valid&&!s.snapshot().foot_push_active&&
            s.snapshot().traversal_plan_kind==0&&s.snapshot().player_grounded,
            "finite foot expires normally and actual exit remains firm",s);
    std::cout<<"PASS PHYSICAL_TOPOUT public_inputs=1 shared_positive_loads=1 preserved_foot=1 elapsed="
        <<elapsed<<" final_tick="<<s.snapshot().tick_index<<'\n';
    return s.snapshot();
}
void release_or_no_candidate(bool release) {
    Simulation s;stage(s);
    if(!release)require(s.set_facing(1,0),"face away from the ledge",s);
    require(s.request_traversal(),"ordinary Action request",s);tick(s);
    if(release) {
        for(int i=0;i<8;++i)tick(s);
        require(s.snapshot().traversal_hand_constraint_count==2&&!s.snapshot().foot_push_active,
                "hands carry the body before receiver contact",s);
        require(s.request_release(),"deliberate release",s);tick(s);
    }
    require(s.snapshot().traversal_hand_constraint_count==0&&!s.snapshot().foot_push_active&&
            s.snapshot().traversal_plan_kind==0,
            "missing candidate or deliberate release leaves no transient support",s);
    std::cout<<"PASS PHYSICAL_TOPOUT "<<(release?"release":"no_candidate")<<'\n';
}
void blocked() {
    Simulation s(InitialSpawn::BlockedLedgeApproach,WorldContent::RegressionFixtures);
    require(s.set_facing(-1,0),"face actual canopy-blocked ledge",s);
    for(int i=0;i<90;++i)tick(s);
    const auto before=s.snapshot();
    require(before.player_grounded&&!before.ledge_available,"actual blocker rejects standing receiver clearance",s);
    require(s.request_traversal(),"blocked Action is queued",s);tick(s);
    const auto v=s.snapshot();
    require(v.traversal_hand_constraint_count==0&&!v.foot_push_active&&v.traversal_plan_kind==0&&
            v.rejected_traversal_count>before.rejected_traversal_count&&
            v.player_position.y<before.player_position.y+.05,
            "obstruction cannot grant contact, completion, or upward travel",s);
    std::cout<<"PASS PHYSICAL_TOPOUT blocked_receiver no_legacy_action_started=1\n";
}
void restart() {
    // Exercise both reachable public boundaries: attached hands before contact,
    // and the retained finite foot immediately after the completed handoff.
    for(bool after_handoff:{false,true}) {
        Simulation s;stage(s);require(s.request_traversal(),"Action before restart",s);tick(s);
        if(after_handoff) {
            for(int i=0;i<153&&s.snapshot().accepted_traversal_count==0;++i)tick(s);
            require(s.snapshot().hand_foot_overlap_solved_ticks>0&&s.snapshot().foot_push_active&&
                    s.snapshot().traversal_hand_constraint_count==0,
                    "restart after a real shared solve while its finite foot remains",s);
        } else require(s.snapshot().traversal_plan_kind==1&&s.snapshot().traversal_hand_constraint_count==2,
                       "restart with real attached hands and a latched plan",s);
        require(s.restart_checkpoint(),"ordinary checkpoint restart",s);
        const auto v=s.snapshot();
        require(v.traversal_plan_kind==0&&v.traversal_hand_constraint_count==0&&!v.foot_push_active&&
                v.foot_contact_tick==0&&v.foot_push_source_contact_tick==0&&v.hand_foot_last_solve_tick==0&&
                v.hand_foot_last_hand_load_n==0&&v.hand_foot_last_foot_load_n==0,
                "restart clears transient plan, constraints and contact receipts",s);
        for(int i=0;i<10;++i)tick(s);
        require(!s.snapshot().hand_foot_overlap_active,"restart never revives old shared support",s);
    }
    std::cout<<"PASS PHYSICAL_TOPOUT restart\n";
}
Snapshot pattern(double frame_dt) {
    Simulation s;stage(s);require(s.request_traversal(),"same ordered Action for frame delivery",s);
    for(int frame=0;frame<5000&&s.snapshot().tick_index<270;++frame) {
        const double remaining=(270-s.snapshot().tick_index)*Simulation::kFixedStepSeconds;
        require(s.advance_frame(std::min(frame_dt,remaining)).accepted,"bounded caller frame",s);
        require(s.snapshot().player_gravity_factor==1&&s.snapshot().death_count==0,
                "caller pattern cannot change native gravity or failure",s);
    }
    const auto v=s.snapshot();
    require(v.tick_index==270&&v.accepted_traversal_count==1&&v.hand_foot_overlap_solved_ticks>0&&
            v.checkpoint_footing_valid&&v.traversal_hand_constraint_count==0&&!v.foot_push_active,
            "every delivery reaches same native transfer and stable exit",s);
    return v;
}
void frame_equivalence() {
    const auto fixed=pattern(Simulation::kFixedStepSeconds);
    for(double dt:{1.0/60,1.0/360,4.0/90}) {
        const auto other=pattern(dt);
        const auto close=[](Vector3 a,Vector3 b){return std::abs(a.x-b.x)<1e-4&&
            std::abs(a.y-b.y)<1e-4&&std::abs(a.z-b.z)<1e-4;};
        if(!close(fixed.player_position,other.player_position)||
           !close(fixed.player_linear_velocity,other.player_linear_velocity)||
           fixed.hand_foot_overlap_solved_ticks!=other.hand_foot_overlap_solved_ticks||
           fixed.accepted_traversal_count!=other.accepted_traversal_count) {
            std::cerr<<"FAIL PHYSICAL_TOPOUT frame equivalence dt="<<dt<<'\n';std::exit(EXIT_FAILURE);
        }
    }
    std::cout<<"PASS PHYSICAL_TOPOUT frame_equivalence fixed90_caller60_360_grouped4=1 tolerance_m=0.0001\n";
}
}

int main(int argc,char**argv) {
    const std::string mode=argc>1?argv[1]:"topout";
    if(mode=="release")release_or_no_candidate(true);
    else if(mode=="no-candidate")release_or_no_candidate(false);
    else if(mode=="blocked")blocked();
    else if(mode=="restart")restart();
    else if(mode=="frames")frame_equivalence();
    else (void)topout();
}
