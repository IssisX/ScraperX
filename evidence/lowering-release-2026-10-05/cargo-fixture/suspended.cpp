// AS-024: removal/unreachable holds/blocked top-out must reject this route.
#include "sim/simulation.hpp"
#include "sim/cargo_net.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
using scraperx::sim::Simulation;
namespace {
void require(bool ok, const char *message) {
    if (!ok) { std::cerr << "FAIL AS-024 " << message << '\n'; std::exit(1); }
}
void tick(Simulation &s) { require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "native fixed tick accepted"); }
void settle(Simulation &s, int n=45) { (void)s.set_move_input(0,0); while(n-- > 0) tick(s); }
void walk(Simulation &s, double x, double z, int limit=2700) {
    for (int i=0; i<limit; ++i) {
        const auto p=s.snapshot().player_position;
        const double dx=x-p.x, dz=z-p.z, d=std::hypot(dx,dz);
        if (d < 0.12) { settle(s); return; }
        const double scale=std::min(1.0,d/0.5)/d;
        (void)s.set_move_input(dx*scale,dz*scale); (void)s.set_facing(dx/d,dz/d); tick(s);
    }
    auto p=s.snapshot().player_position;
    std::cerr << "stalled walk at " << p.x << ',' << p.y << ',' << p.z << '\n';
    require(false,"ordinary walking reaches the next route point");
}
void route(double lane) {
    Simulation s;
    const auto gantry=s.kit_body_index(2952);
    require(gantry!=Simulation::kKitNone && s.kit_body_dynamic(gantry) && s.kit_body_mass(gantry)>1000,
            "gameplay gantry and receiver have finite native mass");
    settle(s);
    walk(s,lane,-114.0);
    (void)s.set_facing(0,-1);
    // Stop at the real CLIMB offer rather than driving through the mesh;
    // walking into holds while airborne can legitimately auto-grab them.
    (void)s.set_move_input(0,-0.25);
    for (int i=0; i<360 && !(s.snapshot().grip_available && s.snapshot().grip_entity_id==scraperx::sim::CargoNet::kEntity); ++i) tick(s);
    (void)s.set_move_input(0,0);
    require(s.snapshot().player_grounded && s.snapshot().player_position.y < 1.1,"entry is reached on grade without restart/teleport");
    require(s.snapshot().grip_available,"cargo mesh offers a real reachable native grip");
    (void)s.request_traversal(); tick(s);
    require((s.snapshot().traversal_state==scraperx::sim::TraversalState::Climbing),"Action takes the mesh through existing climbing authority");
    require(s.snapshot().player_gravity_factor==1.0,"soft hands retain actual rider gravity");
    require(s.snapshot().traversal_hand_constraint_count==2,"net climb has two finite physical hand attachments");

    (void)s.set_move_input(0,-1);
    for(int i=0;i<1200 && s.snapshot().player_position.y<4.5;++i) tick(s);
    (void)s.set_move_input(0,0);
    const auto start=s.snapshot();
    const auto mesh=s.cargo_net_vertices();
    unsigned knot=0;double distance=1e99;
    for(unsigned i=0;i<mesh.size();++i) {
        const auto p=mesh[i];const auto h=start.traversal_left_hand;
        double d=std::hypot(std::hypot(p.x-h.x,p.y-h.y),p.z-h.z);
        if(d<distance){distance=d;knot=i;}
    }
    std::cout.precision(12);
    std::cout<<"SUSPENDED lane="<<lane<<" y="<<start.player_position.y<<" grounded="<<start.player_grounded
        <<" hands="<<start.traversal_hand_constraint_count<<" state="<<int(start.traversal_state)
        <<" knot="<<knot<<" point="<<mesh[knot].x<<','<<mesh[knot].y<<','<<mesh[knot].z<<std::endl;
    double sum=0;
    for(int i=0;i<180;++i) {
        tick(s);auto st=s.snapshot();sum+=st.traversal_left_hand_force.y+st.traversal_right_hand_force.y;
        if(i==44 || i==89 || i==179) {
            const auto v=s.cargo_net_vertices()[knot];
            std::cout<<"HOLD tick="<<i+1<<" y="<<st.player_position.y<<" vy="<<st.player_linear_velocity.y
                <<" grounded="<<st.player_grounded<<" force_y="<<st.traversal_left_hand_force.y+st.traversal_right_hand_force.y
                <<" mean_force_y="<<sum/(i+1)<<" knot_dy="<<v.y-mesh[knot].y<<" knot_dz="<<v.z-mesh[knot].z
                <<" grip_dz="<<st.traversal_left_hand.z-start.traversal_left_hand.z
                <<" hands="<<st.traversal_hand_constraint_count<<std::endl;
        }
    }
    (void)s.request_release();tick(s);
    std::cout<<"RELEASE hands="<<s.snapshot().traversal_hand_constraint_count<<" force_y="
        <<s.snapshot().traversal_left_hand_force.y+s.snapshot().traversal_right_hand_force.y<<std::endl;
}
}
int main(int argc,char **argv){route(argc>1?std::atof(argv[1]):19);}
