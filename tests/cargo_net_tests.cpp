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
    const double before=s.snapshot().player_position.y;
    const double grip_z=s.snapshot().traversal_left_hand.z;
    settle(s,90);
    require(s.snapshot().player_position.y-before<0.06,"no held motion cannot climb upward; physical sag is allowed");
    require(std::abs(s.snapshot().traversal_left_hand.z-grip_z)>0.015,"climber load physically deflects the net and its real grip");
    (void)s.set_move_input(0,-1);
    int climb_ticks=0;
    bool paused=false;
    for (int i=0; i<1800; ++i) {
        tick(s);
        if(lane==20.0 && !paused && s.snapshot().player_position.y>5.5) { settle(s,27); (void)s.set_move_input(0,-1); paused=true; }
        if((s.snapshot().traversal_state==scraperx::sim::TraversalState::Climbing)) ++climb_ticks;
        if (s.snapshot().player_grounded && s.snapshot().player_position.y > 11.7) break;
    }
    settle(s);
    auto p=s.snapshot();
    std::cout << "CARGO_TOP lane=" << lane << " position=" << p.player_position.x << ',' << p.player_position.y << ',' << p.player_position.z << " grounded=" << p.player_grounded << " climb_ticks=" << climb_ticks << " deaths=" << p.death_count << '\n';
    if(!p.player_grounded) {
        const auto vertices=s.cargo_net_vertices();
        for(int r=0;r<23;r+=3) { auto v=vertices[4*(r*9+4)];std::cout << "NET_DIAG row=" << r << " point=" << v.x << ',' << v.y << ',' << v.z << '\n'; }
        std::cout << "HANDS " << p.traversal_left_hand.y << ',' << p.traversal_left_hand.z << " / " << p.traversal_right_hand.y << ',' << p.traversal_right_hand.z << '\n';
    }
    require(p.player_grounded && !(p.traversal_state==scraperx::sim::TraversalState::Climbing) && p.player_position.y > 11.7 && p.player_position.y < 12.1,"held forward tops out onto real +11 m support");
    require(climb_ticks > 700 && p.climb_count > 0 && p.death_count==0,"ordinary hand-over-hand ascent has no launch or death");
    walk(s,18.5,-120.5); walk(s,18.5,-126.0);
    p=s.snapshot();
    require(p.player_grounded && p.support_entity_id==Simulation::kTowerEntityId && std::abs(p.player_position.y-11.9)<0.15,"wide receiver connects by walking to existing first tower ring");
    require(p.death_count==0 && s.slingshot_state().work_j==0,"slingshot is unused throughout alternative route");
    std::cout << "PASS AS-024 grade_to_first_ring lane=" << lane << " supported_height_m=11 deaths=0\n";
}
}
int main() { route(19.0); route(20.0); route(21.0); }
