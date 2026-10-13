#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;
namespace {
void require(bool ok,const char *message) {
    if(!ok){std::cerr<<"FAIL catch player "<<message<<'\n';std::exit(1);}
}
void tick(Simulation &s){require(s.advance_frame(Simulation::kFixedStepSeconds).accepted,"fixed tick accepted");}
void mesh_equal(const std::vector<Vector3> &a,const std::vector<Vector3> &b) {
    require(a.size()==b.size(),"restored mesh retains vertex identity");
    for(std::size_t i=0;i<a.size();++i)
        require(std::hypot(std::hypot(a[i].x-b[i].x,a[i].y-b[i].y),a[i].z-b[i].z)<.00001,
                "checkpoint restores actual mesh and interpolation history");
}
void cleared(const Simulation &s) {
    const auto a=s.snapshot();const auto n=s.catch_net_state();
    require(!a.player_grounded&&a.support_entity_id==0&&a.soft_side_contact_entity==0&&!a.checkpoint_footing_valid,
            "restore clears support and firm-footing observation");
    require(!n.current_contact&&!n.side_contact&&n.contact_serial==0&&n.peak_total_specific_load_mps2==0&&
            n.excess_load_dose_g_seconds==0&&!n.overloaded,"restore clears contact/load episode");
}
void trace(const Simulation &s,int i) {
    const auto a=s.snapshot();const auto n=s.catch_net_state();
    std::cout<<"TRACE tick="<<i<<" x="<<a.player_position.x<<" y="<<a.player_position.y
        <<" z="<<a.player_position.z<<" vx="<<a.player_linear_velocity.x<<" vy="<<a.player_linear_velocity.y
        <<" vz="<<a.player_linear_velocity.z<<" grounded="<<a.player_grounded<<" firm="<<a.checkpoint_footing_valid
        <<" support_vx="<<a.support_point_linear_velocity.x<<" support_vy="<<a.support_point_linear_velocity.y
        <<" support_vz="<<a.support_point_linear_velocity.z<<" support="<<a.support_entity_id<<" contact="<<n.current_contact<<" serial="<<n.contact_serial
        <<" side="<<n.side_contact<<" min_y="<<n.minimum_y_m<<" load_g="<<n.total_specific_load_mps2/9.81
        <<" peak_g="<<n.peak_total_specific_load_mps2/9.81<<" dose="<<n.excess_load_dose_g_seconds
        <<" rigid_speed="<<a.last_impact_speed_mps<<" foot_push_force_x="<<a.foot_push_player_force.x
        <<" foot_push_force_y="<<a.foot_push_player_force.y<<" foot_push_force_z="<<a.foot_push_player_force.z
        <<" recovery_work_j="<<a.landing_recovery_work_j<<" checkpoint_commits="<<a.checkpoint_commit_count
        <<" deaths="<<a.death_count<<'\n';
}
void require_state(bool ok,const Simulation &s,int tick_number,const char *message) {
    if(!ok) {
        std::cerr<<"FIRST_FAILURE catch player "<<message<<'\n';
        trace(s,tick_number);
        std::cout.flush();
    }
    require(ok,message);
}
void exit_to_grade(Simulation &s) {
    bool rim=false,rigid_checkpoint=false;
    (void)s.set_facing(-1,0);
    for(int i=0;i<1400;++i) {
        auto a=s.snapshot();
        const double target_x=rigid_checkpoint?-47.0:-40.3;
        const double dx=target_x-a.player_position.x, dz=-150-a.player_position.z;
        const double distance=std::hypot(dx,dz);
        const double speed=rigid_checkpoint?.20:.35*std::min(1.0,distance/.6);
        if(distance>.07&&a.traversal_state==scraperx::sim::TraversalState::None)
            (void)s.set_move_input(speed*dx/distance,speed*dz/distance);
        else (void)s.set_move_input(0,0);
        if(a.player_position.x < -46.7) (void)s.set_move_input(0,0);
        if(a.ledge_available&&a.ledge_entity_id==2963&&a.traversal_state==scraperx::sim::TraversalState::None&&
            a.support_entity_id!=2963)
            (void)s.request_traversal();
        tick(s);a=s.snapshot();
        require_state(a.death_count==0,s,i,"ordinary rigid exit remains survivable");
        rim=rim||a.support_entity_id==2963;
        rigid_checkpoint=rigid_checkpoint||(a.support_entity_id==2963&&a.checkpoint_footing_valid);
        if(i%90==0) trace(s,i);
        if(a.player_position.x < -46.7&&a.support_entity_id==Simulation::kStaticDeckEntityId&&a.checkpoint_footing_valid) {
            require_state(rim&&rigid_checkpoint,s,i,"exit transfers to real rim/tread and commits rigid footing");
            std::cout<<"PASS catch controlled rigid exit/grade\n";return;
        }
    }
    trace(s,1400);require(false,"ordinary controls reach rim, treads and grade");
}
void neutral(double height,bool exit) {
    Simulation s;
    require(s.catch_net_state().installed&&!s.catch_net_vertices().empty(),"shipping world installs actual catch mesh");
    require(s.debug_restart_at({-34,height,-150}),"initial airborne physical fixture is clear");
    const auto commits=s.snapshot().checkpoint_commit_count;
    bool contact=false,rebound=false,separated=false,recontact=false;int stable=0;
    double minimum=4,peak_up=0,peak_g=0,dose=0;
    for(int i=0;i<2700;++i) {
        tick(s);const auto a=s.snapshot();const auto n=s.catch_net_state();
        if(i%90==0||a.death_count)trace(s,i);
        require_state(a.death_count==0,s,i,"neutral fall survives compliant catch");
        require_state(a.checkpoint_commit_count==commits,s,i,"cloth never commits firm-footing checkpoint");
        if(a.support_entity_id==1963){if(separated)recontact=true;contact=true;}
        if(contact&&a.player_linear_velocity.y>2)rebound=true;
        if(rebound&&!a.player_grounded)separated=true;
        minimum=std::min(minimum,n.minimum_y_m);peak_up=std::max(peak_up,a.player_linear_velocity.y);
        peak_g=std::max(peak_g,n.peak_total_specific_load_mps2/9.81);dose=std::max(dose,n.excess_load_dose_g_seconds);
        const auto v=a.player_linear_velocity, u=a.support_point_linear_velocity;
        if(recontact&&a.support_entity_id==1963&&std::hypot(std::hypot(v.x-u.x,v.y-u.y),v.z-u.z)<.15)++stable;
        else stable=0;
        if(stable>=90) {
            require(contact&&rebound&&separated&&recontact,"neutral rebound separates, recontacts and settles");
            require(minimum<3.0&&minimum>.4,"loaded mesh deforms with real ground clearance");
            require(!a.checkpoint_footing_valid,"moving cloth cannot be firm checkpoint footing");
            std::cout<<"PASS catch neutral height="<<height<<" min_y="<<minimum<<" rebound_mps="<<peak_up
                <<" peak_total_g="<<peak_g<<" dose_g_seconds="<<dose<<" settled_tick="<<i<<'\n';
            if(exit)exit_to_grade(s);
            return;
        }
    }
    trace(s,2700);require(false,"neutral rider settles on material within 30s");
}
void frame_jump() {
    Simulation s;
    const auto frame=s.kit_body_index(2963);
    require(s.kit_body_dynamic(frame)&&s.kit_body_mass(frame)>0,"real finite frame is a dynamic receiver");
    require(s.debug_restart_at({-40.30,5.2,-150}),"frame fixture begins clear above actual deck");
    for(int i=0;i<180&&s.snapshot().support_entity_id!=2963;++i) tick(s);
    const auto before=s.snapshot();
    require(before.player_grounded&&before.support_entity_id==2963,"Jump begins on actual rigid frame");
    require(s.request_jump(),"ordinary frame Jump is accepted");tick(s);
    const auto after=s.snapshot();
    const double work=after.landing_jump_work_j-before.landing_jump_work_j;
    require(after.death_count==0&&!after.player_grounded&&after.player_linear_velocity.y>4,
            "finite frame Jump departs under ordinary controls");
    require(work>0&&work<=.5*85*5.5*5.5+.01,"frame Jump bounds positive work including receiver point inertia");
    std::cout<<"PASS catch finite rigid frame Jump work_j="<<work<<'\n';
}
void exit_probe() {
    Simulation s;require(s.debug_restart_at({-34,5.2,-150}),"low exit fixture is clear");
    bool contact=false;
    for(int i=0;i<600;++i) {tick(s);const auto a=s.snapshot();
        if(i>300&&a.support_entity_id==1963){contact=true;break;}}
    require(contact,"exit begins on actual caught material");
    exit_to_grade(s);
}
void fatal_fixture(const char *label,Vector3 start,bool overload) {
    Simulation s;require(s.debug_restart_at(start),"fatal physical fixture is clear");
    const auto cargo=s.cargo_net_vertices(),catch_mesh=s.catch_net_vertices();
    const auto frame=s.kit_body_index(2963);const auto pose=s.kit_body_position(frame);
    bool touched=false;double peak_g=0;
    for(int i=0;i<720;++i) {
        tick(s);const auto a=s.snapshot();const auto n=s.catch_net_state();
        touched=touched||n.current_contact;peak_g=std::max(peak_g,n.peak_total_specific_load_mps2/9.81);
        if(i%90==0||a.death_count)trace(s,i);
        if(a.death_count) {
            require(a.death_count==1,"one actual fatal event restores once");
            if(overload)require(touched&&a.last_impact_speed_mps<20&&peak_g>20,
                "overload consequence comes from finite net load policy before any lethal rigid strike");
            else require(a.last_impact_speed_mps>20,"real rigid strike retains lethal impact consequence");
            cleared(s);mesh_equal(cargo,s.cargo_net_vertices());mesh_equal(catch_mesh,s.catch_net_vertices());
            mesh_equal(cargo,s.cargo_net_vertices(0));mesh_equal(catch_mesh,s.catch_net_vertices(0));
            const auto restored=s.kit_body_position(frame);
            require(std::hypot(std::hypot(restored.x-pose.x,restored.y-pose.y),restored.z-pose.z)<.00001,
                "death restores actual finite frame before mesh");
            std::cout<<"PASS catch "<<label<<" death/restore peak_total_g="<<peak_g<<'\n';return;
        }
    }
    require(false,"fatal fixture resolves consequence");
}
void bottom_out() {
    Simulation s;
    // The perimeter clip back is a real finite rigid member beneath the cloth.
    // This physical drop acquires soft footing immediately before bottoming
    // onto that obstruction; it does not assert interior grade bottom-out.
    require(s.debug_restart_at({-39.65,36,-150}),"clip bottom-out fixture starts clear above real geometry");
    for(int i=0;i<360;++i) {
        const auto before=s.snapshot();const auto prior=s.catch_net_state();
        tick(s);const auto a=s.snapshot();
        if(a.death_count) {
            require(before.support_entity_id==1963&&prior.current_contact&&
                    before.player_linear_velocity.y < -20&&!prior.overloaded,
                    "bottom-out rigid strike begins with actual current cloth support and lethal descent");
            require(a.death_count==1&&a.last_impact_speed_mps>20,
                    "actual rigid obstruction below cloth retains lethal consequence while soft supported");
            cleared(s);
            std::cout<<"PASS catch real clip bottom-out despite current soft support rigid_mps="
                <<a.last_impact_speed_mps<<'\n';return;
        }
    }
    require(false,"physical clip bottom-out resolves actual rigid strike");
}
void spill() {
    // This first rebound must produce a ground strike above the existing
    // >20 m/s lethal threshold; the 36 m settling fixture lands below it.
    Simulation s;require(s.debug_restart_at({-34,40,-150}),"spill fall fixture is clear");
    bool contact=false,air_input=false,outside=false;
    for(int i=0;i<1000;++i) {
        const auto before=s.snapshot();
        contact=contact||before.support_entity_id==1963;
        if(contact&&before.player_linear_velocity.y>10&&!before.player_grounded) {
            (void)s.set_move_input(-1,0);air_input=true;
        }
        tick(s);const auto a=s.snapshot();
        outside=outside||a.player_position.x < -41;
        if(i%90==0||a.death_count)trace(s,i);
        if(a.death_count) {
            require(air_input&&outside&&a.last_impact_speed_mps>20,
                "ordinary airborne lateral intent spills first rebound into a fatal real ground landing");
            std::cout<<"PASS catch deliberate first-bounce spill remains lethal\n";return;
        }
    }
    trace(s,1000);require(false,"deliberate first rebound spill causes fatal ground impact");
}
void side() {
    Simulation s;
    // A real vertical cargo cloth supplies the shared native side-contact
    // predicate. Horizontal catch-cloth wrapping is not asserted by this case.
    require(s.debug_restart_at({20.25,5.0,-118.40}),"side fixture starts clear beside actual vertical cloth");
    (void)s.set_facing(0,-1);(void)s.set_move_input(0,1);
    for(int i=0;i<120;++i) {
        tick(s);const auto a=s.snapshot();
        require(a.death_count==0,"bounded side approach survives");
        if(a.soft_side_contact_entity==1953) {
            require(a.traversal_hand_constraint_count==0&&a.traversal_state==scraperx::sim::TraversalState::None,
                    "side fixture isolates capsule contact from legitimate hand grips");
            require(!a.player_grounded&&a.support_entity_id!=1953&&!a.checkpoint_footing_valid,
                    "actual capsule-cylinder cloth side cannot stand or be firm footing");
            const double work=a.landing_jump_work_j;
            require(s.request_jump(),"ordinary airborne Jump input is accepted");tick(s);
            const auto after=s.snapshot();
            require(after.landing_jump_work_j==work,"actual side-only contact supplies no reciprocal Jump receiver");
            std::cout<<"PASS actual vertical cloth side rejects shared standing/Jump/checkpoint authority\n";return;
        }
    }
    require(false,"side fixture must observe actual native side contact; empty geometry cannot pass");
}
void underside() {
    Simulation s;require(s.debug_restart_at({-34,36,-150}),"loaded underside setup is clear");
    bool loaded=false;
    for(int i=0;i<300;++i) {tick(s);if(s.catch_net_state().minimum_y_m<2.2){loaded=true;break;}}
    require(loaded,"overhead fixture uses a genuinely loaded falling mesh");
    // Stop loading while the 1.92 m capsule head still has initial clearance.
    // Initial physical fixture below the still-moving cloth. This relocation
    // supplies no gameplay aid; the real soft vertices and their velocity stay.
    require(s.debug_restart_at({-34,1.02,-150}),"capsule fits beneath the actual loaded mesh");
    bool head_contact=false;
    for(int i=0;i<90;++i) {
        const auto before=s.snapshot();
        if(!before.player_grounded)(void)s.request_jump();
        tick(s);const auto a=s.snapshot();const auto n=s.catch_net_state();
        head_contact=head_contact||n.total_specific_load_mps2>0;
        require(a.support_entity_id!=1963&&!n.current_contact,"underside contact cannot stand on cloth");
        if(!before.player_grounded)require(a.landing_jump_work_j==before.landing_jump_work_j,
            "airborne underside contact cannot provide a reciprocal Jump receiver");
        if(a.checkpoint_footing_valid)require(a.support_entity_id==Simulation::kStaticDeckEntityId,
            "only actual grade may commit underneath cloth");
        require(a.death_count==0,"bounded loaded underside fixture survives");
    }
    require(head_contact,"actual supplied soft/player contact is observed from underneath");
    std::cout<<"PASS catch actual underside has no standing/Jump/checkpoint authority\n";
}
void restart() {
    Simulation s;require(s.debug_restart_at({-34,36,-150}),"restart fixture is clear");
    const auto cargo=s.cargo_net_vertices(),catch_mesh=s.catch_net_vertices();
    bool deformed=false;
    for(int i=0;i<350;++i) {tick(s);if(s.catch_net_state().current_contact&&s.catch_net_state().minimum_y_m<2.5){deformed=true;break;}}
    require(deformed,"restart begins with actual loaded deformation");
    require(s.restart_checkpoint(),"ordinary menu restart accepted");
    require(s.snapshot().death_count==0,"menu restart is not a fatal event");
    cleared(s);mesh_equal(cargo,s.cargo_net_vertices());mesh_equal(catch_mesh,s.catch_net_vertices());
    mesh_equal(cargo,s.cargo_net_vertices(0));mesh_equal(catch_mesh,s.catch_net_vertices(0));
    std::cout<<"PASS catch menu restart restores both meshes and clears episode\n";
}
}
int main(int argc,char **argv) {
    std::cout.setf(std::ios::unitbuf);
    const std::string mode=argc>1?argv[1]:"all";
    if(mode=="all"||mode=="neutral"||mode=="exit")neutral(36,mode=="all"||mode=="exit");
    else if(mode=="12"||mode=="exit12")neutral(12,mode=="exit12");
    else if(mode=="28")neutral(28,false);
    else if(mode=="36")neutral(36,false);
    if(mode=="exit_probe")exit_probe();
    if(mode=="all"||mode=="frame")frame_jump();
    if(mode=="all"||mode=="miss")fatal_fixture("outside footprint miss",{-43,36,-153},false);
    if(mode=="all"||mode=="rim")fatal_fixture("real rigid rim strike",{-39.95,36,-150},false);
    if(mode=="all"||mode=="overload")fatal_fixture("overload",{-34,100,-150},true);
    if(mode=="all"||mode=="bottom_out")bottom_out();
    if(mode=="all"||mode=="spill")spill();
    if(mode=="all"||mode=="side")side();
    if(mode=="all"||mode=="underside")underside();
    if(mode=="all"||mode=="restart")restart();
}
