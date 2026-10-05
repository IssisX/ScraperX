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
    const double before=s.snapshot().player_position.y;
    const double grip_z=s.snapshot().traversal_left_hand.z;
    settle(s,90);
    require(s.snapshot().player_position.y-before<0.06,"no held motion cannot climb upward; physical sag is allowed");
    require(std::abs(s.snapshot().traversal_left_hand.z-grip_z)>0.015,"climber load physically deflects the net and its real grip");
    if(lane==20.0) {
        // A retained material coordinate is linear in the SAME two mesh
        // histories as the rendered vertices. This exercises the production
        // per-hand sampler without allocating a whole mesh in that sampler.
        int retained_samples=0;
        for(int sample=0;sample<12;++sample) {
            const auto before=s.snapshot();tick(s);const auto after=s.snapshot();
            require(s.advance_frame(Simulation::kFixedStepSeconds*.5).steps_advanced==0,"net half-tick is render only");
            for(bool left:{false,true}) {
                const auto a=left?before.traversal_left_hand:before.traversal_right_hand;
                const auto b=left?after.traversal_left_hand:after.traversal_right_hand;
                const auto rendered=s.render_traversal_hand(left);
                const auto gap=std::hypot(std::hypot(b.x-a.x,b.y-a.y),b.z-a.z);
                if(gap<.01) {
                    require(std::hypot(std::hypot(rendered.x-(a.x+b.x)*.5,rendered.y-(a.y+b.y)*.5),rendered.z-(a.z+b.z)*.5)<.00002,
                            "retained net material anchor uses the mesh half-tick history");
                    ++retained_samples;
                }
                // A changed material coordinate has different endpoints;
                // a world-point midpoint does not describe its mesh binding.
            }
            require(s.snapshot().tick_index==after.tick_index && s.snapshot().player_position.y==after.player_position.y,
                    "net render reads do not advance native player state");
            require(s.advance_frame(Simulation::kFixedStepSeconds*.5).steps_advanced==1,"finish net render half-tick");
        }
        require(retained_samples>=12,"net render check actually exercises retained mesh holds");
    }
    (void)s.set_move_input(0,-1);
    int climb_ticks=0;
    bool paused=false, transfer_action_exercised=false;
    for (int i=0; i<1800; ++i) {
        const auto previous=s.snapshot();
        if (!transfer_action_exercised && previous.traversal_support_entity_id==2952 &&
            previous.traversal_state==scraperx::sim::TraversalState::Climbing) {
            (void)s.request_traversal();
            transfer_action_exercised=true;
        }
        tick(s);
        require(s.snapshot().player_gravity_factor==1.0,"entire cargo climb and receiver transfer retain gravity");
        if(lane==20.0 && !paused && s.snapshot().player_position.y>5.5) { settle(s,27); (void)s.set_move_input(0,-1); paused=true; }
        if((s.snapshot().traversal_state==scraperx::sim::TraversalState::Climbing)) ++climb_ticks;
        if (s.snapshot().player_grounded && s.snapshot().player_position.y > 11.7) break;
    }
    require(transfer_action_exercised,"Action regression reached the actual receiving transfer");
    settle(s);
    auto p=s.snapshot();
    std::cout << "CARGO_TOP lane=" << lane << " position=" << p.player_position.x << ',' << p.player_position.y << ',' << p.player_position.z << " grounded=" << p.player_grounded << " climb_ticks=" << climb_ticks << " deaths=" << p.death_count << '\n';
    if(!p.player_grounded) {
        const auto vertices=s.cargo_net_vertices();
        for(int r=0;r<23;r+=3) { auto v=vertices[4*(r*9+4)];std::cout << "NET_DIAG row=" << r << " point=" << v.x << ',' << v.y << ',' << v.z << '\n'; }
        std::cout << "HAND_FORCE left=" << p.traversal_left_hand_force.y << "," << p.traversal_left_hand_force.z << " right=" << p.traversal_right_hand_force.y << "," << p.traversal_right_hand_force.z << " target=" << p.traversal_target_point.y << "," << p.traversal_target_point.z << " command_bound=" << p.traversal_command_work_bound_j << " gravity=" << p.player_gravity_factor << " attachments=" << p.traversal_hand_constraint_count << "\n";
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
