#include "sim/simulation.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace scraperx::sim;
namespace {
double length(Vector3 a){return std::hypot(a.x,a.y,a.z);}
// Native world points and poses are floats. At this fixture's |Z|=118 m,
// four float ULPs allow the measured 12 micrometer inverse-pose round trip.
constexpr double kMaterialAnchorTolerance=4.0/131072.0;
Vector3 sub(Vector3 a,Vector3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vector3 local(const Simulation&s,Vector3 point) {
    const auto body=s.kit_body_index(2952);const auto p=sub(point,s.kit_body_position(body));
    const auto q=s.kit_body_rotation(body);const Vector3 u{-q.x,-q.y,-q.z};
    const Vector3 t{2*(u.y*p.z-u.z*p.y),2*(u.z*p.x-u.x*p.z),2*(u.x*p.y-u.y*p.x)};
    return {p.x+q.w*t.x+u.y*t.z-u.z*t.y,p.y+q.w*t.y+u.z*t.x-u.x*t.z,
            p.z+q.w*t.z+u.x*t.y-u.y*t.x};
}
void require(bool ok,const char*why,const Simulation&s) {
    if(ok)return;
    const auto v=s.snapshot();
    std::cerr<<"FAIL SECOND_HAND "<<why<<" tick="<<v.tick_index<<" P="<<v.player_position.x<<','
        <<v.player_position.y<<','<<v.player_position.z<<" state="<<int(v.traversal_state)
        <<" hands="<<v.traversal_hand_constraint_count<<" gen="<<v.traversal_left_hand_generation<<','
        <<v.traversal_right_hand_generation<<" available="<<v.missing_hand_grip_available
        <<" radial="<<v.hanging_free_arc_active<<" foot="<<v.foot_push_active
        <<" work="<<v.traversal_actuator_positive_work_j<<'\n';std::exit(EXIT_FAILURE);
}
void tick(Simulation&s) {
    const auto before=s.snapshot().tick_index;const auto r=s.advance_frame(Simulation::kFixedStepSeconds);
    require(r.accepted&&r.steps_advanced==1&&s.snapshot().tick_index==before+1,"one native tick",s);
    const auto v=s.snapshot();require(v.player_gravity_factor==1&&v.death_count==0,"gravity on without recovery",s);
}
void no_other_support(const Simulation&s) {
    const auto v=s.snapshot();require(!v.player_grounded&&!v.foot_push_active&&v.foot_transfer_count==0&&
        v.traversal_plan_kind==0&&!v.hand_foot_overlap_active,"no fabricated footing or cargo transfer",s);
    require(length(v.traversal_left_hand_force)<=1500.05&&length(v.traversal_right_hand_force)<=1500.05,
            "existing finite per-hand solved forces",s);
}
void recover(Simulation&s,bool recovery_direction=false) {
    for(int i=0;i<180;++i)tick(s);
    require(s.debug_restart_at({16.85,11.50,-118})&&s.set_facing(1,0)&&s.set_move_input(0,0)&&
            s.set_crouch_input(false),"single existing shipping approach staging",s);tick(s);
    require(s.snapshot().grip_available&&s.snapshot().grip_entity_id==2952&&!s.snapshot().player_grounded,
            "actual visible airborne2952 grip",s);
    require(s.request_traversal(),"ordinary initial Action",s);tick(s);const auto entry=s.snapshot();
    require(entry.tick_index==182&&entry.traversal_hand_constraint_count==1&&entry.hanging_free_arc_active&&
            entry.traversal_left_hand_generation>0&&entry.traversal_right_hand_generation==0&&
            !entry.missing_hand_grip_available,"real solo catch with blocked missing-hand offer",s);
    const auto planted=local(s,entry.traversal_left_hand);
    require(s.request_traversal(),"fresh immediate blocked Action",s);tick(s);
    require(s.snapshot().rejected_traversal_count==entry.rejected_traversal_count+1,
            "immediate occluded missing-hand request rejects",s);
    for(;;) {
        const auto v=s.snapshot();no_other_support(s);
        require(v.traversal_hand_constraint_count==1&&v.hanging_free_arc_active&&
                v.traversal_left_hand_generation==entry.traversal_left_hand_generation&&
                v.traversal_right_hand_generation==0&&length(v.traversal_right_hand)==0&&
                length(v.traversal_right_hand_force)==0&&length(sub(local(s,v.traversal_left_hand),planted))<kMaterialAnchorTolerance,
                "blocked request and neutral history retain planted material identity without auto acquisition",s);
        require(v.traversal_actuator_positive_work_j==entry.traversal_actuator_positive_work_j&&
                v.traversal_command_work_bound_j==entry.traversal_command_work_bound_j,
                "neutral passive solo history adds no actuator work",s);
        if(v.tick_index==230)require(!v.missing_hand_grip_available,"near-coincident30mm candidate is not a second palm",s);
        if(v.tick_index==259)break;
        tick(s);
    }
    const auto before=s.snapshot();const auto before_local=local(s,before.traversal_left_hand);
    require(before.missing_hand_grip_available,"real visible distinct missing-hand offer before Action260",s);
    if(recovery_direction)require(s.set_move_input(1,0),"direction queued with recovery Action",s);
    require(s.request_traversal(),"fresh ordinary recovery Action260",s);tick(s);const auto pair=s.snapshot();
    require(pair.tick_index==260&&pair.traversal_hand_constraint_count==2&&pair.hanging_free_arc_active&&
            pair.traversal_support_entity_id==2952&&!pair.missing_hand_grip_available&&
            pair.traversal_left_hand_generation==before.traversal_left_hand_generation&&
            pair.traversal_right_hand_generation==before.traversal_right_hand_generation+1&&
            length(sub(local(s,pair.traversal_left_hand),before_local))<kMaterialAnchorTolerance,
            "fresh Action adds only missing rigid hand and retains planted anchor through the solve",s);
    require(length(sub(pair.traversal_left_hand,pair.traversal_right_hand))>=.082-1.e-5&&
            length(sub(pair.traversal_right_hand,pair.player_position))<=1.75,
            "actual new palm is distinct and within existing radial reach",s);
    require(pair.rejected_traversal_count==before.rejected_traversal_count&&
            pair.traversal_actuator_positive_work_j==before.traversal_actuator_positive_work_j&&
            pair.traversal_command_work_bound_j==before.traversal_command_work_bound_j&&
            length(sub(pair.player_linear_velocity,before.player_linear_velocity))<.6,
            "recovery neither rejects, refunds work nor grants an unbounded velocity change",s);
    no_other_support(s);
    if(recovery_direction)require(s.set_move_input(0,0),"neutral immediately after recovery transaction",s);
    tick(s);no_other_support(s);
    require(s.snapshot().traversal_hand_constraint_count==2&&s.snapshot().hanging_free_arc_active,
            "neutral recovered pair remains radial through the next native solve",s);
    std::cout<<"RECOVERED tick=260 separation="<<length(sub(pair.traversal_left_hand,pair.traversal_right_hand))
        <<" generation="<<pair.traversal_left_hand_generation<<','<<pair.traversal_right_hand_generation
        <<" recovery_direction="<<recovery_direction<<'\n';
}
void paid_controls_and_release() {
    Simulation s;recover(s);require(s.set_crouch_input(true),"ordinary paired tuck",s);
    const auto start=s.snapshot();
    for(int i=0;i<30;++i) {
        const auto before=s.snapshot();tick(s);const auto after=s.snapshot();no_other_support(s);
        require(after.traversal_hand_constraint_count==2&&after.hanging_free_arc_active&&
                after.traversal_actuator_positive_work_j>=before.traversal_actuator_positive_work_j&&
                after.traversal_actuator_positive_work_j-before.traversal_actuator_positive_work_j<=
                    500*Simulation::kFixedStepSeconds+.00001&&
                std::abs(after.hanging_commanded_span_m-before.hanging_commanded_span_m)<=
                    .6*Simulation::kFixedStepSeconds+.00001,
                "recovered pair shares existing500W and0.6m/s radial posture budget",s);
    }
    require(s.snapshot().traversal_actuator_positive_work_j>start.traversal_actuator_positive_work_j&&
            s.snapshot().hanging_commanded_span_m<start.hanging_commanded_span_m-.01,
            "paired tuck makes actual paid target progress",s);
    require(s.set_crouch_input(false)&&s.set_move_input(1,0),"later ordinary paired directional control",s);tick(s);
    require(s.snapshot().traversal_hand_constraint_count==2&&!s.snapshot().hanging_free_arc_active,
            "later paired movement resumes normal Cartesian climb authority",s);
    require(s.set_move_input(0,0)&&s.request_release(),"ordinary paired Drop",s);tick(s);
    for(int i=0;i<15;++i){require(s.snapshot().traversal_hand_constraint_count==0&&
        !s.snapshot().hanging_free_arc_active&&!s.snapshot().missing_hand_grip_available&&!s.snapshot().foot_push_active,
        "Drop clears both hands and recovery offer without automatic reattachment",s);tick(s);}
}
void paired_restart() {
    Simulation s;recover(s,true);require(s.restart_checkpoint(),"ordinary checkpoint restart with recovered pair",s);
    require(s.snapshot().traversal_hand_constraint_count==0&&!s.snapshot().hanging_free_arc_active&&
        !s.snapshot().missing_hand_grip_available&&!s.snapshot().foot_push_active,
        "restart clears actual recovered pair and transient offer",s);
    for(int i=0;i<10;++i){tick(s);require(s.snapshot().traversal_hand_constraint_count==0,
        "restart never revives missing-hand attachment",s);}
}
}
int main(int argc,char**argv){
    if(argc>1&&std::string(argv[1])=="direction")paired_restart();
    else {paid_controls_and_release();paired_restart();}
    std::cout<<"PASS SECOND_HAND publicAction260 paired_controls Drop restart\n";
}
