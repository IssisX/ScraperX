#include "sim/simulation.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace scraperx::sim;
namespace {
void require(bool ok,const char*why,const Simulation&s) {
    if(ok)return;
    const auto v=s.snapshot();
    std::cerr<<"FAIL SHIMMY "<<why<<" tick="<<v.tick_index<<" pos="<<v.player_position.x<<','
        <<v.player_position.y<<','<<v.player_position.z<<" state="<<int(v.traversal_state)
        <<" hands="<<v.traversal_hand_constraint_count<<" generations="
        <<v.traversal_left_hand_generation<<','<<v.traversal_right_hand_generation
        <<" target="<<v.traversal_target_point.x<<','<<v.traversal_target_point.y<<','<<v.traversal_target_point.z
        <<" ledge="<<v.traversal_ledge_point.x<<','<<v.traversal_ledge_point.y<<','<<v.traversal_ledge_point.z
        <<" plan="<<int(v.traversal_plan_kind)<<" reject="<<int(v.traversal_rejection_reason)
        <<" phase="<<int(v.traversal_phase)<<" foot="<<v.foot_push_active<<" transfers="<<v.foot_transfer_count
        <<" command_work="<<v.traversal_command_work_bound_j
        <<" positive_work="<<v.traversal_actuator_positive_work_j<<'\n';
    std::exit(EXIT_FAILURE);
}
void tick(Simulation&s) {
    const auto before=s.snapshot().tick_index;
    require(s.advance_frame(Simulation::kFixedStepSeconds).accepted,"native advance accepted",s);
    const auto v=s.snapshot();
    require(v.tick_index==before+1,"exactly one native solve observed",s);
    require(v.player_gravity_factor==1&&v.death_count==0,"gravity on without recovery",s);
}
double distance(Vector3 a,Vector3 b) {return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);}
Vector3 local(const Simulation&s,Vector3 p) {
    const auto body=s.kit_body_index(1901);
    const auto c=s.kit_body_position(body);
    const auto q=s.kit_body_rotation(body);
    // Rotate by the conjugate support quaternion: material point, not pose-relative camera data.
    const Vector3 v{p.x-c.x,p.y-c.y,p.z-c.z};
    const Vector3 u{-q.x,-q.y,-q.z};
    const Vector3 cross{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
    const Vector3 twice{2*cross.x,2*cross.y,2*cross.z};
    return {v.x+q.w*twice.x+u.y*twice.z-u.z*twice.y,
            v.y+q.w*twice.y+u.z*twice.x-u.x*twice.z,
            v.z+q.w*twice.z+u.x*twice.y-u.y*twice.x};
}
void hanging(const Simulation&s) {
    const auto v=s.snapshot();
    require(v.traversal_state==TraversalState::Hanging&&v.traversal_hand_constraint_count==2&&
            v.traversal_support_entity_id==1901&&!v.foot_push_active,
            "real two-hand north hang without fabricated foot",s);
}
struct Observation {
    int both=0,left=0,right=0;std::uint64_t last_change=0;bool cadence=true,planted=true,trailing=true;
    void observe(const Snapshot&before,const Simulation&s,double direction) {
        hanging(s);const auto after=s.snapshot();
        const bool l=before.traversal_left_hand_generation!=after.traversal_left_hand_generation;
        const bool r=before.traversal_right_hand_generation!=after.traversal_right_hand_generation;
        if(!l)planted&=distance(local(s,before.traversal_left_hand),local(s,after.traversal_left_hand))<1e-5;
        if(!r)planted&=distance(local(s,before.traversal_right_hand),local(s,after.traversal_right_hand))<1e-5;
        if(l&&r)++both;
        if(l||r) {
            if(last_change)cadence&=after.tick_index-last_change>=18;
            last_change=after.tick_index;
            // Travel is along world Z on this west-facing north shelf.
            if(l&&!r)trailing&=direction*(before.traversal_left_hand.z-before.traversal_right_hand.z)<=1e-5;
            if(r&&!l)trailing&=direction*(before.traversal_right_hand.z-before.traversal_left_hand.z)<=1e-5;
            left+=l;right+=r;
            std::cout<<"REGRIP tick="<<after.tick_index<<" changed="<<l<<','<<r
                <<" hand_z="<<after.traversal_left_hand.z<<','<<after.traversal_right_hand.z<<'\n';
        }
    }
};
void neutral(Simulation&s,int ticks) {
    require(s.set_move_input(0,0),"neutral input",s);const auto before=s.snapshot();
    for(int i=0;i<ticks;++i) {
        tick(s);hanging(s);const auto v=s.snapshot();
        require(v.traversal_left_hand_generation==before.traversal_left_hand_generation&&
                v.traversal_right_hand_generation==before.traversal_right_hand_generation&&
                distance(local(s,v.traversal_left_hand),local(s,before.traversal_left_hand))<1e-5&&
                distance(local(s,v.traversal_right_hand),local(s,before.traversal_right_hand))<1e-5,
                "neutral preserves both planted material anchors and identities",s);
    }
}
}
int main(int argc,char**argv) {
    const bool baseline=argc>1&&std::string(argv[1])=="baseline";
    Simulation s;
    // The only staging call puts the body on the existing +103.5 m approach shelf.
    require(s.debug_restart_at({15.1,104.4,-180.6}),"single production approach staging",s);
    for(int i=0;i<45;++i)tick(s);
    require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==1901&&
            std::abs(s.snapshot().player_position.y-104.4)<.12,"actual approach shelf footing",s);
    require(s.set_facing(-1,0)&&s.request_jump()&&s.set_move_input(-.5,0),"ordinary jump toward lip",s);
    bool caught=false;
    for(int i=0;i<180;++i) {tick(s);if(s.snapshot().traversal_state==TraversalState::Hanging){caught=true;break;}}
    require(caught,"ordinary airborne lip catch",s);hanging(s);
    std::cout<<"CATCH tick="<<s.snapshot().tick_index<<" z="<<s.snapshot().player_position.z<<'\n';
    neutral(s,36);
    const auto blocked=s.snapshot();require(s.request_jump(),"blocked topout request",s);
    for(int i=0;i<45;++i){tick(s);hanging(s);}
    require(s.snapshot().foot_transfer_count==blocked.foot_transfer_count&&
            s.snapshot().accepted_traversal_count==blocked.accepted_traversal_count,
            "canopy rejects topout without completion or foot",s);
    Observation observed;
    require(s.set_move_input(0,-1),"northward shimmy",s);
    const auto start=s.snapshot();
    for(int i=0;i<90;++i){const auto before=s.snapshot();tick(s);observed.observe(before,s,-1);}
    require(s.snapshot().player_position.z<start.player_position.z-.15,"first shimmy makes physical progress",s);
    neutral(s,36);
    require(s.set_move_input(0,1),"ordinary direction reversal",s);
    const double reversal_start=s.snapshot().player_position.z;
    for(int i=0;i<54;++i){const auto before=s.snapshot();tick(s);observed.observe(before,s,1);}
    require(s.snapshot().player_position.z>reversal_start+.08,"reverse input makes physical progress",s);
    // Keep the same ordinary reversal held into the existing south end of the
    // continuous lip. Invalid lead space must leave both real grips planted.
    for(int i=0;i<270;++i){const auto before=s.snapshot();tick(s);observed.observe(before,s,1);}
    const auto edge=s.snapshot();
    require(edge.player_position.z>-181.0&&edge.player_position.z<-180.0,
            "reversal reaches the authored south lip end",s);
    for(int i=0;i<36;++i){const auto before=s.snapshot();tick(s);observed.observe(before,s,1);
        const auto v=s.snapshot();
        require(v.traversal_left_hand_generation==edge.traversal_left_hand_generation&&
                v.traversal_right_hand_generation==edge.traversal_right_hand_generation&&
                distance(local(s,v.traversal_left_hand),local(s,edge.traversal_left_hand))<1e-5&&
                distance(local(s,v.traversal_right_hand),local(s,edge.traversal_right_hand))<1e-5&&
                v.foot_transfer_count==edge.foot_transfer_count,
                "invalid lead beyond real lip cannot fabricate grip or completion",s);}
    std::cout<<"BLOCKED_EDGE tick="<<s.snapshot().tick_index<<" z="<<s.snapshot().player_position.z<<'\n';
    neutral(s,36);
    require(s.set_move_input(0,-1),"continue to clear pocket",s);
    bool clear=false;
    for(int i=0;i<450;++i){const auto before=s.snapshot();tick(s);observed.observe(before,s,-1);
        if(s.snapshot().player_position.z<-182.0){clear=true;break;}}
    require(clear,"shimmy reaches existing clear pocket",s);
    std::cout<<"ALTERNATION both="<<observed.both<<" left="<<observed.left<<" right="<<observed.right
        <<" cadence="<<observed.cadence<<" planted="<<observed.planted<<" trailing="<<observed.trailing<<'\n';
    if(!baseline)require(observed.both==0&&observed.left>=2&&observed.right>=2&&observed.cadence&&
                         observed.planted&&observed.trailing,"one trailing hand changes per >=18 ticks; planted hand stays",s);
    require(s.set_move_input(0,0),"stop lateral input in clear pocket",s);
    const auto transfer=s.snapshot();require(s.request_jump(),"ordinary clear-pocket topout",s);
    bool finished=false;
    for(int i=0;i<153;++i){tick(s);const auto v=s.snapshot();
        require(v.traversal_state!=TraversalState::Mantling,"topout does not use legacy path driver",s);
        if(v.player_grounded&&v.support_entity_id==1901&&v.traversal_state==TraversalState::None&&
            v.traversal_hand_constraint_count==0&&std::abs(v.player_position.y-106.9)<.12){finished=true;break;}}
    const auto end=s.snapshot();
    require(finished&&end.foot_transfer_count==transfer.foot_transfer_count+1&&
            end.foot_transfer_support_entity_id==1901&&end.foot_transfer_peak_hand_load_n>0&&
            end.traversal_actuator_positive_work_j>transfer.traversal_actuator_positive_work_j,
            "clear pocket transfers finite hand work into real high-shelf footing",s);
    for(int i=0;i<90;++i){tick(s);require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==1901&&
        std::abs(s.snapshot().player_position.y-106.9)<.12,"firm actual high-shelf exit",s);}
    if(baseline)require(observed.both==0,"baseline simultaneous hand replacement rejected",s);
    std::cout<<"PASS SHIMMY ordinary_jump_canopy_reversal_clear_topout firm90=1 tick="<<s.snapshot().tick_index<<'\n';
}
