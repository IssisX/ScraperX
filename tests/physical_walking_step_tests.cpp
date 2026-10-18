#include "sim/simulation.hpp"
#include "sim/grapple_practice_route.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>
using namespace scraperx::sim;
namespace {
void require(bool value,const char *message,const Simulation &s) {
    if (value) return;
    const auto p=s.snapshot();
    std::cerr<<"FAIL WALKING_STEP "<<message<<" tick="<<p.tick_index
        <<" position="<<p.player_position.x<<','<<p.player_position.y<<','<<p.player_position.z
        <<" velocity="<<p.player_linear_velocity.x<<','<<p.player_linear_velocity.y
        <<" steps="<<p.step_up_count<<" foot="<<p.foot_push_active<<'\n';
    std::exit(EXIT_FAILURE);
}
void tick(Simulation &s) {
    require(s.advance_frame(Simulation::kFixedStepSeconds).accepted,"public native tick",s);
    require(s.snapshot().death_count==0&&s.snapshot().player_gravity_factor==1&&
        s.snapshot().step_up_pose_count==0&&s.snapshot().traversal_hand_constraint_count==0,
        "ordinary stairs retain gravity, collision and no pose/hand assist",s);
}
void require_unpowered(const Simulation &s) {
    const auto p=s.snapshot();
    require(!p.foot_push_active&&p.foot_push_last_load_n==0&&p.foot_push_last_impulse_ns==0&&
        p.foot_push_player_force.x==0&&p.foot_push_player_force.y==0&&p.foot_push_player_force.z==0,
        "cancelled walking leg leaves no active row, force or impulse",s);
}
void interruption_case(std::string_view mode) {
    Simulation s;
    for (int i=0;i<45;++i) tick(s);
    const auto body=s.kit_body_index(kGrapplePracticeRecoveryEntity);
    const auto base=s.kit_body_position(body);
    const Vector3 toe{base.x-1.5,base.y+.95,base.z-1.85};
    require(s.debug_restart_at(toe),"interruption starts at real recovery stair toe",s);
    require(s.set_facing(-1,0)&&s.set_move_input(0,0),"settle interruption fixture",s);
    for (int i=0;i<20;++i) tick(s);
    require(s.snapshot().player_grounded&&std::abs(s.snapshot().player_linear_velocity.y)<.01,
        "interruption starts from stable actual floor contact",s);
    const auto settled_tick=s.snapshot().tick_index;
    require(s.set_move_input(-.5,0),"approach stairs with ordinary walking input",s);
    for (int i=0;i<240;++i) {
        const auto p=s.snapshot();
        if (p.foot_push_active&&p.foot_push_last_load_n>0) break;
        tick(s);
    }
    const auto active=s.snapshot();
    require(active.foot_push_active&&active.foot_push_support_entity_id==kGrapplePracticeRecoveryEntity&&
        active.foot_push_source_contact_tick>=settled_tick&&active.foot_push_last_load_n>0,
        "interruption exercises a powered stroke from fresh real stair contact",s);
    if (mode=="restart") {
        require(s.debug_restart_at(toe),"debug restart during real walking stroke",s);
        require_unpowered(s);
        require(s.snapshot().foot_push_source_contact_tick==0&&s.snapshot().foot_contact_tick==0&&
            !s.snapshot().hand_foot_overlap_active,
            "restart clears transient foot contact and overlap state",s);
    } else if (mode=="cancel") {
        require(s.request_release(),"explicit release cancels walking stroke",s);
        tick(s);
        require_unpowered(s);
        require(s.snapshot().foot_push_start_count==active.foot_push_start_count,
            "release cannot reacquire the cancelled walking leg in the same tick",s);
        require(s.set_move_input(0,0),"neutral after explicit cancellation",s);
    } else {
        require(s.set_move_input(mode=="reverse"?.5:0,0),"interrupt walking direction",s);
    }
    for (int i=0;i<20;++i) {
        tick(s);
        require_unpowered(s);
        require(s.snapshot().foot_push_start_count==active.foot_push_start_count,
            "interrupted walking cannot renew the stale stair stroke",s);
    }
    std::cout<<"PASS WALKING_STEP "<<mode<<" active_tick="<<active.tick_index
        <<" checked_ticks=20\n";
}
}
int main(int argc,char **argv) {
    const bool fast = argc == 2 && std::string_view(argv[1]) == "fast";
    if (argc>1 && !fast) {
        const std::string_view mode=argv[1];
        if (argc!=2||(mode!="neutral"&&mode!="cancel"&&mode!="reverse"&&mode!="restart")) {
            std::cerr<<"usage: physical_walking_step_tests [fast|neutral|cancel|reverse|restart]\n";
            return EXIT_FAILURE;
        }
        interruption_case(mode);
        return EXIT_SUCCESS;
    }
    Simulation s;
    for (int i=0;i<45;++i) tick(s);
    const auto body=s.kit_body_index(kGrapplePracticeRecoveryEntity);
    const auto base=s.kit_body_position(body);
    // One initial fixture relocation; all later movement is ordinary stick input.
    require(s.debug_restart_at({base.x-1.5,base.y+.95,base.z-1.85}),"real recovery stair toe",s);
    require(s.set_facing(-1,0)&&s.set_move_input(0,0),"settle at the real stair toe",s);
    for (int i=0;i<20;++i) tick(s);
    require(s.snapshot().player_grounded&&std::abs(s.snapshot().player_linear_velocity.y)<.01,
        "ordinary walking starts from stable actual floor contact",s);
    require(s.set_move_input(fast?-1:-.5,0),"ordinary forward walking",s);
    const auto before=s.snapshot();
    bool powered=false,received=false;
    unsigned elapsed=0,slow_ticks=0,longest_slow=0;
    double peak_load=0,peak_work=0;
    for (;elapsed<240;++elapsed) {
        if (fast) {
            const auto p=s.snapshot();
            // This authored flight ends at a sideways exit, not an endless
            // forward landing. Brake through ordinary stick input, then walk
            // onto its real departure deck without overwriting momentum.
            const double x=std::clamp(1.2*(base.x-4.75-p.player_position.x)-
                .45*p.player_linear_velocity.x,-1.0,1.0);
            const bool at_exit = p.player_position.x<base.x-4.4 &&
                p.player_position.y>base.y+3.25;
            const double z=at_exit ? std::clamp(1.2*(base.z-.7-p.player_position.z)-
                .45*p.player_linear_velocity.z,-.5,.5) : 0;
            require(s.set_move_input(x,z),"full-input approach and real sideways exit",s);
        }
        tick(s);const auto p=s.snapshot();
        if (p.foot_push_active) {
            powered=true;
            require(p.foot_push_support_entity_id==kGrapplePracticeRecoveryEntity&&
                p.foot_push_source_contact_tick>=before.tick_index,
                "walking leg starts on actual fresh material contact",s);
            require(p.foot_push_peak_load_n<=3500.01&&p.foot_push_command_work_bound_j<=1285.62501&&
                p.foot_push_stroke_m<=.300001&&p.foot_push_elapsed_seconds<=.250001,
                "walking retains the existing force work stroke and lifetime bounds",s);
            peak_load=std::max(peak_load,p.foot_push_peak_load_n);
            peak_work=std::max(peak_work,p.foot_push_command_work_bound_j);
        }
        if (elapsed>20&&p.player_position.x<base.x-1.9&&p.player_position.x>base.x-4.5) {
            if (-p.player_linear_velocity.x<.25) ++slow_ticks; else slow_ticks=0;
            longest_slow=std::max(longest_slow,slow_ticks);
        }
        if (p.player_position.x<base.x-4.5&&p.player_position.y>base.y+3.2&&
            p.player_grounded&&p.support_entity_id==(fast ? kGrapplePracticeDepartureEntity :
                kGrapplePracticeRecoveryEntity)) {
            received=true;break;
        }
    }
    // Ordinary Jolt contacts may climb a tread without a powered stroke.
    // Count successful physical handoffs rather than requiring one motor
    // acquisition per authored riser.
    require(powered&&received&&s.snapshot().step_up_count>=2,
        "successive real receiving contacts carry ordinary walking to the top tread",s);
    require(longest_slow<18,"no compulsory stop between consecutive treads",s);
    require(s.snapshot().landing_jump_work_j>before.landing_jump_work_j+1,
        "physical stair gain has positive finite leg work",s);
    std::cout<<"PASS WALKING_STEP input="<<(fast?1:.5)<<" ticks="<<elapsed+1<<" steps="<<s.snapshot().step_up_count
        <<" longest_slow_ticks="<<longest_slow<<" peak_force_n="<<peak_load
        <<" peak_stroke_work_j="<<peak_work<<'\n';
}
